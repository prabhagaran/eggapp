// MQTT ingest adapter — inbound adapter per docs/iot/mqtt-topics.md and
// telemetry-contract.md. Calls the same Prisma models the REST layer uses;
// no business rules live here beyond what the contract docs specify.
import { randomUUID } from "node:crypto";
import mqtt from "mqtt";
import type { FastifyBaseLogger } from "fastify";
import { z } from "zod";
import { config } from "../../config.js";
import { evaluateTelemetry } from "../../services/alert.service.js";
import { getPrisma } from "../db.js";

const TOPIC_TELEMETRY = "eggapp/devices/+/telemetry";
const TOPIC_STATUS = "eggapp/devices/+/status";
const TOPIC_CMD_ACK = "eggapp/devices/+/cmd/ack";

// Set once by startMqttIngest — lets services publish commands (US-INC-003)
// without each one standing up its own MQTT connection. Personal-scale,
// single-process deployment; no need for a pub/sub abstraction layer.
let sharedClient: mqtt.MqttClient | null = null;

const cmdAckSchema = z.object({
  version: z.number().int(),
  state: z.enum(["received", "applied"]),
});

// Matches task_mqtt.cpp's payload exactly (telemetry-contract.md).
// EGG-only fields are optional since CLIMATE-profile payloads omit them.
// setTemp/setHum/setTempHyst/setHumHyst are optional too — older firmware
// (pre US-INC-003) won't send the hysteresis fields.
const telemetrySchema = z.object({
  id: z.string(),
  profile: z.enum(["EGG", "CLIMATE"]),
  temp: z.number().nullable(),
  hum: z.number().nullable(),
  turner: z.union([z.literal(0), z.literal(1)]),
  setTemp: z.number().optional(),
  setHum: z.number().optional(),
  setTempHyst: z.number().optional(),
  setHumHyst: z.number().optional(),
});

function parseTopic(topic: string): { deviceId: string; kind: "telemetry" | "status" | "cmd_ack" } | null {
  const parts = topic.split("/");
  if (parts[0] !== "eggapp" || parts[1] !== "devices") return null;
  const deviceId = parts[2];
  if (!deviceId) return null;
  if (parts.length === 4 && (parts[3] === "telemetry" || parts[3] === "status")) {
    return { deviceId, kind: parts[3] };
  }
  if (parts.length === 5 && parts[3] === "cmd" && parts[4] === "ack") {
    return { deviceId, kind: "cmd_ack" };
  }
  return null;
}

async function handleTelemetry(log: FastifyBaseLogger, deviceId: string, raw: Buffer) {
  const parsed = telemetrySchema.safeParse(JSON.parse(raw.toString()));
  if (!parsed.success) {
    log.warn({ deviceId, issues: parsed.error.issues }, "[mqtt] malformed telemetry payload");
    return;
  }
  const prisma = getPrisma();
  // BR-007: unmatched device id is logged and dropped, never auto-created.
  const device = await prisma.device.findUnique({ where: { hardwareId: deviceId } });
  if (!device) {
    log.warn({ deviceId }, "[mqtt] telemetry from unregistered device — dropped");
    return;
  }
  await prisma.$transaction(async (tx) => {
    await tx.telemetryReading.create({
      data: {
        deviceId: device.id,
        ts: new Date(),
        tempC: parsed.data.temp,
        humidityPct: parsed.data.hum,
        turnerOn: parsed.data.turner === 1,
        source: "mqtt",
      },
    });
    await tx.device.update({
      where: { id: device.id },
      data: {
        lastSeenAt: new Date(),
        // US-INC-003: snapshot of the device's current control-loop
        // values — only touched when the field is actually present, so
        // firmware that hasn't sent it yet doesn't overwrite a value
        // with null.
        ...(parsed.data.setTemp != null ? { currentTempSetpoint: parsed.data.setTemp } : {}),
        ...(parsed.data.setTempHyst != null ? { currentTempHysteresis: parsed.data.setTempHyst } : {}),
        ...(parsed.data.setHum != null ? { currentHumSetpoint: parsed.data.setHum } : {}),
        ...(parsed.data.setHumHyst != null ? { currentHumHysteresis: parsed.data.setHumHyst } : {}),
      },
    });
    // Live telemetry is itself proof-of-life: promote status even if the
    // one-off retained `status` message was missed (e.g. it arrived
    // before this device was registered — observed in practice, not
    // hypothetical). The status topic remains authoritative for the
    // reverse direction (active -> offline is LWT-only, never inferred
    // from a telemetry gap — see device-lifecycle.md).
    if (device.status !== "active") {
      await tx.device.update({ where: { id: device.id }, data: { status: "active" } });
      await tx.deviceEvent.create({ data: { deviceId: device.id, type: "online" } });
    }
    // US-ENV-004: telemetry resuming resolves any open device-silence fault
    // — the thing it was flagging (no readings) is no longer true.
    await tx.alert.updateMany({
      where: { deviceId: device.id, state: "open", message: { startsWith: "[device_silence]" } },
      data: { state: "resolved", resolvedAt: new Date() },
    });
  });

  // Outside the write transaction — alert evaluation does its own reads/
  // writes and a failure here must never roll back the telemetry write
  // that already succeeded.
  try {
    await evaluateTelemetry(log, device.id, { tempC: parsed.data.temp, humidityPct: parsed.data.hum });
  } catch (err) {
    log.error({ err, deviceId }, "[mqtt] alert evaluation failed");
  }
}

