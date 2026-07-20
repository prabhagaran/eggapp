import type { BatchStatus, Prisma } from "@prisma/client";
import type { FastifyBaseLogger } from "fastify";
import {
  assertHatchDateAllowed,
  assertTransition,
  candlingOutcome,
  type CandlingCounts,
  classifyStorageAge,
  computeSchedule,
  fertilityPct,
  type HatchCounts,
  hatchDiscrepancy,
  hatchMetrics,
} from "../domain/incubation.js";
import { pushSetpoints } from "./deviceConfig.service.js";
import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

async function getFarmBatch(tx: Prisma.TransactionClient, farmId: string, id: string) {
  const batch = await tx.eggBatch.findFirst({ where: { id, farmId } });
  if (!batch) throw new AppError(404, "not_found", "Batch not found");
  return batch;
}

async function setCount(tx: Prisma.TransactionClient, batchId: string) {
  const agg = await tx.batchEggSource.aggregate({ _sum: { count: true }, where: { batchId } });
  return agg._sum.count ?? 0;
}

export interface CreateBatchInput {
  incubatorId: string;
  speciesId: string;
  sources: { collectionId: string; count: number }[];
  overrideNote?: string;
}

export async function createBatch(farmId: string, input: CreateBatchInput, now = new Date()) {
  return getPrisma().$transaction(async (tx) => {
    const incubator = await tx.incubator.findFirst({ where: { id: input.incubatorId, farmId } });
    if (!incubator) throw new AppError(404, "not_found", "Incubator not found");
    const species = await tx.species.findUnique({ where: { id: input.speciesId } });
    if (!species) throw new AppError(400, "unknown_species", `No species '${input.speciesId}'`);

    const warnings: string[] = [];
    let total = 0;
    for (const source of input.sources) {
      const collection = await tx.eggCollection.findFirst({
        where: { id: source.collectionId, farmId },
        include: { batchSources: { select: { count: true } } },
      });
      if (!collection) throw new AppError(404, "not_found", `Collection ${source.collectionId} not found`);
      const assigned = collection.batchSources.reduce((sum, s) => sum + s.count, 0);
      const available = collection.count - collection.discardedCount - assigned;
      if (source.count > available) {
        throw new AppError(409, "insufficient_eggs", `Collection has only ${available} eggs available`);
      }
      // BR-011
      const age = classifyStorageAge(collection.collectedOn, now);
      if (age.verdict === "blocked" && !input.overrideNote) {
        throw new AppError(
          400,
          "eggs_too_old",
          `Eggs are ${age.ageDays} days old (>14); provide overrideNote to set anyway`,
        );
      }
      if (age.verdict !== "ok") {
        warnings.push(`Collection ${collection.id}: eggs ${age.ageDays} days old`);
      }
      total += source.count;
    }

    const batch = await tx.eggBatch.create({
      data: {
        farmId,
        incubatorId: input.incubatorId,
        speciesId: input.speciesId,
        candlingDays: species.candlingDays, // BR-008: snapshot at creation
        viableCount: total,
        storageOverrideNote: input.overrideNote,
      },
    });
    await tx.batchEggSource.createMany({
      data: input.sources.map((s) => ({ batchId: batch.id, ...s })),
    });
    return { batch, warnings };
  });
}

export interface UpdateBatchInput {
  incubatorId?: string;
  speciesId?: string;
  sources?: { collectionId: string; count: number }[];
  overrideNote?: string;
}

// "planned" and "incubating" batches are editable. "incubating" is allowed
// so a mis-set incubator/species/source can be corrected while still live;
// when species changes on an incubating batch we re-snapshot candlingDays
// (BR-008) and recompute lockdownAt/expectedHatchAt off the existing setAt
// below, since those were computed from the old species. Anything past
// incubating (lockdown+) has candling/hatch data already recorded against
// the schedule and is not editable.
const EDITABLE_STATUSES: BatchStatus[] = ["planned", "incubating"];

