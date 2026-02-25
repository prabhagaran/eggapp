// ─────────────────────────────────────────────
// ClimateCloud – Main Server Entry Point
// ─────────────────────────────────────────────

import Fastify from 'fastify';
import cors from '@fastify/cors';
import helmet from '@fastify/helmet';
import rateLimit from '@fastify/rate-limit';
import fastifyJwt from '@fastify/jwt';
import fastifyWebsocket from '@fastify/websocket';
import fastifyCookie from '@fastify/cookie';

import config from './config/index.js';
import { db } from './db/index.js';
import { initMqttService, getMqttClient, setWsClients } from './services/mqtt.js';

// Route modules
import authRoutes from './modules/auth.js';
import deviceRoutes from './modules/devices.js';
import telemetryRoutes from './modules/telemetry.js';
import commandRoutes from './modules/commands.js';
import shadowRoutes from './modules/shadow.js';
import adminRoutes from './modules/admin.js';

// ──── Create Fastify Instance ────

const fastify = Fastify({
    logger: {
        level: config.nodeEnv === 'production' ? 'info' : 'debug',
        transport: config.nodeEnv !== 'production' ? { target: 'pino-pretty' } : undefined,
    },
    trustProxy: true,
});

// ──── Register Plugins ────

await fastify.register(cors, {
    origin: config.corsOrigin,
    credentials: true,
});

await fastify.register(helmet, {
    contentSecurityPolicy: false,
});

await fastify.register(rateLimit, {
    max: config.rateLimit.max,
    timeWindow: config.rateLimit.windowMs,
});

await fastify.register(fastifyJwt, {
    secret: config.jwt.secret,
});

await fastify.register(fastifyCookie);
await fastify.register(fastifyWebsocket);

// ──── Decorate with DB and MQTT ────

fastify.decorate('db', db);

// ──── Authentication Decorator ────

fastify.decorate('authenticate', async function (request, reply) {
    try {
        const authHeader = request.headers.authorization;
        if (!authHeader || !authHeader.startsWith('Bearer ')) {
            return reply.status(401).send({ error: 'Authorization header required' });
        }
        const token = authHeader.substring(7);
        request.user = fastify.jwt.verify(token);
    } catch (err) {
        return reply.status(401).send({ error: 'Invalid or expired token' });
    }
});

// ──── Authorization Decorator ────

fastify.decorate('authorize', function (allowedRoles) {
    return async function (request, reply) {
        if (!request.user || !allowedRoles.includes(request.user.role)) {
            return reply.status(403).send({ error: 'Insufficient permissions' });
        }
    };
});

// ──── WebSocket for Real-time Updates ────

const wsClients = new Set();
setWsClients(wsClients);

fastify.register(async function (fastify) {
    fastify.get('/ws', { websocket: true }, (socket, req) => {
        // Verify JWT from query param for WebSocket
        const token = req.query.token;
        if (token) {
            try {
                const user = fastify.jwt.verify(token);
                socket.user = user;
            } catch (e) {
                socket.close(4001, 'Invalid token');
                return;
            }
        }

        wsClients.add(socket);
        console.log(`[WS] Client connected. Total: ${wsClients.size}`);

        socket.on('close', () => {
            wsClients.delete(socket);
            console.log(`[WS] Client disconnected. Total: ${wsClients.size}`);
        });

        socket.on('message', (msg) => {
            // Handle ping/pong or subscribe to specific device updates
            try {
                const data = JSON.parse(msg.toString());
                if (data.type === 'ping') {
                    socket.send(JSON.stringify({ type: 'pong' }));
                }
            } catch (e) { /* ignore */ }
        });
    });
});

// ──── Register Route Modules ────

await fastify.register(authRoutes);
await fastify.register(deviceRoutes);
await fastify.register(telemetryRoutes);
await fastify.register(commandRoutes);
await fastify.register(shadowRoutes);
await fastify.register(adminRoutes);

// ──── Health Check ────

fastify.get('/health', async () => ({
    status: 'ok',
    timestamp: new Date().toISOString(),
    mqtt: getMqttClient()?.connected ? 'connected' : 'disconnected',
    database: db ? 'connected' : 'disconnected',
}));

// ──── API Info ────

fastify.get('/', async () => ({
    name: 'ClimateCloud API',
    version: '1.0.0',
    docs: '/api-docs',
}));

// ──── Error Handler ────

fastify.setErrorHandler((error, request, reply) => {
    if (error.validation) {
        return reply.status(400).send({
            error: 'Validation Error',
            details: error.validation,
        });
    }

    if (error.name === 'ZodError') {
        return reply.status(400).send({
            error: 'Validation Error',
            details: error.issues,
        });
    }

    fastify.log.error(error);
    reply.status(error.statusCode || 500).send({
        error: config.nodeEnv === 'production'
            ? 'Internal Server Error'
            : error.message,
    });
});

// ──── Start Server ────

const start = async () => {
    try {
        // Initialize MQTT
        if (db) {
            const mqttClient = initMqttService(db);
            fastify.decorate('mqttClient', mqttClient);
        }

        await fastify.listen({ port: config.port, host: config.host });
        console.log(`\n🌩  ClimateCloud API running on http://${config.host}:${config.port}`);
        console.log(`📡 MQTT: ${config.mqtt.brokerUrl}`);
        console.log(`🗄  Database: ${db ? 'Connected' : 'Not configured'}\n`);
    } catch (err) {
        fastify.log.error(err);
        process.exit(1);
    }
};

start();
