import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

const deviceSummary = {
  select: { id: true, hardwareId: true, name: true, status: true, lastSeenAt: true },
} as const;

export interface CreateIncubatorInput {
  name: string;
  capacity: number;
  defaultSpeciesId?: string;
}

export async function createIncubator(farmId: string, input: CreateIncubatorInput) {
  const prisma = getPrisma();
  if (input.defaultSpeciesId) {
    const species = await prisma.species.findUnique({ where: { id: input.defaultSpeciesId } });
    if (!species) throw new AppError(400, "unknown_species", `No species '${input.defaultSpeciesId}'`);
  }
  return prisma.incubator.create({ data: { farmId, ...input } });
}

// US-INC-002/ENV-001: live status must be ≤60s old. Latest reading is
// fetched per bound device — personal-scale incubator counts (1-3) make
// N small queries perfectly fine; no need for a window-function query.
async function attachLatestTelemetry<T extends { deviceId: string | null }>(
  incubators: T[],
): Promise<(T & { latestTelemetry: LatestTelemetry | null })[]> {
  const prisma = getPrisma();
  return Promise.all(
    incubators.map(async (inc) => {
      if (!inc.deviceId) return { ...inc, latestTelemetry: null };
      const reading = await prisma.telemetryReading.findFirst({
        where: { deviceId: inc.deviceId },
        orderBy: { ts: "desc" },
        select: { ts: true, tempC: true, humidityPct: true, turnerOn: true, source: true },
      });
      return { ...inc, latestTelemetry: reading };
    }),
  );
}

interface LatestTelemetry {
  ts: Date;
  tempC: number | null;
  humidityPct: number | null;
  turnerOn: boolean | null;
  source: string;
}

export async function listIncubators(farmId: string) {
  const incubators = await getPrisma().incubator.findMany({
    where: { farmId },
    include: { device: deviceSummary },
    orderBy: { createdAt: "asc" },
  });
  return attachLatestTelemetry(incubators);
}

export async function getIncubator(farmId: string, id: string) {
  const incubator = await getPrisma().incubator.findFirst({
    where: { id, farmId },
    include: { device: deviceSummary, defaultSpecies: true },
  });
  if (!incubator) throw new AppError(404, "not_found", "Incubator not found");
  const [withTelemetry] = await attachLatestTelemetry([incubator]);
  return withTelemetry;
}
