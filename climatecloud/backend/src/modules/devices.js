// ─────────────────────────────────────────────
// ClimateCloud – Device Management Module
// ─────────────────────────────────────────────

import crypto from 'node:crypto';
import bcrypt from 'bcrypt';
import { eq, and, desc } from 'drizzle-orm';
import { z } from 'zod';
import { devices, deviceShadows, users } from '../db/schema.js';
import { createAuditLog } from './audit.js';

// ──────── Validation ────────

const registerDeviceSchema = z.object({
    name: z.string().min(1).max(255),
    type: z.enum(['egg_incubator', 'climate_chamber', 'thermostat', 'generic']).default('generic'),
    deviceUid: z.string().min(4).max(64).optional(),
    metadata: z.record(z.any()).optional(),
});

const updateDeviceSchema = z.object({
    name: z.string().min(1).max(255).optional(),
    type: z.enum(['egg_incubator', 'climate_chamber', 'thermostat', 'generic']).optional(),
    assignedUserId: z.string().uuid().nullable().optional(),
    metadata: z.record(z.any()).optional(),
});

// ──────── Helpers ────────

function generateApiKey() {
    return 'cc_' + crypto.randomBytes(32).toString('hex');
}

function generateDeviceUid() {
    return 'DEV_' + crypto.randomBytes(8).toString('hex').toUpperCase();
}

function generateMqttPassword() {
    return crypto.randomBytes(24).toString('base64url');
}

// ──────── Route Plugin ────────

