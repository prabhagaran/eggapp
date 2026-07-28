// Coop — the physical bird house (ADR 0009). Environmental telemetry binds
// here rather than to Flock: a coop outlives the cohorts that pass through
// it, so per-house history survives a flock ending.
import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

const deviceSummary = {
  select: {
    id: true,
    hardwareId: true,
    name: true,
    status: true,
    lastSeenAt: true,
  },
} as const;

// Only the channels a coop node actually reports. Setpoint/actuator
// columns are deliberately absent — a coop node drives no relays, so
// surfacing those fields here would invite a UI that offers controls the
// hardware cannot honour.
interface CoopTelemetry {
  ts: Date;
  tempC: number | null;
  humidityPct: number | null;
  co2Ppm: number | null;
  ammoniaPpm: number | null;
  lightLux: number | null;
  feedLevelPct: number | null;
  waterLevelPct: number | null;
  source: string;
}

async function attachLatestTelemetry<T extends { deviceId: string | null }>(
  coops: T[],
): Promise<(T & { latestTelemetry: CoopTelemetry | null })[]> {
  const prisma = getPrisma();
  // One small query per bound device — personal-scale coop counts (1-5)
  // make this fine, same reasoning as incubator.service.ts.
  return Promise.all(
    coops.map(async (coop) => {
      if (!coop.deviceId) return { ...coop, latestTelemetry: null };
      const reading = await prisma.telemetryReading.findFirst({
        where: { deviceId: coop.deviceId },
        orderBy: { ts: "desc" },
        select: {
          ts: true,
          tempC: true,
          humidityPct: true,
          co2Ppm: true,
          ammoniaPpm: true,
          lightLux: true,
          feedLevelPct: true,
          waterLevelPct: true,
          source: true,
        },
      });
      return { ...coop, latestTelemetry: reading };
    }),
  );
}

const coopInclude = {
  device: deviceSummary,
  flocks: { select: { id: true, name: true } },
} as const;

export interface CreateCoopInput {
  name: string;
  capacity?: number;
}

export async function createCoop(farmId: string, input: CreateCoopInput) {
  return getPrisma().coop.create({ data: { farmId, ...input } });
}

export async function listCoops(farmId: string) {
  const coops = await getPrisma().coop.findMany({
    where: { farmId },
    include: coopInclude,
    orderBy: { createdAt: "asc" },
  });
  return attachLatestTelemetry(coops);
}

export async function getCoop(farmId: string, id: string) {
  const coop = await getPrisma().coop.findFirst({
    where: { id, farmId },
    include: coopInclude,
  });
  if (!coop) throw new AppError(404, "not_found", "Coop not found");
  const [withTelemetry] = await attachLatestTelemetry([coop]);
  return withTelemetry;
}

export interface UpdateCoopInput {
  name?: string;
  capacity?: number | null;
}

export async function updateCoop(farmId: string, id: string, input: UpdateCoopInput) {
  const prisma = getPrisma();
  const existing = await prisma.coop.findFirst({ where: { id, farmId } });
  if (!existing) throw new AppError(404, "not_found", "Coop not found");
  const updated = await prisma.coop.update({
    where: { id },
    data: { name: input.name, capacity: input.capacity },
    include: coopInclude,
  });
  const [withTelemetry] = await attachLatestTelemetry([updated]);
  return withTelemetry;
}

export async function deleteCoop(farmId: string, id: string) {
  const prisma = getPrisma();
  const existing = await prisma.coop.findFirst({ where: { id, farmId } });
  if (!existing) throw new AppError(404, "not_found", "Coop not found");
  // Flock.coopId is ON DELETE SET NULL — the birds' ledger must never be
  // touched by removing a building record. Flocks simply become unhoused.
  await prisma.coop.delete({ where: { id } });
}

/** Reading history for a coop's bound device, for the environment charts. */
export async function getCoopHistory(farmId: string, id: string, hours: number) {
  const prisma = getPrisma();
  const coop = await prisma.coop.findFirst({ where: { id, farmId } });
  if (!coop) throw new AppError(404, "not_found", "Coop not found");
  if (!coop.deviceId) return { readings: [] };
  const since = new Date(Date.now() - hours * 3600_000);
  const readings = await prisma.telemetryReading.findMany({
    where: { deviceId: coop.deviceId, ts: { gte: since } },
    orderBy: { ts: "asc" },
    select: {
      ts: true,
      tempC: true,
      humidityPct: true,
      co2Ppm: true,
      ammoniaPpm: true,
      lightLux: true,
      feedLevelPct: true,
      waterLevelPct: true,
    },
  });
  return { readings };
}