export async function updateBatch(farmId: string, id: string, input: UpdateBatchInput, now = new Date()) {
  return getPrisma().$transaction(async (tx) => {
    const batch = await getFarmBatch(tx, farmId, id);
    if (!EDITABLE_STATUSES.includes(batch.status)) {
      throw new AppError(409, "not_editable", `Only 'planned' or 'incubating' batches can be edited (this one is '${batch.status}')`);
    }

    if (input.incubatorId) {
      const incubator = await tx.incubator.findFirst({ where: { id: input.incubatorId, farmId } });
      if (!incubator) throw new AppError(404, "not_found", "Incubator not found");
    }

    let speciesId = batch.speciesId;
    let candlingDays = batch.candlingDays;
    let lockdownAt = batch.lockdownAt;
    let expectedHatchAt = batch.expectedHatchAt;
    if (input.speciesId && input.speciesId !== batch.speciesId) {
      const species = await tx.species.findUnique({ where: { id: input.speciesId } });
      if (!species) throw new AppError(400, "unknown_species", `No species '${input.speciesId}'`);
      speciesId = species.id;
      candlingDays = species.candlingDays; // BR-008: re-snapshot since species changed
      if (batch.status === "incubating" && batch.setAt) {
        const schedule = computeSchedule(species, batch.setAt);
        lockdownAt = schedule.lockdownAt;
        expectedHatchAt = schedule.expectedHatchAt;
      }
    }

    const warnings: string[] = [];
    let viableCount = batch.viableCount;
    if (input.sources) {
      if (input.sources.length === 0) {
        throw new AppError(400, "sources_required", "A batch needs at least one egg source");
      }
      // Release this batch's own current sources first so re-validating
      // "available" below doesn't double-count them against themselves
      // (e.g. lowering — or even just re-submitting — a count on the
      // same collection this batch already draws from).
      await tx.batchEggSource.deleteMany({ where: { batchId: id } });

      let total = 0;
      for (const source of input.sources) {
        const collection = await tx.eggCollection.findFirst({
          where: { id: source.collectionId, farmId },
          include: { batchSources: { select: { count: true } } },
        });
        if (!collection) throw new AppError(404, "not_found", `Collection ${source.collectionId} not found`);
        const assigned = collection.batchSources.reduce((sum, s) => sum + s.count, 0);
        const available = collection.count - collection.discardedCount - assigned;
        if (source.count > available) {
          throw new AppError(409, "insufficient_eggs", `Collection has only ${available} eggs available`);
        }
        // BR-011
        const age = classifyStorageAge(collection.collectedOn, now);
        if (age.verdict === "blocked" && !input.overrideNote && !batch.storageOverrideNote) {
          throw new AppError(
            400,
            "eggs_too_old",
            `Eggs are ${age.ageDays} days old (>14); provide overrideNote to set anyway`,
          );
        }
        if (age.verdict !== "ok") {
          warnings.push(`Collection ${collection.id}: eggs ${age.ageDays} days old`);
        }
        total += source.count;
      }
      await tx.batchEggSource.createMany({
        data: input.sources.map((s) => ({ batchId: id, ...s })),
      });
      viableCount = total;
    }

    const updated = await tx.eggBatch.update({
      where: { id },
      data: {
        ...(input.incubatorId ? { incubatorId: input.incubatorId } : {}),
        speciesId,
        candlingDays,
        lockdownAt,
        expectedHatchAt,
        viableCount,
        storageOverrideNote: input.overrideNote ?? batch.storageOverrideNote,
      },
    });
    return { batch: updated, warnings };
  });
}

// "planned" (never happened) and "aborted" (terminal, no hardware using it)
// are safe to hard-delete. "incubating" is deliberately excluded — eggs are
// physically in a device; use the /status abort transition instead of
// deleting so there's a record of what happened to them.
const DELETABLE_STATUSES: BatchStatus[] = ["planned", "aborted"];

