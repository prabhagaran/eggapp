import bcrypt from 'bcrypt';
import { eq, and } from 'drizzle-orm';
import { z } from 'zod';
import { users, organizations } from '../db/schema.js';
import { createAuditLog } from './audit.js';

const inviteUserSchema = z.object({
    email: z.string().email().max(320),
    firstName: z.string().min(1).max(100),
    lastName: z.string().min(1).max(100),
    role: z.enum(['admin', 'technician', 'viewer']),
    password: z.string().min(8).max(128),
});

const updateUserSchema = z.object({
    firstName: z.string().min(1).max(100).optional(),
    lastName: z.string().min(1).max(100).optional(),
    role: z.enum(['admin', 'technician', 'viewer']).optional(),
    isActive: z.boolean().optional(),
});

export default async function adminRoutes(fastify, opts) {
    const { db } = fastify;

    fastify.get('/api/admin/users', {
        preHandler: [fastify.authenticate, fastify.authorize(['owner', 'admin'])],
    }, async (request, reply) => {
        const orgUsers = await db.select({
            id: users.id, email: users.email, firstName: users.firstName,
            lastName: users.lastName, role: users.role, isActive: users.isActive,
            lastLogin: users.lastLogin, createdAt: users.createdAt,
        }).from(users).where(eq(users.orgId, request.user.orgId));
        return reply.send({ users: orgUsers });
    });

    fastify.post('/api/admin/users', {
        preHandler: [fastify.authenticate, fastify.authorize(['owner', 'admin'])],
    }, async (request, reply) => {
        const v = inviteUserSchema.parse(request.body);
        const existing = await db.select().from(users).where(eq(users.email, v.email.toLowerCase())).limit(1);
        if (existing.length > 0) return reply.status(409).send({ error: 'Email already registered' });
        const passwordHash = await bcrypt.hash(v.password, 12);
        const [user] = await db.insert(users).values({
            email: v.email.toLowerCase(), passwordHash, firstName: v.firstName,
            lastName: v.lastName, orgId: request.user.orgId, role: v.role,
        }).returning();
        await createAuditLog(db, {
            orgId: request.user.orgId, userId: request.user.sub,
            action: 'admin.user.create', resourceType: 'user', resourceId: user.id, ipAddress: request.ip
        });
        return reply.status(201).send({ user: { id: user.id, email: user.email, role: user.role } });
    });

    fastify.patch('/api/admin/users/:userId', {
        preHandler: [fastify.authenticate, fastify.authorize(['owner', 'admin'])],
    }, async (request, reply) => {
        const v = updateUserSchema.parse(request.body);
        const [user] = await db.select().from(users)
            .where(and(eq(users.id, request.params.userId), eq(users.orgId, request.user.orgId))).limit(1);
        if (!user) return reply.status(404).send({ error: 'User not found' });
        if (user.role === 'owner' && v.role) return reply.status(403).send({ error: 'Cannot change owner role' });
        const [updated] = await db.update(users).set({ ...v, updatedAt: new Date() })
            .where(eq(users.id, request.params.userId)).returning();
        return reply.send({ user: { id: updated.id, email: updated.email, role: updated.role, isActive: updated.isActive } });
    });

    fastify.delete('/api/admin/users/:userId', {
        preHandler: [fastify.authenticate, fastify.authorize(['owner'])],
    }, async (request, reply) => {
        if (request.params.userId === request.user.sub) return reply.status(400).send({ error: 'Cannot delete yourself' });
        const [user] = await db.select().from(users)
            .where(and(eq(users.id, request.params.userId), eq(users.orgId, request.user.orgId))).limit(1);
        if (!user) return reply.status(404).send({ error: 'User not found' });
        if (user.role === 'owner') return reply.status(403).send({ error: 'Cannot delete owner' });
        await db.delete(users).where(eq(users.id, request.params.userId));
        return reply.send({ message: 'User deleted' });
    });

    fastify.get('/api/admin/organization', {
        preHandler: [fastify.authenticate, fastify.authorize(['owner', 'admin'])],
    }, async (request, reply) => {
        const [org] = await db.select().from(organizations).where(eq(organizations.id, request.user.orgId)).limit(1);
        return reply.send({ organization: org });
    });
}
