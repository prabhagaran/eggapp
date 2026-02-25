// ─────────────────────────────────────────────
// ClimateCloud – Command Module
// ─────────────────────────────────────────────

import { eq, and, desc } from 'drizzle-orm';
import { z } from 'zod';
import { commands, devices, deviceShadows } from '../db/schema.js';
import { createAuditLog } from './audit.js';

// ──────── Validation ────────

const sendCommandSchema = z.object({
    commandType: z.enum(['set_setpoint', 'set_mode', 'set_humidity_setpoint', 'set_hysteresis', 'restart', 'factory_reset', 'custom']),
    payload: z.record(z.any()),
});

// ──────── Route Plugin ────────

export default async function commandRoutes(fastify, opts) {
    const { db, mqttClient } = fastify;

    // ── Send Command to Device ──
    fastify.post('/api/devices/:deviceId/commands', {
        preHandler: [fastify.authenticate, fastify.authorize(['owner', 'admin', 'technician'])],
    }, async (request, reply) => {
        const { deviceId } = request.params;
        const orgId = request.user.orgId;
        const validated = sendCommandSchema.parse(request.body);

        // Verify device
        const [device] = await db
            .select()
            .from(devices)
            .where(and(eq(devices.id, deviceId), eq(devices.orgId, orgId)))
            .limit(1);

        if (!device) {
            return reply.status(404).send({ error: 'Device not found' });
        }

        // Create command record
        const [command] = await db.insert(commands).values({
            deviceId,
            userId: request.user.sub,
            commandType: validated.commandType,
            payload: validated.payload,
            status: 'pending',
        }).returning();

        // Update device shadow desired state if applicable
        if (['set_setpoint', 'set_mode', 'set_humidity_setpoint', 'set_hysteresis'].includes(validated.commandType)) {
            const [shadow] = await db
                .select()
                .from(deviceShadows)
                .where(eq(deviceShadows.deviceId, deviceId))
                .limit(1);

            if (shadow) {
                const newDesired = { ...shadow.desired, ...validated.payload };
                const delta = computeDelta(newDesired, shadow.reported);

                await db.update(deviceShadows).set({
                    desired: newDesired,
                    delta,
                    version: shadow.version + 1,
                    updatedAt: new Date(),
                }).where(eq(deviceShadows.deviceId, deviceId));
            }
        }

        // Publish via MQTT
        const mqttTopic = `climatecloud/device/${device.deviceUid}/command`;
        const mqttPayload = JSON.stringify({
            command_id: command.id,
            command_type: validated.commandType,
            ...validated.payload,
            timestamp: new Date().toISOString(),
        });

        if (mqttClient && mqttClient.connected) {
            mqttClient.publish(mqttTopic, mqttPayload, { qos: 1 }, (err) => {
                if (err) {
                    console.error(`[MQTT] Failed to publish command to ${mqttTopic}:`, err);
                } else {
                    console.log(`[MQTT] Command published to ${mqttTopic}`);
                    // Update status to sent
                    db.update(commands).set({ status: 'sent', sentAt: new Date() }).where(eq(commands.id, command.id)).then(() => { });
                }
            });
        } else {
            console.warn('[MQTT] Client not connected, command queued');
        }

        // Audit
        await createAuditLog(db, {
            orgId,
            userId: request.user.sub,
            action: 'command.send',
            resourceType: 'command',
            resourceId: command.id,
            details: { deviceUid: device.deviceUid, commandType: validated.commandType },
            ipAddress: request.ip,
        });

        return reply.status(201).send({
            command: {
                id: command.id,
                commandType: command.commandType,
                payload: command.payload,
                status: command.status,
                createdAt: command.createdAt,
            },
        });
    });

    // ── Get Command History ──
    fastify.get('/api/devices/:deviceId/commands', {
        preHandler: [fastify.authenticate],
    }, async (request, reply) => {
        const { deviceId } = request.params;
        const orgId = request.user.orgId;
        const limit = parseInt(request.query.limit || '50');

        // Verify device
        const [device] = await db
            .select({ id: devices.id })
            .from(devices)
            .where(and(eq(devices.id, deviceId), eq(devices.orgId, orgId)))
            .limit(1);

        if (!device) {
            return reply.status(404).send({ error: 'Device not found' });
        }

        const cmdHistory = await db
            .select()
            .from(commands)
            .where(eq(commands.deviceId, deviceId))
            .orderBy(desc(commands.createdAt))
            .limit(limit);

        return reply.send({ commands: cmdHistory });
    });

    // ── Get Command Status ──
    fastify.get('/api/commands/:commandId', {
        preHandler: [fastify.authenticate],
    }, async (request, reply) => {
        const { commandId } = request.params;

        const [command] = await db
            .select()
            .from(commands)
            .where(eq(commands.id, commandId))
            .limit(1);

        if (!command) {
            return reply.status(404).send({ error: 'Command not found' });
        }

        // Verify org access
        const [device] = await db
            .select({ orgId: devices.orgId })
            .from(devices)
            .where(eq(devices.id, command.deviceId))
            .limit(1);

        if (!device || device.orgId !== request.user.orgId) {
            return reply.status(404).send({ error: 'Command not found' });
        }

        return reply.send({ command });
    });
}

// ──────── Shadow Delta Computation ────────

function computeDelta(desired, reported) {
    const delta = {};
    for (const key of Object.keys(desired)) {
        if (JSON.stringify(desired[key]) !== JSON.stringify(reported[key])) {
            delta[key] = desired[key];
        }
    }
    return Object.keys(delta).length > 0 ? delta : null;
}

export { computeDelta };
