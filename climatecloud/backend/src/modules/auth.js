// ─────────────────────────────────────────────
// ClimateCloud – Authentication Module
// ─────────────────────────────────────────────

import bcrypt from 'bcrypt';
import { eq } from 'drizzle-orm';
import { z } from 'zod';
import { users, organizations } from '../db/schema.js';
import { createAuditLog } from './audit.js';

// ──────── Validation Schemas ────────

const registerSchema = z.object({
    email: z.string().email().max(320),
    password: z.string().min(8).max(128),
    firstName: z.string().min(1).max(100),
    lastName: z.string().min(1).max(100),
    orgName: z.string().min(2).max(255),
});

const loginSchema = z.object({
    email: z.string().email(),
    password: z.string().min(1),
});

// ──────── Helpers ────────

function slugify(text) {
    return text
        .toLowerCase()
        .replace(/[^a-z0-9]+/g, '-')
        .replace(/(^-|-$)/g, '');
}

// ──────── Route Plugin ────────

export default async function authRoutes(fastify, opts) {
    const { db } = fastify;

    // ── Register ──
    fastify.post('/auth/register', {
        schema: {
            body: {
                type: 'object',
                required: ['email', 'password', 'firstName', 'lastName', 'orgName'],
                properties: {
                    email: { type: 'string' },
                    password: { type: 'string' },
                    firstName: { type: 'string' },
                    lastName: { type: 'string' },
                    orgName: { type: 'string' },
                },
            },
        },
    }, async (request, reply) => {
        const validated = registerSchema.parse(request.body);
        const { email, password, firstName, lastName, orgName } = validated;

        // Check existing user
        const existing = await db.select().from(users).where(eq(users.email, email.toLowerCase())).limit(1);
        if (existing.length > 0) {
            return reply.status(409).send({ error: 'Email already registered' });
        }

        // Hash password
        const passwordHash = await bcrypt.hash(password, 12);

        // Create organization
        const slug = slugify(orgName) + '-' + Date.now().toString(36);
        const [org] = await db.insert(organizations).values({
            name: orgName,
            slug,
        }).returning();

        // Create user (owner role)
        const [user] = await db.insert(users).values({
            email: email.toLowerCase(),
            passwordHash,
            firstName,
            lastName,
            orgId: org.id,
            role: 'owner',
        }).returning();

        // Generate tokens
        const accessToken = fastify.jwt.sign(
            {
                sub: user.id,
                email: user.email,
                orgId: org.id,
                role: user.role,
            },
            { expiresIn: '15m' }
        );

        const refreshToken = fastify.jwt.sign(
            { sub: user.id, type: 'refresh' },
            { expiresIn: '7d' }
        );

        // Store refresh token hash
        const refreshHash = await bcrypt.hash(refreshToken, 10);
        await db.update(users).set({ refreshToken: refreshHash }).where(eq(users.id, user.id));

        // Audit log
        await createAuditLog(db, {
            orgId: org.id,
            userId: user.id,
            action: 'user.register',
            resourceType: 'user',
            resourceId: user.id,
            ipAddress: request.ip,
        });

        return reply.status(201).send({
            user: {
                id: user.id,
                email: user.email,
                firstName: user.firstName,
                lastName: user.lastName,
                role: user.role,
                orgId: org.id,
                orgName: org.name,
            },
            accessToken,
            refreshToken,
        });
    });

    // ── Login ──
    fastify.post('/auth/login', async (request, reply) => {
        const validated = loginSchema.parse(request.body);
        const { email, password } = validated;

        const [user] = await db
            .select()
            .from(users)
            .where(eq(users.email, email.toLowerCase()))
            .limit(1);

        if (!user) {
            return reply.status(401).send({ error: 'Invalid credentials' });
        }

        if (!user.isActive) {
            return reply.status(403).send({ error: 'Account is deactivated' });
        }

        const valid = await bcrypt.compare(password, user.passwordHash);
        if (!valid) {
            return reply.status(401).send({ error: 'Invalid credentials' });
        }

        // Fetch organization
        const [org] = await db.select().from(organizations).where(eq(organizations.id, user.orgId)).limit(1);

        // Generate tokens
        const accessToken = fastify.jwt.sign(
            {
                sub: user.id,
                email: user.email,
                orgId: user.orgId,
                role: user.role,
            },
            { expiresIn: '15m' }
        );

        const refreshToken = fastify.jwt.sign(
            { sub: user.id, type: 'refresh' },
            { expiresIn: '7d' }
        );

        // Store refresh token + update last login
        const refreshHash = await bcrypt.hash(refreshToken, 10);
        await db.update(users).set({
            refreshToken: refreshHash,
            lastLogin: new Date(),
        }).where(eq(users.id, user.id));

        // Audit
        await createAuditLog(db, {
            orgId: user.orgId,
            userId: user.id,
            action: 'user.login',
            resourceType: 'user',
            resourceId: user.id,
            ipAddress: request.ip,
        });

        return reply.send({
            user: {
                id: user.id,
                email: user.email,
                firstName: user.firstName,
                lastName: user.lastName,
                role: user.role,
                orgId: user.orgId,
                orgName: org?.name,
            },
            accessToken,
            refreshToken,
        });
    });

    // ── Refresh Token ──
    fastify.post('/auth/refresh', async (request, reply) => {
        const { refreshToken } = request.body || {};

        if (!refreshToken) {
            return reply.status(400).send({ error: 'Refresh token required' });
        }

        let decoded;
        try {
            decoded = fastify.jwt.verify(refreshToken);
        } catch (err) {
            return reply.status(401).send({ error: 'Invalid or expired refresh token' });
        }

        if (decoded.type !== 'refresh') {
            return reply.status(401).send({ error: 'Invalid token type' });
        }

        const [user] = await db.select().from(users).where(eq(users.id, decoded.sub)).limit(1);
        if (!user || !user.isActive) {
            return reply.status(401).send({ error: 'User not found or inactive' });
        }

        // Verify refresh token matches stored hash
        if (!user.refreshToken) {
            return reply.status(401).send({ error: 'No active session' });
        }

        const tokenValid = await bcrypt.compare(refreshToken, user.refreshToken);
        if (!tokenValid) {
            return reply.status(401).send({ error: 'Token has been revoked' });
        }

        // Issue new tokens (rotation)
        const newAccessToken = fastify.jwt.sign(
            {
                sub: user.id,
                email: user.email,
                orgId: user.orgId,
                role: user.role,
            },
            { expiresIn: '15m' }
        );

        const newRefreshToken = fastify.jwt.sign(
            { sub: user.id, type: 'refresh' },
            { expiresIn: '7d' }
        );

        const newRefreshHash = await bcrypt.hash(newRefreshToken, 10);
        await db.update(users).set({ refreshToken: newRefreshHash }).where(eq(users.id, user.id));

        return reply.send({
            accessToken: newAccessToken,
            refreshToken: newRefreshToken,
        });
    });

    // ── Logout ──
    fastify.post('/auth/logout', {
        preHandler: [fastify.authenticate],
    }, async (request, reply) => {
        await db.update(users).set({ refreshToken: null }).where(eq(users.id, request.user.sub));

        return reply.send({ message: 'Logged out successfully' });
    });

    // ── Get Current User ──
    fastify.get('/auth/me', {
        preHandler: [fastify.authenticate],
    }, async (request, reply) => {
        const [user] = await db
            .select({
                id: users.id,
                email: users.email,
                firstName: users.firstName,
                lastName: users.lastName,
                role: users.role,
                orgId: users.orgId,
                isActive: users.isActive,
                lastLogin: users.lastLogin,
                createdAt: users.createdAt,
            })
            .from(users)
            .where(eq(users.id, request.user.sub))
            .limit(1);

        if (!user) {
            return reply.status(404).send({ error: 'User not found' });
        }

        const [org] = await db.select().from(organizations).where(eq(organizations.id, user.orgId)).limit(1);

        return reply.send({
            ...user,
            orgName: org?.name,
            orgSlug: org?.slug,
            orgPlan: org?.plan,
        });
    });
}
