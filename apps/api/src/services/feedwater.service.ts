import type { FastifyBaseLogger } from "fastify";
import { ageDaysFrom, birthDate, deriveStage, expectedFeedKeyword } from "../domain/flock.js";
import { getPrisma } from "../infra/db.js";
import { sendPush } from "../infra/fcm/client.js";
import { AppError } from "../lib/errors.js";
import { deductStock } from "./inventory.service.js";

// US-FED-003/US-WTR-002: >20% drop vs the flock's 7-day trailing average,
// sustained across 2 consecutive logs. The "spike alongside falling feed
// in hot weather" half of US-WTR-002 needs ambient weather data this app
// doesn't have any source for (flocks aren't tied to a telemetry device);
// only the sustained-drop illness signal is implemented for both feed
// and water — noted here rather than silently dropped.
const ANOMALY_DROP_THRESHOLD = 0.2;
const TREND_WINDOW_DAYS = 7;

async function getFarmFlock(farmId: string, flockId: string) {
  const flock = await getPrisma().flock.findFirst({ where: { id: flockId, farmId }, include: { hatchEvent: true } });
  if (!flock) throw new AppError(404, "not_found", "Flock not found");
  return flock;
}

export interface FeedLogInput {
  loggedAt: Date;
  feedType: string;
  quantityKg: number;
  inventoryItemId?: string;
  clientId?: string;
  recordedById?: string;
}

export async function recordFeedLog(farmId: string, flockId: string, input: FeedLogInput) {
  const prisma = getPrisma();
  const flock = await getFarmFlock(farmId, flockId);

  if (input.clientId) {
    const existing = await prisma.feedLog.findUnique({ where: { clientId: input.clientId } });
    if (existing) {
      if (existing.flockId !== flockId) throw new AppError(409, "conflict", "clientId already used");
      return { log: existing, replay: true };
    }
  }

  // US-FED-002: non-blocking stage-mismatch warning.
  const birth = birthDate({
    hatchedAt: flock.hatchEvent?.hatchedAt ?? null,
    acquisitionDate: flock.acquisitionDate,
    acquisitionAgeDays: flock.acquisitionAgeDays,
  });
  let stageMismatch = false;
  if (birth) {
    const stage = deriveStage(flock.purpose, ageDaysFrom(birth));
    const expected = expectedFeedKeyword(stage);
    stageMismatch = !input.feedType.toLowerCase().includes(expected);
  }

  const log = await prisma.$transaction(async (tx) => {
    const log = await tx.feedLog.create({
      data: {
        flockId,
        loggedAt: input.loggedAt,
        feedType: input.feedType,
        quantityKg: input.quantityKg,
        stageMismatch,
        inventoryItemId: input.inventoryItemId,
        clientId: input.clientId,
        recordedById: input.recordedById,
      },
    });
    // US-INV-002: a log never exists without its stock deduction landing too.
    if (input.inventoryItemId) {
      await deductStock(tx, farmId, input.inventoryItemId, -input.quantityKg, "feed_log", log.id);
    }
    return log;
  });
  return { log, replay: false };
}

export interface WaterLogInput {
  loggedAt: Date;
  quantityLiters: number;
  clientId?: string;
  recordedById?: string;
}

export async function recordWaterLog(farmId: string, flockId: string, input: WaterLogInput) {
  const prisma = getPrisma();
  await getFarmFlock(farmId, flockId);

  if (input.clientId) {
    const existing = await prisma.waterLog.findUnique({ where: { clientId: input.clientId } });
    if (existing) {
      if (existing.flockId !== flockId) throw new AppError(409, "conflict", "clientId already used");
      return { log: existing, replay: true };
    }
  }

  const log = await prisma.waterLog.create({
    data: {
      flockId,
      loggedAt: input.loggedAt,
      quantityLiters: input.quantityLiters,
      clientId: input.clientId,
      recordedById: input.recordedById,
    },
  });
  return { log, replay: false };
}

export async function listFeedLogs(farmId: string, flockId: string) {
  await getFarmFlock(farmId, flockId);
  return getPrisma().feedLog.findMany({ where: { flockId }, orderBy: { loggedAt: "desc" }, take: 100 });
}

export async function listWaterLogs(farmId: string, flockId: string) {
  await getFarmFlock(farmId, flockId);
  return getPrisma().waterLog.findMany({ where: { flockId }, orderBy: { loggedAt: "desc" }, take: 100 });
}

// US-FED-003/US-WTR-002 — periodic sweep (same pattern as
// checkDeviceSilence). Trailing-average comparison needs the last N
// logs anyway, so this recomputes rather than tracking running state —
// simplest correct thing at personal scale (a handful of flocks).
async function checkAnomalyFor(
  log: FastifyBaseLogger,
  kind: "feed" | "water",
  flockId: string,
  farmId: string,
  flockName: string,
) {
  const prisma = getPrisma();
  const rows =
    kind === "feed"
      ? await prisma.feedLog.findMany({ where: { flockId }, orderBy: { loggedAt: "desc" }, take: 20 })
      : await prisma.waterLog.findMany({ where: { flockId }, orderBy: { loggedAt: "desc" }, take: 20 });
  if (rows.length < 3) return; // need at least 2 recent + some trailing history

  const quantity = (r: (typeof rows)[number]) => ("quantityKg" in r ? r.quantityKg : r.quantityLiters);
  const cutoff = new Date(Date.now() - TREND_WINDOW_DAYS * 86_400_000);
  const trailing = rows.slice(2).filter((r) => r.loggedAt >= cutoff);
  if (trailing.length === 0) return;
  const avg = trailing.reduce((sum, r) => sum + quantity(r), 0) / trailing.length;
  if (avg <= 0) return;

  const isDrop = (r: (typeof rows)[number]) => quantity(r) < avg * (1 - ANOMALY_DROP_THRESHOLD);
  if (!isDrop(rows[0]!) || !isDrop(rows[1]!)) return;

  const messageKey = `[${kind}_anomaly]`;
  const openExisting = await prisma.alert.findFirst({
    where: { farmId, flockId, state: "open", message: { startsWith: messageKey } },
  });
  if (openExisting) return;

  const label = kind === "feed" ? "Feed" : "Water";
  const message = `${messageKey} ${flockName}: ${label.toLowerCase()} consumption dropped >20% below its 7-day average across the last 2 checks`;
  await prisma.alert.create({ data: { farmId, flockId, severity: "warning", state: "open", message } });

  const members = await prisma.farmMembership.findMany({
    where: { farmId, user: { fcmToken: { not: null } } },
    include: { user: { select: { fcmToken: true } } },
  });
  await Promise.all(
    members.map((m) =>
      sendPush(log, m.user.fcmToken!, {
        title: `⚠️ ${label} drop — ${flockName}`,
        body: `Possible early illness signal (domain-knowledge §5)`,
        deepLink: `eggapp://flocks/${flockId}`,
      }),
    ),
  );
}

export async function checkConsumptionAnomalies(log: FastifyBaseLogger) {
  const flocks = await getPrisma().flock.findMany({ select: { id: true, farmId: true, name: true } });
  for (const flock of flocks) {
    await checkAnomalyFor(log, "feed", flock.id, flock.farmId, flock.name);
    await checkAnomalyFor(log, "water", flock.id, flock.farmId, flock.name);
  }
}