async function handleStatus(log: FastifyBaseLogger, deviceId: string, raw: Buffer) {
  const value = raw.toString().trim();
  if (value !== "online" && value !== "offline") {
    log.warn({ deviceId, value }, "[mqtt] unexpected status payload");
    return;
  }
  const prisma = getPrisma();
  const device = await prisma.device.findUnique({ where: { hardwareId: deviceId } });
  if (!device) {
    log.warn({ deviceId, value }, "[mqtt] status from unregistered device — dropped");
    return;
  }
  // Idempotent: a retained/duplicate status message is a no-op if state
  // already matches, still cheap enough not to bother checking first.
  await prisma.$transaction([
    prisma.device.update({
      where: { id: device.id },
      data: { status: value === "online" ? "active" : "offline" },
    }),
    prisma.deviceEvent.create({ data: { deviceId: device.id, type: value } }),
  ]);
  log.info({ deviceId, value }, "[mqtt] device status updated");
}

async function handleCmdAck(log: FastifyBaseLogger, deviceId: string, raw: Buffer) {
  const parsed = cmdAckSchema.safeParse(JSON.parse(raw.toString()));
  if (!parsed.success) {
    log.warn({ deviceId, issues: parsed.error.issues }, "[mqtt] malformed cmd ack");
    return;
  }
  const prisma = getPrisma();
  const device = await prisma.device.findUnique({ where: { hardwareId: deviceId } });
  if (!device) {
    log.warn({ deviceId }, "[mqtt] cmd ack from unregistered device — dropped");
    return;
  }
  const deviceConfig = await prisma.deviceConfig.findUnique({
    where: { deviceId_version: { deviceId: device.id, version: parsed.data.version } },
  });
  if (!deviceConfig) {
    log.warn({ deviceId, version: parsed.data.version }, "[mqtt] cmd ack for unknown config version — dropped");
    return;
  }

  const now = new Date();
  // The "received" and "applied" acks for one command typically arrive
  // milliseconds apart, and the message handler below doesn't await —
  // two handleCmdAck calls for the same version can run concurrently.
  // Without a guard, a "received" ack whose write commits after
  // "applied" would silently downgrade a config that's already in its
  // terminal state. The WHERE clause makes the guard atomic at the SQL
  // level (Postgres row-level locking during the UPDATE), so it holds
  // regardless of how the two async handlers happen to interleave.
  if (parsed.data.state === "applied") {
    await prisma.$transaction([
      prisma.deviceConfig.updateMany({
        where: { id: deviceConfig.id },
        data: { state: "applied", appliedAt: now, receivedAt: deviceConfig.receivedAt ?? now },
      }),
      prisma.deviceEvent.create({ data: { deviceId: device.id, type: "config_applied" } }),
    ]);
  } else {
    const result = await prisma.deviceConfig.updateMany({
      where: { id: deviceConfig.id, state: { not: "applied" } },
      data: { state: "received", receivedAt: now },
    });
    if (result.count > 0) {
      await prisma.deviceEvent.create({ data: { deviceId: device.id, type: "config_received" } });
    }
  }
  log.info({ deviceId, version: parsed.data.version, state: parsed.data.state }, "[mqtt] config ack");
}

/** Publishes a setpoint command to a device's cmd topic (US-INC-003). Returns false if MQTT isn't connected. */
export function publishCommand(hardwareId: string, payload: object): boolean {
  if (!sharedClient?.connected) return false;
  const topic = `eggapp/devices/${hardwareId}/cmd`;
  sharedClient.publish(topic, JSON.stringify(payload), { qos: 1 });
  return true;
}

/** Starts the MQTT ingest client. No-op (returns null) if MQTT_URL is unset. */
export function startMqttIngest(log: FastifyBaseLogger): mqtt.MqttClient | null {
  if (!config.mqttUrl) {
    log.info("[mqtt] MQTT_URL not set — ingest disabled");
    return null;
  }

  const client = mqtt.connect(config.mqttUrl, {
    clientId: `eggapp-api-${randomUUID().slice(0, 8)}`,
    username: config.mqttUsername || undefined,
    password: config.mqttPassword || undefined,
    reconnectPeriod: 5000,
  });
  sharedClient = client;

  client.on("connect", () => {
    log.info({ url: config.mqttUrl }, "[mqtt] connected");
    client.subscribe([TOPIC_TELEMETRY, TOPIC_STATUS, TOPIC_CMD_ACK], (err) => {
      if (err) log.error({ err }, "[mqtt] subscribe failed");
    });
  });

  client.on("message", (topic, payload) => {
    const parsed = parseTopic(topic);
    if (!parsed) {
      log.warn({ topic }, "[mqtt] message on unrecognized topic shape");
      return;
    }
    const handler =
      parsed.kind === "telemetry" ? handleTelemetry : parsed.kind === "status" ? handleStatus : handleCmdAck;
    handler(log, parsed.deviceId, payload).catch((err) =>
      log.error({ err, topic }, "[mqtt] handler failed"),
    );
  });

  client.on("error", (err) => log.error({ err }, "[mqtt] client error"));

  return client;
}
