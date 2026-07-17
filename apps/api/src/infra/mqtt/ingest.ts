// MQTT ingest adapter — inbound adapter per docs/iot/mqtt-topics.md and
// telemetry-contract.md. Calls the same Prisma models the REST layer uses;
// no business rules live here beyond what the contract docs specify.
import { randomUUID } from "node:crypto";
import mqtt from "mqtt";
import type { FastifyBaseLogger } from "fastify";
import { z } from "zod";
import { config } from "../../config.js";
import { getPrisma } from "../db.js";

const TOPIC_TELEMETRY = "eggapp/devices/+/telemetry";
const TOPIC_STATUS = "eggapp/devices/+/status";

// Matches task_mqtt.cpp's payload exactly (telemetry-contract.md).
// EGG-only fields are optional since CLIMATE-profile payloads omit them.
const telemetrySchema = z.object({
  id: z.string(),
  profile: z.enum(["EGG", "CLIMATE"]),
  temp: z.number().nullable(),
  hum: z.number().nullable(),
  turner: z.union([z.literal(0), z.literal(1)]),
});

function parseTopic(topic: string): { deviceId: string; kind: "telemetry" | "status" } | null {
  const parts = topic.split("/");
  if (parts.length !== 4 || parts[0] !== "eggapp" || parts[1] !== "devices") return null;
  const kind = parts[3];
  if (kind !== "telemetry" && kind !== "status") return null;
  return { deviceId: parts[2]!, kind };
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
    await tx.device.update({ where: { id: device.id }, data: { lastSeenAt: new Date() } });
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
  });
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

  client.on("connect", () => {
    log.info({ url: config.mqttUrl }, "[mqtt] connected");
    client.subscribe([TOPIC_TELEMETRY, TOPIC_STATUS], (err) => {
      if (err) log.error({ err }, "[mqtt] subscribe failed");
    });
  });

  client.on("message", (topic, payload) => {
    const parsed = parseTopic(topic);
    if (!parsed) {
      log.warn({ topic }, "[mqtt] message on unrecognized topic shape");
      return;
    }
    const handler = parsed.kind === "telemetry" ? handleTelemetry : handleStatus;
    handler(log, parsed.deviceId, payload).catch((err) =>
      log.error({ err, topic }, "[mqtt] handler failed"),
    );
  });

  client.on("error", (err) => log.error({ err }, "[mqtt] client error"));

  return client;
}
