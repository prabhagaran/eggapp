// US-INC-003: setpoint config push with sent → received → applied ack
// states. See docs/iot/mqtt-topics.md for the wire contract.
import type { Prisma } from "@prisma/client";
import type { FastifyBaseLogger } from "fastify";
import { publishCommand } from "../infra/mqtt/ingest.js";
import { getPrisma } from "../infra/db.js";
import { sendPush } from "../infra/fcm/client.js";
import { AppError } from "../lib/errors.js";

export interface SetpointInput {
  tempSetpoint?: number;
  tempHysteresis?: number;
  humSetpoint?: number;
  humHysteresis?: number;
  // Unix seconds. Pushes the app's incubation start date to the device's
  // own day counter (see docs/iot/mqtt-topics.md "startEpoch") — routed
  // through the same DeviceConfig/ack/unconfirmed-alert pipeline as
  // setpoints since the wire mechanics (version, ack states, 2-minute
  // timeout) are identical, not because it's conceptually a setpoint.
  startEpoch?: number;
}

// No ack at all within this window means something's wrong (device
// offline, message dropped) — matches US-INC-003's given/when.
const UNCONFIRMED_TIMEOUT_MS = 2 * 60_000;

export async function pushSetpoints(farmId: string, incubatorId: string, input: SetpointInput) {
  const prisma = getPrisma();
  const incubator = await prisma.incubator.findFirst({
    where: { id: incubatorId, farmId },
    include: { device: true },
  });
  if (!incubator) throw new AppError(404, "not_found", "Incubator not found");
  if (!incubator.device) throw new AppError(400, "no_device", "Incubator has no bound device");

  const latest = await prisma.deviceConfig.findFirst({
    where: { deviceId: incubator.device.id },
    orderBy: { version: "desc" },
  });
  const version = (latest?.version ?? 0) + 1;

  const deviceConfig = await prisma.$transaction(async (tx) => {
    const created = await tx.deviceConfig.create({
      data: { deviceId: incubator.device!.id, version, payload: input as Prisma.InputJsonObject, state: "sent" },
    });
    await tx.deviceEvent.create({ data: { deviceId: incubator.device!.id, type: "config_sent" } });
    return created;
  });

  const sent = publishCommand(incubator.device.hardwareId, { version, ...input });
  if (!sent) {
    // Not fatal — the row stays "sent" and the unconfirmed-timeout job
    // will flag it same as any other undelivered command.
    throw new AppError(503, "mqtt_unavailable", "Broker not connected — command queued but not yet sent");
  }

  return deviceConfig;
}

export async function getLatestConfig(farmId: string, incubatorId: string) {
  const prisma = getPrisma();
  const incubator = await prisma.incubator.findFirst({ where: { id: incubatorId, farmId }, include: { device: true } });
  if (!incubator) throw new AppError(404, "not_found", "Incubator not found");
  if (!incubator.device) return null;
  return prisma.deviceConfig.findFirst({
    where: { deviceId: incubator.device.id },
    orderBy: { version: "desc" },
  });
}

export async function checkUnconfirmedConfigs(log: FastifyBaseLogger) {
  const prisma = getPrisma();
  const staleCutoff = new Date(Date.now() - UNCONFIRMED_TIMEOUT_MS);
  const stale = await prisma.deviceConfig.findMany({
    where: { state: { in: ["sent", "received"] }, sentAt: { lt: staleCutoff } },
    include: { device: { select: { farmId: true, hardwareId: true, name: true } } },
  });

  for (const cfg of stale) {
    await prisma.deviceConfig.update({ where: { id: cfg.id }, data: { state: "unconfirmed" } });
    const label = cfg.device.name ?? cfg.device.hardwareId;
    const members = await prisma.farmMembership.findMany({
      where: { farmId: cfg.device.farmId, user: { fcmToken: { not: null } } },
      include: { user: { select: { fcmToken: true } } },
    });
    await Promise.all(
      members.map((m) =>
        sendPush(log, m.user.fcmToken!, {
          title: "⚠️ Config not confirmed",
          body: `${label} hasn't acknowledged setpoint change v${cfg.version}`,
          deepLink: "eggapp://incubators",
        }),
      ),
    );
  }
}
