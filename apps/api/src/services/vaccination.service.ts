import type { FastifyBaseLogger } from "fastify";
import type { FlockPurpose } from "@prisma/client";
import { computeCompliance } from "../domain/vaccination.js";
import { ageDaysFrom, birthDate } from "../domain/flock.js";
import { getPrisma } from "../infra/db.js";
import { sendPush } from "../infra/fcm/client.js";
import { AppError } from "../lib/errors.js";

export interface TemplateItemInput {
  speciesId: string;
  purpose: FlockPurpose;
  ageDaysFrom: number;
  ageDaysTo: number;
  vaccine: string;
  disease: string;
  route: string;
}

export async function listTemplates(farmId: string) {
  return getPrisma().vaccinationTemplateItem.findMany({
    where: { farmId },
    orderBy: [{ speciesId: "asc" }, { purpose: "asc" }, { ageDaysFrom: "asc" }],
  });
}

export async function createTemplateItem(farmId: string, input: TemplateItemInput) {
  const species = await getPrisma().species.findUnique({ where: { id: input.speciesId } });
  if (!species) throw new AppError(400, "unknown_species", `No species '${input.speciesId}'`);
  return getPrisma().vaccinationTemplateItem.create({ data: { farmId, ...input } });
}

// US-VAC-001: starting-point templates seeded from domain-knowledge.md
// §6 (chicken only — the only species with a documented schedule; the
// doc itself flags these as "regional template — confirm with a local
// vet/hatchery," editable per-farm, not hardcoded truth). Called once
// at farm setup; safe to call again (skips if the farm already has any
// chicken template items, so it never duplicates or overwrites edits).
const CHICKEN_LAYER_TEMPLATE = [
  { ageDaysFrom: 1, ageDaysTo: 1, vaccine: "Marek's (HVT)", disease: "Marek's disease", route: "SC injection" },
  { ageDaysFrom: 1, ageDaysTo: 5, vaccine: "ND + IB live", disease: "Newcastle, Infectious bronchitis", route: "Eye drop / spray" },
  { ageDaysFrom: 10, ageDaysTo: 14, vaccine: "IBD live", disease: "Gumboro", route: "Drinking water" },
  { ageDaysFrom: 18, ageDaysTo: 21, vaccine: "IBD booster", disease: "Gumboro", route: "Drinking water" },
  { ageDaysFrom: 21, ageDaysTo: 28, vaccine: "ND booster (LaSota)", disease: "Newcastle", route: "Drinking water" },
  { ageDaysFrom: 42, ageDaysTo: 56, vaccine: "Fowl pox", disease: "Fowl pox", route: "Wing-web stab" },
  { ageDaysFrom: 112, ageDaysTo: 126, vaccine: "ND + IB + EDS inactivated", disease: "pre-lay booster", route: "IM injection" },
] as const;

const CHICKEN_BROILER_TEMPLATE = [
  { ageDaysFrom: 1, ageDaysTo: 1, vaccine: "ND + IB", disease: "Newcastle, Infectious bronchitis", route: "Eye drop / spray" },
  { ageDaysFrom: 10, ageDaysTo: 14, vaccine: "IBD", disease: "Gumboro", route: "Drinking water" },
  { ageDaysFrom: 18, ageDaysTo: 21, vaccine: "ND booster", disease: "Newcastle", route: "Drinking water" },
] as const;

export async function seedDefaultVaccinationTemplates(farmId: string) {
  const prisma = getPrisma();
  const existing = await prisma.vaccinationTemplateItem.findFirst({ where: { farmId, speciesId: "chicken" } });
  if (existing) return;
  await prisma.vaccinationTemplateItem.createMany({
    data: [
      ...CHICKEN_LAYER_TEMPLATE.map((t) => ({ farmId, speciesId: "chicken", purpose: "layer" as const, ...t })),
      ...CHICKEN_BROILER_TEMPLATE.map((t) => ({ farmId, speciesId: "chicken", purpose: "broiler" as const, ...t })),
    ],
  });
}

export async function deleteTemplateItem(farmId: string, id: string) {
  const item = await getPrisma().vaccinationTemplateItem.findFirst({ where: { id, farmId } });
  if (!item) throw new AppError(404, "not_found", "Template item not found");
  await getPrisma().vaccinationTemplateItem.delete({ where: { id } });
}

async function getFlockForCompliance(farmId: string, flockId: string) {
  const flock = await getPrisma().flock.findFirst({
    where: { id: flockId, farmId },
    include: { hatchEvent: true },
  });
  if (!flock) throw new AppError(404, "not_found", "Flock not found");
  const birth = birthDate({
    hatchedAt: flock.hatchEvent?.hatchedAt ?? null,
    acquisitionDate: flock.acquisitionDate,
    acquisitionAgeDays: flock.acquisitionAgeDays,
  });
  return { flock, birth };
}

