import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

export interface CreateCollectionInput {
  collectedOn: Date;
  count: number;
  avgWeightGrams?: number;
  sourceNote?: string;
  clientId?: string;
}

// BR-010: replaying the same clientId returns the original record instead
// of creating a duplicate (offline queue retries are safe).
export async function createCollection(farmId: string, input: CreateCollectionInput) {
  const prisma = getPrisma();
  if (input.clientId) {
    const existing = await prisma.eggCollection.findUnique({ where: { clientId: input.clientId } });
    if (existing) {
      if (existing.farmId !== farmId) throw new AppError(409, "conflict", "clientId already used");
      return { collection: existing, replay: true };
    }
  }
  const collection = await prisma.eggCollection.create({ data: { farmId, ...input } });
  return { collection, replay: false };
}

export async function listCollections(farmId: string) {
  const collections = await getPrisma().eggCollection.findMany({
    where: { farmId },
    include: { batchSources: { select: { count: true, batchId: true } } },
    orderBy: { collectedOn: "desc" },
  });
  return collections.map(({ batchSources, ...c }) => {
    const assignedCount = batchSources.reduce((sum, s) => sum + s.count, 0);
    return { ...c, assignedCount, availableCount: c.count - c.discardedCount - assignedCount };
  });
}

export async function discardEggs(farmId: string, id: string, count: number, reason: string) {
  return getPrisma().$transaction(async (tx) => {
    const collection = await tx.eggCollection.findFirst({
      where: { id, farmId },
      include: { batchSources: { select: { count: true } } },
    });
    if (!collection) throw new AppError(404, "not_found", "Collection not found");
    const assigned = collection.batchSources.reduce((sum, s) => sum + s.count, 0);
    const available = collection.count - collection.discardedCount - assigned;
    if (count > available) {
      throw new AppError(409, "insufficient_eggs", `Only ${available} eggs available to discard`);
    }
    const stamp = `${new Date().toISOString().slice(0, 10)}: ${count} — ${reason}`;
    return tx.eggCollection.update({
      where: { id },
      data: {
        discardedCount: { increment: count },
        discardNote: collection.discardNote ? `${collection.discardNote}\n${stamp}` : stamp,
      },
    });
  });
}
