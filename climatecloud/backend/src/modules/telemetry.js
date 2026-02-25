// ─────────────────────────────────────────────
// ClimateCloud – Telemetry Module
// ─────────────────────────────────────────────

import { eq, and, desc, gte, lte, sql } from 'drizzle-orm';
import { z } from 'zod';
import { telemetry, devices } from '../db/schema.js';

// ──────── Validation ────────

const querySchema = z.object({
    from: z.string().datetime().optional(),
    to: z.string().datetime().optional(),
    limit: z.coerce.number().int().min(1).max(1000).default(100),
    offset: z.coerce.number().int().min(0).default(0),
});

// ──────── Route Plugin ────────

export default async function telemetryRoutes(fastify, opts) {
    const { db } = fastify;

    // ── Get Latest Telemetry for a Device ──
    fastify.get('/api/devices/:deviceId/telemetry/latest', {
        preHandler: [fastify.authenticate],
    }, async (request, reply) => {
        const { deviceId } = request.params;
        const orgId = request.user.orgId;

        // Verify device belongs to org
        const [device] = await db
            .select({ id: devices.id })
            .from(devices)
            .where(and(eq(devices.id, deviceId), eq(devices.orgId, orgId)))
            .limit(1);

        if (!device) {
            return reply.status(404).send({ error: 'Device not found' });
        }

        const [latest] = await db
            .select()
            .from(telemetry)
            .where(eq(telemetry.deviceId, deviceId))
            .orderBy(desc(telemetry.recordedAt))
            .limit(1);

        return reply.send({ telemetry: latest || null });
    });

    // ── Get Historical Telemetry ──
    fastify.get('/api/devices/:deviceId/telemetry', {
        preHandler: [fastify.authenticate],
    }, async (request, reply) => {
        const { deviceId } = request.params;
        const orgId = request.user.orgId;
        const params = querySchema.parse(request.query);

        // Verify device belongs to org
        const [device] = await db
            .select({ id: devices.id })
            .from(devices)
            .where(and(eq(devices.id, deviceId), eq(devices.orgId, orgId)))
            .limit(1);

        if (!device) {
            return reply.status(404).send({ error: 'Device not found' });
        }

        // Build conditions
        const conditions = [eq(telemetry.deviceId, deviceId)];

        if (params.from) {
            conditions.push(gte(telemetry.recordedAt, new Date(params.from)));
        }
        if (params.to) {
            conditions.push(lte(telemetry.recordedAt, new Date(params.to)));
        }

        const data = await db
            .select()
            .from(telemetry)
            .where(and(...conditions))
            .orderBy(desc(telemetry.recordedAt))
            .limit(params.limit)
            .offset(params.offset);

        // Get total count for pagination
        const [{ count }] = await db
            .select({ count: sql`count(*)::int` })
            .from(telemetry)
            .where(and(...conditions));

        return reply.send({
            telemetry: data,
            pagination: {
                total: count,
                limit: params.limit,
                offset: params.offset,
                hasMore: params.offset + params.limit < count,
            },
        });
    });

    // ── Get Aggregated Telemetry (for charts) ──
    fastify.get('/api/devices/:deviceId/telemetry/aggregate', {
        preHandler: [fastify.authenticate],
    }, async (request, reply) => {
        const { deviceId } = request.params;
        const orgId = request.user.orgId;

        const intervalSchema = z.object({
            interval: z.enum(['1h', '6h', '24h', '7d', '30d']).default('24h'),
        });
        const { interval } = intervalSchema.parse(request.query);

        // Verify device
        const [device] = await db
            .select({ id: devices.id })
            .from(devices)
            .where(and(eq(devices.id, deviceId), eq(devices.orgId, orgId)))
            .limit(1);

        if (!device) {
            return reply.status(404).send({ error: 'Device not found' });
        }

        const intervalMap = {
            '1h': '1 hour',
            '6h': '6 hours',
            '24h': '24 hours',
            '7d': '7 days',
            '30d': '30 days',
        };

        const bucketMap = {
            '1h': '1 minute',
            '6h': '5 minutes',
            '24h': '15 minutes',
            '7d': '1 hour',
            '30d': '6 hours',
        };

        const pgInterval = intervalMap[interval];
        const bucket = bucketMap[interval];

        const data = await db.execute(sql`
      SELECT
        date_trunc(${bucket}, recorded_at) as time_bucket,
        AVG(temperature) as avg_temperature,
        MIN(temperature) as min_temperature,
        MAX(temperature) as max_temperature,
        AVG(humidity) as avg_humidity,
        MIN(humidity) as min_humidity,
        MAX(humidity) as max_humidity,
        COUNT(*) as sample_count
      FROM telemetry
      WHERE device_id = ${deviceId}
        AND recorded_at >= NOW() - ${pgInterval}::interval
      GROUP BY time_bucket
      ORDER BY time_bucket ASC
    `);

        return reply.send({ data: data.rows || data });
    });

    // ── Get Dashboard Summary (all devices) ──
    fastify.get('/api/dashboard/summary', {
        preHandler: [fastify.authenticate],
    }, async (request, reply) => {
        const orgId = request.user.orgId;

        // Get all devices with latest telemetry
        const summary = await db.execute(sql`
      SELECT
        d.id,
        d.device_uid,
        d.name,
        d.type,
        d.is_online,
        d.last_seen,
        d.fw_version,
        t.temperature,
        t.humidity,
        t.heater_on,
        t.cooler_on,
        t.setpoint,
        t.mode,
        t.recorded_at
      FROM devices d
      LEFT JOIN LATERAL (
        SELECT *
        FROM telemetry
        WHERE device_id = d.id
        ORDER BY recorded_at DESC
        LIMIT 1
      ) t ON true
      WHERE d.org_id = ${orgId}
      ORDER BY d.name ASC
    `);

        const totalDevices = summary.rows?.length || summary.length || 0;
        const onlineDevices = (summary.rows || summary).filter(d => d.is_online).length;

        return reply.send({
            devices: summary.rows || summary,
            stats: {
                total: totalDevices,
                online: onlineDevices,
                offline: totalDevices - onlineDevices,
            },
        });
    });
}
