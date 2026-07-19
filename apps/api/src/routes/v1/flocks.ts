import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { FLOCK_PURPOSES, FLOCK_STAGES, MORTALITY_CAUSES } from "@eggapp/shared-types";
import { requireMembership } from "../../services/access.js";
import * as flocks from "../../services/flock.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const flockParams = farmParams.extend({ id: z.uuid() });

const createSchema = z
  .object({
    name: z.string().min(1).max(120),
    speciesId: z.string().min(1),
    purpose: z.enum(FLOCK_PURPOSES),
    placedCount: z.number().int().positive(),
    hatchEventId: z.uuid().optional(),
    acquisitionDate: z.coerce.date().optional(),
    acquisitionAgeDays: z.number().int().min(0).optional(),
    acquisitionNote: z.string().max(300).optional(),
  })
  .refine((v) => Boolean(v.hatchEventId) !== (v.acquisitionDate != null && v.acquisitionAgeDays != null), {
    message: "Provide exactly one of hatchEventId or acquisitionDate+acquisitionAgeDays",
  });

const updateSchema = z
  .object({
    name: z.string().min(1).max(120).optional(),
    speciesId: z.string().min(1).optional(),
    purpose: z.enum(FLOCK_PURPOSES).optional(),
    acquisitionNote: z.string().max(300).optional(),
    // null clears the override back to auto-derive
    stageOverride: z.enum(FLOCK_STAGES).nullable().optional(),
  })
  .refine((v) => Object.keys(v).length > 0, { message: "At least one field is required" });

const mortalitySchema = z.object({
  date: z.coerce.date(),
  count: z.number().int().positive(),
  cause: z.enum(MORTALITY_CAUSES),
  notes: z.string().max(300).optional(),
  clientId: z.uuid().optional(),
});

export async function flockRoutes(app: FastifyInstance) {
  app.post("/farms/:farmId/flocks", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = createSchema.parse(req.body);
    const flock = await flocks.createFlock(farmId, body);
    return reply.code(201).send(flock);
  });

  app.get("/farms/:farmId/flocks", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return flocks.listFlocks(farmId);
  });

  app.get("/farms/:farmId/flocks/:id", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = flockParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return flocks.getFlock(farmId, id);
  });

  app.patch("/farms/:farmId/flocks/:id", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = flockParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = updateSchema.parse(req.body);
    return flocks.updateFlock(farmId, id, body);
  });

  app.delete("/farms/:farmId/flocks/:id", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId, id } = flockParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    await flocks.deleteFlock(farmId, id);
    return reply.code(204).send();
  });

  // Field record (worker role suffices, same bar as candling/collections).
  app.post("/farms/:farmId/flocks/:id/mortality", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId, id } = flockParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    const body = mortalitySchema.parse(req.body);
    const { record, replay } = await flocks.recordMortality(farmId, id, {
      ...body,
      recordedById: req.user.sub,
    });
    return reply.code(replay ? 200 : 201).send(record);
  });
}
