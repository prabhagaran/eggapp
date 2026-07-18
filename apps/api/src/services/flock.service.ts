import type { FlockPurpose } from "@prisma/client";
import { ageDaysFrom, birthDate, deriveStage } from "../domain/flock.js";
import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

export interface CreateFlockInput {
  name: string;
  speciesId: string;
  purpose: FlockPurpose;
  placedCount: number;
  // Exactly one origin (US-FLK-001, BR-009 traceability): a hatch event,
  // or a manual acquisition with age + provenance note.
  hatchEventId?: string;
  acquisitionDate?: Date;
  acquisitionAgeDays?: number;
  acquisitionNote?: string;
}

function withDerived<T extends { purpose: FlockPurpose; placedCount: number; hatchEvent: { hatchedAt: Date } | null; acquisitionDate: Date | null; acquisitionAgeDays: number | null; mortalityRecords: { count: number }[] }>(
  flock: T,
) {
  const birth = birthDate({
    hatchedAt: flock.hatchEvent?.hatchedAt ?? null,
    acquisitionDate: flock.acquisitionDate,
    acquisitionAgeDays: flock.acquisitionAgeDays,
  });
  const ageDays = birth ? ageDaysFrom(birth) : null;
  const stage = ageDays != null ? deriveStage(flock.purpose, ageDays) : null;
  const lost = flock.mortalityRecords.reduce((sum, r) => sum + r.count, 0);
  const currentCount = flock.placedCount - lost; // BR-009: the only ledger, no direct edits
  const { mortalityRecords: _mortalityRecords, ...rest } = flock;
  return { ...rest, ageDays, stage, currentCount };
}

export async function createFlock(farmId: string, input: CreateFlockInput) {
  const prisma = getPrisma();
  const species = await prisma.species.findUnique({ where: { id: input.speciesId } });
  if (!species) throw new AppError(400, "unknown_species", `No species '${input.speciesId}'`);

  if (input.hatchEventId) {
    const hatch = await prisma.hatchEvent.findFirst({
      where: { id: input.hatchEventId, batch: { farmId } },
      include: { flock: true },
    });
    if (!hatch) throw new AppError(404, "not_found", "Hatch event not found");
    if (hatch.flock) throw new AppError(409, "already_linked", "This hatch already has a flock");
  } else if (!input.acquisitionDate || input.acquisitionAgeDays == null) {
    throw new AppError(
      400,
      "origin_required",
      "A flock needs either a hatchEventId or acquisitionDate + acquisitionAgeDays (BR-009 traceability)",
    );
  }

  const flock = await prisma.flock.create({
    data: {
      farmId,
      name: input.name,
      speciesId: input.speciesId,
      purpose: input.purpose,
      placedCount: input.placedCount,
      hatchEventId: input.hatchEventId,
      acquisitionDate: input.acquisitionDate,
      acquisitionAgeDays: input.acquisitionAgeDays,
      acquisitionNote: input.acquisitionNote,
    },
    include: { hatchEvent: true, mortalityRecords: { select: { count: true } } },
  });
  return withDerived(flock);
}

export async function listFlocks(farmId: string) {
  const flocks = await getPrisma().flock.findMany({
    where: { farmId },
    include: { hatchEvent: true, mortalityRecords: { select: { count: true } }, species: true },
    orderBy: { createdAt: "desc" },
  });
  return flocks.map(withDerived);
}

async function getFarmFlock(farmId: string, id: string) {
  const flock = await getPrisma().flock.findFirst({
    where: { id, farmId },
    include: {
      hatchEvent: { include: { batch: { select: { id: true, incubatorId: true } } } },
      mortalityRecords: { orderBy: { date: "desc" } },
      species: true,
    },
  });
  if (!flock) throw new AppError(404, "not_found", "Flock not found");
  return flock;
}

export async function getFlock(farmId: string, id: string) {
  const flock = await getFarmFlock(farmId, id);
  const [vaccinationRecords, recentFeed, recentWater] = await Promise.all([
    getPrisma().vaccinationRecord.findMany({ where: { flockId: id }, orderBy: { date: "desc" } }),
    getPrisma().feedLog.findMany({ where: { flockId: id }, orderBy: { loggedAt: "desc" }, take: 10 }),
    getPrisma().waterLog.findMany({ where: { flockId: id }, orderBy: { loggedAt: "desc" }, take: 10 }),
  ]);
  return {
    ...withDerived(flock),
    mortalityRecords: flock.mortalityRecords,
    vaccinationRecords,
    recentFeed,
    recentWater,
  };
}

export interface RecordMortalityInput {
  date: Date;
  count: number;
  cause: "death" | "cull" | "sale";
  notes?: string;
  clientId?: string;
  recordedById?: string;
}

export async function recordMortality(farmId: string, flockId: string, input: RecordMortalityInput) {
  return getPrisma().$transaction(async (tx) => {
    const flock = await tx.flock.findFirst({
      where: { id: flockId, farmId },
      include: { mortalityRecords: { select: { count: true } } },
    });
    if (!flock) throw new AppError(404, "not_found", "Flock not found");

    if (input.clientId) {
      const existing = await tx.mortalityRecord.findUnique({ where: { clientId: input.clientId } });
      if (existing) {
        if (existing.flockId !== flockId) throw new AppError(409, "conflict", "clientId already used");
        return { record: existing, replay: true };
      }
    }

    const lost = flock.mortalityRecords.reduce((sum, r) => sum + r.count, 0);
    const currentCount = flock.placedCount - lost;
    if (input.count > currentCount) {
      throw new AppError(409, "exceeds_count", `Only ${currentCount} birds remain in this flock`);
    }

    const record = await tx.mortalityRecord.create({
      data: {
        flockId,
        date: input.date,
        count: input.count,
        cause: input.cause,
        notes: input.notes,
        clientId: input.clientId,
        recordedById: input.recordedById,
      },
    });
    return { record, replay: false };
  });
}