export default async function deviceRoutes(fastify, opts) {
    const { db } = fastify;

    // ── Register Device ──
    fastify.post('/api/devices', {
        preHandler: [fastify.authenticate, fastify.authorize(['owner', 'admin'])],
    }, async (request, reply) => {
        const validated = registerDeviceSchema.parse(request.body);
        const orgId = request.user.orgId;

        // Check device limit
        const existingDevices = await db.select().from(devices).where(eq(devices.orgId, orgId));
        // TODO: Check against org.maxDevices

        const deviceUid = validated.deviceUid || generateDeviceUid();

        // Check for duplicate UID
        const existing = await db.select().from(devices).where(eq(devices.deviceUid, deviceUid)).limit(1);
        if (existing.length > 0) {
            return reply.status(409).send({ error: 'Device UID already exists' });
        }

        // Generate credentials
        const apiKey = generateApiKey();
        const apiKeyHash = crypto.createHash('sha256').update(apiKey).digest('hex');
        const mqttPassword = generateMqttPassword();
        const mqttPasswordHash = await bcrypt.hash(mqttPassword, 10);

        // Insert device
        const [device] = await db.insert(devices).values({
            deviceUid,
            name: validated.name,
            type: validated.type,
            orgId,
            apiKeyHash,
            mqttUsername: deviceUid.toLowerCase(),
            mqttPasswordHash,
            metadata: validated.metadata || {},
        }).returning();

        // Create shadow
        await db.insert(deviceShadows).values({
            deviceId: device.id,
            desired: {},
            reported: {},
            delta: {},
        });

        // Audit
        await createAuditLog(db, {
            orgId,
            userId: request.user.sub,
            action: 'device.register',
            resourceType: 'device',
            resourceId: device.id,
            details: { deviceUid, name: validated.name },
            ipAddress: request.ip,
        });

        // Return credentials (shown only once!)
        return reply.status(201).send({
            device: {
                id: device.id,
                deviceUid: device.deviceUid,
                name: device.name,
                type: device.type,
            },
            credentials: {
                apiKey, // ⚠️ Show only once
                mqttUsername: device.mqttUsername,
                mqttPassword, // ⚠️ Show only once
                mqttBrokerUrl: process.env.MQTT_BROKER_URL || 'mqtts://broker.emqx.io:8883',
            },
            warning: 'Save these credentials now. They cannot be retrieved again.',
        });
    });

    // ── List Devices ──
    fastify.get('/api/devices', {
        preHandler: [fastify.authenticate],
    }, async (request, reply) => {
        const orgId = request.user.orgId;
        const role = request.user.role;

        let query;
        if (role === 'owner' || role === 'admin') {
            // Admins see all org devices
            query = db
                .select({
                    id: devices.id,
                    deviceUid: devices.deviceUid,
                    name: devices.name,
                    type: devices.type,
                    isOnline: devices.isOnline,
                    lastSeen: devices.lastSeen,
                    fwVersion: devices.fwVersion,
                    assignedUserId: devices.assignedUserId,
                    metadata: devices.metadata,
                    createdAt: devices.createdAt,
                })
                .from(devices)
                .where(eq(devices.orgId, orgId))
                .orderBy(desc(devices.createdAt));
        } else {
            // Non-admins only see assigned devices
            query = db
                .select({
                    id: devices.id,
                    deviceUid: devices.deviceUid,
                    name: devices.name,
                    type: devices.type,
                    isOnline: devices.isOnline,
                    lastSeen: devices.lastSeen,
                    fwVersion: devices.fwVersion,
                    assignedUserId: devices.assignedUserId,
                    metadata: devices.metadata,
                    createdAt: devices.createdAt,
                })
                .from(devices)
                .where(and(
                    eq(devices.orgId, orgId),
                    eq(devices.assignedUserId, request.user.sub)
                ))
                .orderBy(desc(devices.createdAt));
        }

        const result = await query;
        return reply.send({ devices: result });
    });

    // ── Get Single Device ──
    fastify.get('/api/devices/:deviceId', {
        preHandler: [fastify.authenticate],
    }, async (request, reply) => {
        const { deviceId } = request.params;
        const orgId = request.user.orgId;

        const [device] = await db
            .select()
            .from(devices)
            .where(and(eq(devices.id, deviceId), eq(devices.orgId, orgId)))
            .limit(1);

        if (!device) {
            return reply.status(404).send({ error: 'Device not found' });
        }

        // Get shadow
        const [shadow] = await db
            .select()
            .from(deviceShadows)
            .where(eq(deviceShadows.deviceId, deviceId))
            .limit(1);

        return reply.send({
            device: {
                id: device.id,
                deviceUid: device.deviceUid,
                name: device.name,
                type: device.type,
                isOnline: device.isOnline,
                lastSeen: device.lastSeen,
                fwVersion: device.fwVersion,
                assignedUserId: device.assignedUserId,
                metadata: device.metadata,
                createdAt: device.createdAt,
            },
            shadow: shadow || null,
        });
    });

    // ── Update Device ──
    fastify.patch('/api/devices/:deviceId', {
        preHandler: [fastify.authenticate, fastify.authorize(['owner', 'admin'])],
    }, async (request, reply) => {
        const { deviceId } = request.params;
        const orgId = request.user.orgId;
        const validated = updateDeviceSchema.parse(request.body);

        const [device] = await db
            .select()
            .from(devices)
            .where(and(eq(devices.id, deviceId), eq(devices.orgId, orgId)))
            .limit(1);

        if (!device) {
            return reply.status(404).send({ error: 'Device not found' });
        }

        // If assigning user, verify they belong to same org
        if (validated.assignedUserId) {
            const [user] = await db
                .select()
                .from(users)
                .where(and(eq(users.id, validated.assignedUserId), eq(users.orgId, orgId)))
                .limit(1);

            if (!user) {
                return reply.status(400).send({ error: 'User not found in organization' });
            }
        }

        const [updated] = await db
            .update(devices)
            .set({ ...validated, updatedAt: new Date() })
            .where(eq(devices.id, deviceId))
            .returning();

        await createAuditLog(db, {
            orgId,
            userId: request.user.sub,
            action: 'device.update',
            resourceType: 'device',
            resourceId: deviceId,
            details: validated,
            ipAddress: request.ip,
        });

        return reply.send({ device: updated });
    });

    // ── Delete Device ──
    fastify.delete('/api/devices/:deviceId', {
        preHandler: [fastify.authenticate, fastify.authorize(['owner', 'admin'])],
    }, async (request, reply) => {
        const { deviceId } = request.params;
        const orgId = request.user.orgId;

        const [device] = await db
            .select()
            .from(devices)
            .where(and(eq(devices.id, deviceId), eq(devices.orgId, orgId)))
            .limit(1);

        if (!device) {
            return reply.status(404).send({ error: 'Device not found' });
        }

        await db.delete(devices).where(eq(devices.id, deviceId));

        await createAuditLog(db, {
            orgId,
            userId: request.user.sub,
            action: 'device.delete',
            resourceType: 'device',
            resourceId: deviceId,
            details: { deviceUid: device.deviceUid, name: device.name },
            ipAddress: request.ip,
        });

        return reply.send({ message: 'Device deleted', deviceUid: device.deviceUid });
    });

    // ── Regenerate API Key ──
    fastify.post('/api/devices/:deviceId/regenerate-key', {
        preHandler: [fastify.authenticate, fastify.authorize(['owner', 'admin'])],
    }, async (request, reply) => {
        const { deviceId } = request.params;
        const orgId = request.user.orgId;

        const [device] = await db
            .select()
            .from(devices)
            .where(and(eq(devices.id, deviceId), eq(devices.orgId, orgId)))
            .limit(1);

        if (!device) {
            return reply.status(404).send({ error: 'Device not found' });
        }

        const newApiKey = generateApiKey();
        const newApiKeyHash = crypto.createHash('sha256').update(newApiKey).digest('hex');
        const newMqttPassword = generateMqttPassword();
        const newMqttPasswordHash = await bcrypt.hash(newMqttPassword, 10);

        await db.update(devices).set({
            apiKeyHash: newApiKeyHash,
            mqttPasswordHash: newMqttPasswordHash,
            updatedAt: new Date(),
        }).where(eq(devices.id, deviceId));

        await createAuditLog(db, {
            orgId,
            userId: request.user.sub,
            action: 'device.regenerate_key',
            resourceType: 'device',
            resourceId: deviceId,
            ipAddress: request.ip,
        });

        return reply.send({
            credentials: {
                apiKey: newApiKey,
                mqttUsername: device.mqttUsername,
                mqttPassword: newMqttPassword,
            },
            warning: 'Save these credentials now. The device will need to be reconfigured.',
        });
    });
}
