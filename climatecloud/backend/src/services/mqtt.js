// ─────────────────────────────────────────────
// ClimateCloud – MQTT Service
// ─────────────────────────────────────────────

import mqtt from 'mqtt';
import { eq } from 'drizzle-orm';
import config from '../config/index.js';
import { devices, telemetry, commands } from '../db/schema.js';
import { updateReportedState } from '../modules/shadow.js';

let mqttClient = null;
let db = null;
let wsClients = new Set(); // WebSocket clients for real-time push

export function getMqttClient() { return mqttClient; }
export function setWsClients(clients) { wsClients = clients; }

export function initMqttService(database) {
    db = database;

    const options = {
        clientId: config.mqtt.clientId,
        username: config.mqtt.username,
        password: config.mqtt.password,
        clean: true,
        reconnectPeriod: 5000,
        connectTimeout: 30000,
    };

    console.log(`[MQTT] Connecting to ${config.mqtt.brokerUrl}...`);
    mqttClient = mqtt.connect(config.mqtt.brokerUrl, options);

    mqttClient.on('connect', () => {
        console.log('[MQTT] Connected to broker');
        // Subscribe to all device topics
        mqttClient.subscribe('climatecloud/device/+/telemetry', { qos: 1 });
        mqttClient.subscribe('climatecloud/device/+/status', { qos: 1 });
        mqttClient.subscribe('climatecloud/device/+/command/ack', { qos: 1 });
        mqttClient.subscribe('climatecloud/device/+/shadow/update', { qos: 1 });
        console.log('[MQTT] Subscribed to device topics');
    });

    mqttClient.on('message', async (topic, message) => {
        try {
            const parts = topic.split('/');
            const deviceUid = parts[2];
            const messageType = parts[3];

            const payload = JSON.parse(message.toString());

            switch (messageType) {
                case 'telemetry':
                    await handleTelemetry(deviceUid, payload);
                    break;
                case 'status':
                    await handleStatus(deviceUid, payload);
                    break;
                case 'command':
                    if (parts[4] === 'ack') await handleCommandAck(deviceUid, payload);
                    break;
                case 'shadow':
                    if (parts[4] === 'update') await handleShadowUpdate(deviceUid, payload);
                    break;
            }
        } catch (err) {
            console.error('[MQTT] Error processing message:', err.message);
        }
    });

    mqttClient.on('error', (err) => {
        console.error('[MQTT] Connection error:', err.message);
    });

    mqttClient.on('reconnect', () => {
        console.log('[MQTT] Reconnecting...');
    });

    return mqttClient;
}

// ──── Handle Telemetry ────
async function handleTelemetry(deviceUid, payload) {
    if (!db) return;

    const [device] = await db.select().from(devices)
        .where(eq(devices.deviceUid, deviceUid)).limit(1);
    if (!device) { console.warn(`[MQTT] Unknown device: ${deviceUid}`); return; }

    // Insert telemetry
    await db.insert(telemetry).values({
        deviceId: device.id,
        temperature: payload.temperature,
        humidity: payload.humidity,
        heaterOn: payload.heater,
        coolerOn: payload.cooler,
        humidifierOn: payload.humidifier,
        setpoint: payload.setpoint,
        humidSetpoint: payload.humid_setpoint,
        mode: payload.mode,
        fwVersion: payload.fw_version,
        payload,
        recordedAt: payload.timestamp ? new Date(payload.timestamp) : new Date(),
    });

    // Update device last_seen and online status
    await db.update(devices).set({
        isOnline: true,
        lastSeen: new Date(),
        fwVersion: payload.fw_version || device.fwVersion,
    }).where(eq(devices.id, device.id));

    // Update shadow reported state
    await updateReportedState(db, device.id, {
        temperature: payload.temperature,
        humidity: payload.humidity,
        heater: payload.heater,
        cooler: payload.cooler,
        setpoint: payload.setpoint,
        mode: payload.mode,
        fw_version: payload.fw_version,
    });

    // Push to WebSocket clients
    broadcastToWs({
        type: 'telemetry',
        deviceId: device.id,
        deviceUid,
        data: payload,
        timestamp: new Date().toISOString(),
    });

    console.log(`[MQTT] Telemetry from ${deviceUid}: T=${payload.temperature} H=${payload.humidity}`);
}

// ──── Handle Status (Online/Offline) ────
async function handleStatus(deviceUid, payload) {
    if (!db) return;

    const isOnline = payload.status === 'online';

    await db.update(devices).set({
        isOnline,
        lastSeen: new Date(),
    }).where(eq(devices.deviceUid, deviceUid));

    broadcastToWs({
        type: 'device_status',
        deviceUid,
        status: payload.status,
        timestamp: new Date().toISOString(),
    });

    console.log(`[MQTT] Device ${deviceUid} is ${payload.status}`);
}

// ──── Handle Command Acknowledgment ────
async function handleCommandAck(deviceUid, payload) {
    if (!db) return;

    const { command_id, status } = payload;
    if (!command_id) return;

    const newStatus = status === 'applied' ? 'applied' : 'failed';

    await db.update(commands).set({
        status: newStatus,
        response: payload,
        ackAt: new Date(),
    }).where(eq(commands.id, command_id));

    broadcastToWs({
        type: 'command_ack',
        deviceUid,
        commandId: command_id,
        status: newStatus,
        timestamp: new Date().toISOString(),
    });

    console.log(`[MQTT] Command ${command_id} ${newStatus} by ${deviceUid}`);
}

// ──── Handle Shadow Update from Device ────
async function handleShadowUpdate(deviceUid, payload) {
    if (!db) return;

    const [device] = await db.select().from(devices)
        .where(eq(devices.deviceUid, deviceUid)).limit(1);
    if (!device) return;

    const updated = await updateReportedState(db, device.id, payload.reported || payload);

    if (updated?.delta && Object.keys(updated.delta).length > 0 && mqttClient?.connected) {
        mqttClient.publish(`climatecloud/device/${deviceUid}/shadow/delta`, JSON.stringify({
            delta: updated.delta,
            version: updated.version,
            timestamp: new Date().toISOString(),
        }), { qos: 1 });
    }

    broadcastToWs({
        type: 'shadow_update',
        deviceUid,
        shadow: { desired: updated?.desired, reported: updated?.reported, delta: updated?.delta },
        timestamp: new Date().toISOString(),
    });
}

// ──── WebSocket Broadcast ────
function broadcastToWs(data) {
    const msg = JSON.stringify(data);
    for (const client of wsClients) {
        try {
            if (client.readyState === 1) client.send(msg);
        } catch (e) { /* ignore */ }
    }
}