export async function deleteBatch(farmId: string, id: string) {
  return getPrisma().$transaction(async (tx) => {
    const batch = await getFarmBatch(tx, farmId, id);
    if (!DELETABLE_STATUSES.includes(batch.status)) {
      throw new AppError(409, "not_deletable", `Only 'planned' or 'aborted' batches can be deleted (this one is '${batch.status}')`);
    }
    // BatchEggSource rows cascade-delete (schema onDelete: Cascade),
    // releasing their egg counts back to each collection's available
    // pool automatically since "available" is computed, not stored.
    await tx.eggBatch.delete({ where: { id } });
  });
}

// planned/setting → incubating: eggs are physically in; schedule dates fix here.
export async function setBatch(farmId: string, id: string, setAt = new Date(), log?: FastifyBaseLogger) {
  const updated = await getPrisma().$transaction(async (tx) => {
    const batch = await getFarmBatch(tx, farmId, id);
    assertTransition(batch.status, "incubating");
    const species = await tx.species.findUniqueOrThrow({ where: { id: batch.speciesId } });
    const schedule = computeSchedule(species, setAt);
    return tx.eggBatch.update({
      where: { id },
      data: {
        status: "incubating",
        setAt,
        lockdownAt: schedule.lockdownAt,
        expectedHatchAt: schedule.expectedHatchAt,
      },
    });
  });

  // Best-effort: push the start date to the device's own day counter too
  // (see task_mqtt.cpp's startEpoch command), so "set eggs" from the app
  // doesn't leave the physical incubator's local clock unset/stale. Not
  // part of the transaction above and never fails the batch transition —
  // an incubator with no bound device, or a broker that's unreachable, is
  // routine (not every incubator has MQTT wired up), and pushSetpoints
  // already routes delivery failure into the existing unconfirmed-config
  // alert path once a device *is* bound.
  try {
    await pushSetpoints(farmId, updated.incubatorId, { startEpoch: Math.floor(setAt.getTime() / 1000) });
  } catch (err) {
    log?.info({ err, batchId: id, incubatorId: updated.incubatorId }, "[batch] startEpoch device push skipped");
  }

  return updated;
}

// Manual lifecycle moves (BR-001). `completed` only via recordHatch.
export async function transitionBatch(farmId: string, id: string, to: BatchStatus, reason?: string) {
  if (to === "completed") {
    throw new AppError(409, "use_hatch_endpoint", "Record a hatch outcome to complete a batch");
  }
  if (to === "incubating") {
    throw new AppError(409, "use_set_endpoint", "Use /set to start incubation (computes the schedule)");
  }
  if (to === "aborted" && !reason) {
    throw new AppError(400, "reason_required", "Aborting a batch requires a reason");
  }
  return getPrisma().$transaction(async (tx) => {
    const batch = await getFarmBatch(tx, farmId, id);
    assertTransition(batch.status, to);
    return tx.eggBatch.update({
      where: { id },
      data: { status: to, abortReason: to === "aborted" ? reason : batch.abortReason },
    });
  });
}

export interface RecordCandlingInput extends CandlingCounts {
  dayNo: number;
  candledAt: Date;
  discrepancyNote?: string;
  clientId?: string;
  recordedById?: string;
}

export async function recordCandling(farmId: string, batchId: string, input: RecordCandlingInput) {
  return getPrisma().$transaction(async (tx) => {
    const batch = await getFarmBatch(tx, farmId, batchId);
    if (input.clientId) {
      const existing = await tx.candlingSession.findUnique({ where: { clientId: input.clientId } });
      if (existing) {
        if (existing.batchId !== batchId) throw new AppError(409, "conflict", "clientId already used");
        return { session: existing, batch, replay: true };
      }
    }
    if (batch.status !== "incubating") {
      throw new AppError(409, "not_incubating", "Candling can only be recorded while incubating (BR-002)");
    }
    const outcome = candlingOutcome(input, batch.viableCount);
    if (outcome.discrepancy !== 0 && !input.discrepancyNote) {
      throw new AppError(
        400,
        "discrepancy_note_required",
        `Counts sum to ${batch.viableCount + outcome.discrepancy} but current viable count is ${batch.viableCount}; add discrepancyNote (BR-003)`,
      );
    }
    const priorSessions = await tx.candlingSession.count({ where: { batchId } });
    const session = await tx.candlingSession.create({
      data: {
        batchId,
        dayNo: input.dayNo,
        candledAt: input.candledAt,
        fertile: input.fertile,
        clear: input.clear,
        bloodRing: input.bloodRing,
        unsure: input.unsure,
        discrepancyNote: input.discrepancyNote,
        recordedById: input.recordedById,
        clientId: input.clientId,
      },
    });
    const updated = await tx.eggBatch.update({
      where: { id: batchId },
      data: {
        viableCount: outcome.newViable,
        // BR-015: fertility fixes at the first candling session
        ...(priorSessions === 0
          ? { fertilityPct: fertilityPct(input.fertile, await setCount(tx, batchId)) }
          : {}),
      },
    });
    return { session, batch: updated, replay: false };
  });
}