export async function getCompliance(farmId: string, flockId: string) {
  const { flock, birth } = await getFlockForCompliance(farmId, flockId);
  if (!birth) return [];
  const [templates, records] = await Promise.all([
    getPrisma().vaccinationTemplateItem.findMany({
      where: { farmId, speciesId: flock.speciesId, purpose: flock.purpose },
    }),
    getPrisma().vaccinationRecord.findMany({
      where: { flockId },
      select: { id: true, templateItemId: true, date: true },
    }),
  ]);
  return computeCompliance(birth, templates, records);
}

export interface RecordVaccinationInput {
  templateItemId?: string;
  date: Date;
  vaccine: string;
  disease: string;
  manufacturer?: string;
  lotNumber?: string;
  expiry?: Date;
  route: string;
  dose?: string;
  count: number;
  administeredBy: string;
  reactions?: string;
  nextDueDate?: Date;
  amendsRecordId?: string; // BR-005: correcting an existing record
  clientId?: string;
  recordedById?: string;
}

// BR-005: records are immutable — corrections are new rows. amendsRecordId
// always points at the true original, even when amending an amendment,
// so every record in a chain is one hop from the source (never chase a
// linked list to find what actually happened).
export async function recordVaccination(farmId: string, flockId: string, input: RecordVaccinationInput) {
  return getPrisma().$transaction(async (tx) => {
    const flock = await tx.flock.findFirst({ where: { id: flockId, farmId } });
    if (!flock) throw new AppError(404, "not_found", "Flock not found");

    if (input.clientId) {
      const existing = await tx.vaccinationRecord.findUnique({ where: { clientId: input.clientId } });
      if (existing) {
        if (existing.flockId !== flockId) throw new AppError(409, "conflict", "clientId already used");
        return { record: existing, replay: true };
      }
    }

    let amendsRecordId = input.amendsRecordId;
    if (amendsRecordId) {
      const original = await tx.vaccinationRecord.findFirst({ where: { id: amendsRecordId, flockId } });
      if (!original) throw new AppError(404, "not_found", "Original record to amend not found");
      // Resolve to the true original, not the immediate target, if the
      // target is itself already an amendment.
      amendsRecordId = original.amendsRecordId ?? original.id;
    }

    const record = await tx.vaccinationRecord.create({
      data: {
        flockId,
        templateItemId: input.templateItemId,
        date: input.date,
        vaccine: input.vaccine,
        disease: input.disease,
        manufacturer: input.manufacturer,
        lotNumber: input.lotNumber,
        expiry: input.expiry,
        route: input.route,
        dose: input.dose,
        count: input.count,
        administeredBy: input.administeredBy,
        reactions: input.reactions,
        nextDueDate: input.nextDueDate,
        amendsRecordId,
        clientId: input.clientId,
        recordedById: input.recordedById,
      },
    });
    return { record, replay: false };
  });
}

// US-VAC-002: periodic sweep (mirrors checkDeviceSilence/checkUnconfirmedConfigs).
// One push per due/overdue template item per flock, deduped via a
// Notification row so re-running the sweep doesn't re-spam.
export async function checkVaccinationDue(log: FastifyBaseLogger) {
  const prisma = getPrisma();
  const flocks = await prisma.flock.findMany({ include: { hatchEvent: true } });

  for (const flock of flocks) {
    const birth = birthDate({
      hatchedAt: flock.hatchEvent?.hatchedAt ?? null,
      acquisitionDate: flock.acquisitionDate,
      acquisitionAgeDays: flock.acquisitionAgeDays,
    });
    if (!birth) continue;

    const [templates, records] = await Promise.all([
      prisma.vaccinationTemplateItem.findMany({
        where: { farmId: flock.farmId, speciesId: flock.speciesId, purpose: flock.purpose },
      }),
      prisma.vaccinationRecord.findMany({ where: { flockId: flock.id }, select: { id: true, templateItemId: true, date: true } }),
    ]);
    const compliance = computeCompliance(birth, templates, records);

    for (const item of compliance) {
      if (item.status !== "due" && item.status !== "overdue") continue;
      const alreadyNotified = await prisma.notification.findFirst({
        where: { kind: "vaccination_due", deepLink: `eggapp://flocks/${flock.id}/vaccination/${item.id}` },
      });
      if (alreadyNotified) continue;

      const members = await prisma.farmMembership.findMany({
        where: { farmId: flock.farmId, user: { fcmToken: { not: null } } },
        include: { user: { select: { id: true, fcmToken: true } } },
      });
      const title = item.status === "overdue" ? "⚠️ Vaccination overdue" : "Vaccination due";
      const body = `${flock.name}: ${item.vaccine} (${item.disease})`;
      const deepLink = `eggapp://flocks/${flock.id}/vaccination/${item.id}`;
      await Promise.all(members.map((m) => sendPush(log, m.user.fcmToken!, { title, body, deepLink })));
      await Promise.all(
        members.map((m) =>
          prisma.notification.create({
            data: { userId: m.user.id, kind: "vaccination_due", title, body, deepLink, channel: "fcm", sentAt: new Date() },
          }),
        ),
      );
    }
  }
}
