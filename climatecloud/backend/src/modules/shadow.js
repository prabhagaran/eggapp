// ─────────────────────────────────────────────
// ClimateCloud – Device Shadow Module
// ─────────────────────────────────────────────

import { eq, and } from 'drizzle-orm';
import { z } from 'zod';
import { deviceShadows, devices } from '../db/schema.js';
import { computeDelta } from './commands.js';

// ──────── Validation ────────

const updateDesiredSchema = z.object({
    desired: z.record(z.any()),
});

// ──────── Route Plugin ────────

export default async function shadowRoutes(fastify, opts) {
    const { db, mqttClient } = fastify;

    // ── Get Device Shadow ──
    fastify.get('/api/devices/:deviceId/shadow', {
        preHandler: [fastify.authenticate],
    }, async (request, reply) => {
        const { deviceId } = request.params;
        const orgId = request.user.orgId;

        // Verify access
        const [device] = await db
            .select()
            .from(devices)
            .where(and(eq(devices.id, deviceId), eq(devices.orgId, orgId)))
            .limit(1);

        if (!device) {
            return reply.status(404).send({ error: 'Device not found' });
        }

        const [shadow] = await db
            .select()
            .from(deviceShadows)
            .where(eq(deviceShadows.deviceId, deviceId))
            .limit(1);

        if (!shadow) {
            return reply.status(404).send({ error: 'Shadow not found' });
        }

        return reply.send({
            shadow: {
                desired: shadow.desired,
                reported: shadow.reported,
                delta: shadow.delta,
                version: shadow.version,
                lastSync: shadow.lastSync,
                updatedAt: shadow.updatedAt,
            },
        });
    });

    // ── Update Shadow Desired State ──
    fastify.put('/api/devices/:deviceId/shadow/desired', {
        preHandler: [fastify.authenticate, fastify.authorize(['owner', 'admin', 'technician'])],
    }, async (request, reply) => {
        const { deviceId } = request.params;
        const orgId = request.user.orgId;
        const validated = updateDesiredSchema.parse(request.body);

        // Verify access
        const [device] = await db
            .select()
            .from(devices)
            .where(and(eq(devices.id, deviceId), eq(devices.orgId, orgId)))
            .limit(1);

        if (!device) {
            return reply.status(404).send({ error: 'Device not found' });
        }

        const [shadow] = await db
            .select()
            .from(deviceShadows)
            .where(eq(deviceShadows.deviceId, deviceId))
            .limit(1);

        if (!shadow) {
            return reply.status(404).send({ error: 'Shadow not found' });
        }

        // Merge desired state
        const newDesired = { ...shadow.desired, ...validated.desired };
        const delta = computeDelta(newDesired, shadow.reported);

        const [updated] = await db.update(deviceShadows).set({
            desired: newDesired,
            delta,
            version: shadow.version + 1,
            updatedAt: new Date(),
        }).where(eq(deviceShadows.deviceId, deviceId)).returning();

        // If there's a delta, publish to device
        if (delta && Object.keys(delta).length > 0 && mqttClient?.connected) {
            const topic = `climatecloud/device/${device.deviceUid}/shadow/delta`;
            mqttClient.publish(topic, JSON.stringify({
                delta,
                version: updated.version,
                timestamp: new Date().toISOString(),
            }), { qos: 1 });
        }

        return reply.send({
            shadow: {
                desired: updated.desired,
                reported: updated.reported,
                delta: updated.delta,
                version: updated.version,
            },
        });
    });

    // ── Clear Shadow Delta (force sync) ──
    fastify.post('/api/devices/:deviceId/shadow/sync', {
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

        // Request shadow state from device
        if (mqttClient?.connected) {
            const topic = `climatecloud/device/${device.deviceUid}/shadow/get`;
            mqttClient.publish(topic, JSON.stringify({
                timestamp: new Date().toISOString(),
            }), { qos: 1 });
        }

        return reply.send({ message: 'Shadow sync requested' });
    });
}

// ──────── Shadow Update Handler (called from MQTT service) ────────

export async function updateReportedState(db, deviceId, reported) {
    const [shadow] = await db
        .select()
        .from(deviceShadows)
        .where(eq(deviceShadows.deviceId, deviceId))
        .limit(1);

    if (!shadow) return null;

    const newReported = { ...shadow.reported, ...reported };
    const delta = computeDelta(shadow.desired, newReported);

    const [updated] = await db.update(deviceShadows).set({
        reported: newReported,
        delta,
        lastSync: new Date(),
        updatedAt: new Date(),
    }).where(eq(deviceShadows.deviceId, deviceId)).returning();

    return updated;
}