export interface RecordHatchInput extends HatchCounts {
  hatchedAt: Date;
  discrepancyNote?: string;
  clientId?: string;
  recordedById?: string;
}

export async function recordHatch(farmId: string, batchId: string, input: RecordHatchInput) {
  return getPrisma().$transaction(async (tx) => {
    const batch = await getFarmBatch(tx, farmId, batchId);
    if (input.clientId) {
      const existing = await tx.hatchEvent.findUnique({ where: { clientId: input.clientId } });
      if (existing) {
        if (existing.batchId !== batchId) throw new AppError(409, "conflict", "clientId already used");
        return { event: existing, batch, replay: true };
      }
    }
    if (batch.status !== "lockdown" && batch.status !== "hatching") {
      throw new AppError(409, "invalid_transition", `Cannot record hatch for a '${batch.status}' batch`);
    }
    if (batch.expectedHatchAt) assertHatchDateAllowed(batch.expectedHatchAt, input.hatchedAt); // BR-008
    const discrepancy = hatchDiscrepancy(input, batch.viableCount);
    if (discrepancy !== 0 && !input.discrepancyNote) {
      throw new AppError(
        400,
        "discrepancy_note_required",
        `Counts must reconcile against viable count ${batch.viableCount}; add discrepancyNote (BR-003)`,
      );
    }
    const event = await tx.hatchEvent.create({
      data: {
        batchId,
        hatchedAt: input.hatchedAt,
        hatched: input.hatched,
        pippedDead: input.pippedDead,
        deadInShell: input.deadInShell,
        unhatched: input.unhatched,
        discrepancyNote: input.discrepancyNote,
        recordedById: input.recordedById,
        clientId: input.clientId,
      },
    });
    const firstCandling = await tx.candlingSession.findFirst({
      where: { batchId },
      orderBy: { dayNo: "asc" },
    });
    const metrics = hatchMetrics(
      await setCount(tx, batchId),
      firstCandling ? firstCandling.fertile : null,
      input.hatched,
    );
    const updated = await tx.eggBatch.update({
      where: { id: batchId },
      data: { status: "completed", ...metrics }, // BR-015: stored, never recomputed
    });
    return { event, batch: updated, replay: false };
  });
}

export async function listBatches(farmId: string, status?: BatchStatus) {
  return getPrisma().eggBatch.findMany({
    where: { farmId, ...(status ? { status } : {}) },
    include: {
      species: { select: { id: true, name: true, incubationDays: true } },
      incubator: { select: { id: true, name: true } },
      // Phase 2 — lets the Flocks page offer "create from this hatch",
      // and hide hatches that already became a flock.
      hatch: { select: { id: true, flock: { select: { id: true } } } },
    },
    orderBy: { createdAt: "desc" },
  });
}

export async function getBatch(farmId: string, id: string) {
  const batch = await getPrisma().eggBatch.findFirst({
    where: { id, farmId },
    include: {
      species: true,
      incubator: { select: { id: true, name: true } },
      sources: { include: { collection: true } },
      candlings: { orderBy: { dayNo: "asc" } },
      hatch: true,
    },
  });
  if (!batch) throw new AppError(404, "not_found", "Batch not found");
  return batch;
}
