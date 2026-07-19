import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { FLOCK_PURPOSES } from "@eggapp/shared-types";
import { requireMembership } from "../../services/access.js";
import * as vaccination from "../../services/vaccination.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const templateParams = farmParams.extend({ id: z.uuid() });
const flockParams = farmParams.extend({ flockId: z.uuid() });

const templateItemSchema = z.object({
  speciesId: z.string().min(1),
  purpose: z.enum(FLOCK_PURPOSES),
  ageDaysFrom: z.number().int().min(0),
  ageDaysTo: z.number().int().min(0),
  vaccine: z.string().min(1).max(120),
  disease: z.string().min(1).max(120),
  route: z.string().min(1).max(60),
});

const updateTemplateItemSchema = templateItemSchema
  .partial()
  .refine((v) => Object.keys(v).length > 0, { message: "At least one field is required" });

const recordSchema = z.object({
  templateItemId: z.uuid().optional(),
  date: z.coerce.date(),
  vaccine: z.string().min(1).max(120),
  disease: z.string().min(1).max(120),
  manufacturer: z.string().max(120).optional(),
  lotNumber: z.string().max(60).optional(),
  expiry: z.coerce.date().optional(),
  route: z.string().min(1).max(60),
  dose: z.string().max(60).optional(),
  count: z.number().int().positive(),
  administeredBy: z.string().min(1).max(120),
  reactions: z.string().max(300).optional(),
  nextDueDate: z.coerce.date().optional(),
  amendsRecordId: z.uuid().optional(),
  inventoryItemId: z.uuid().optional(),
  clientId: z.uuid().optional(),
});

export async function vaccinationRoutes(app: FastifyInstance) {
  // Backfill/reset for a farm with no templates yet — no-ops if the farm
  // already has any (never overwrites edits). Manager+, same bar as edits.
  app.post("/farms/:farmId/vaccination-templates/seed-defaults", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    await vaccination.seedDefaultVaccinationTemplates(farmId);
    return reply.code(204).send();
  });

  // US-VAC-001 — manager+, same bar as editing incubators.
  app.post("/farms/:farmId/vaccination-templates", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = templateItemSchema.parse(req.body);
    const item = await vaccination.createTemplateItem(farmId, body);
    return reply.code(201).send(item);
  });

  app.get("/farms/:farmId/vaccination-templates", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return vaccination.listTemplates(farmId);
  });

  app.patch("/farms/:farmId/vaccination-templates/:id", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = templateParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = updateTemplateItemSchema.parse(req.body);
    return vaccination.updateTemplateItem(farmId, id, body);
  });

  app.delete("/farms/:farmId/vaccination-templates/:id", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId, id } = templateParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    await vaccination.deleteTemplateItem(farmId, id);
    return reply.code(204).send();
  });

  app.get(
    "/farms/:farmId/flocks/:flockId/vaccination/compliance",
    { preHandler: [app.authenticate] },
    async (req) => {
      const { farmId, flockId } = flockParams.parse(req.params);
      await requireMembership(req.user.sub, farmId);
      return vaccination.getCompliance(farmId, flockId);
    },
  );

  // Field record (worker role suffices, same bar as candling/mortality).
  app.post("/farms/:farmId/flocks/:flockId/vaccination", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId, flockId } = flockParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    const body = recordSchema.parse(req.body);
    const { record, replay } = await vaccination.recordVaccination(farmId, flockId, {
      ...body,
      recordedById: req.user.sub,
    });
    return reply.code(replay ? 200 : 201).send(record);
  });
}
