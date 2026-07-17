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

export async function listIncubators(farmId: string) {
  return getPrisma().incubator.findMany({
    where: { farmId },
    include: { device: deviceSummary },
    orderBy: { createdAt: "asc" },
  });
}

export async function getIncubator(farmId: string, id: string) {
  const incubator = await getPrisma().incubator.findFirst({
    where: { id, farmId },
    include: { device: deviceSummary, defaultSpecies: true },
  });
  if (!incubator) throw new AppError(404, "not_found", "Incubator not found");
  return incubator;
}
