import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { requireMembership } from "../../services/access.js";
import * as coops from "../../services/coop.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const coopParams = farmParams.extend({ id: z.uuid() });
const historyQuery = z.object({ range: z.enum(["24h", "7d"]) });

const createSchema = z.object({
  name: z.string().min(1).max(120),
  capacity: z.number().int().positive().optional(),
});

const updateSchema = z
  .object({
    name: z.string().min(1).max(120).optional(),
    capacity: z.number().int().positive().nullable().optional(),
  })
  .refine((v) => Object.keys(v).length > 0, { message: "At least one field is required" });

export async function coopRoutes(app: FastifyInstance) {
  app.post("/farms/:farmId/coops", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = createSchema.parse(req.body);
    const coop = await coops.createCoop(farmId, body);
    return reply.code(201).send(coop);
  });

  app.get("/farms/:farmId/coops", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return coops.listCoops(farmId);
  });

  app.get("/farms/:farmId/coops/:id", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = coopParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return coops.getCoop(farmId, id);
  });

  app.patch("/farms/:farmId/coops/:id", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = coopParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = updateSchema.parse(req.body);
    return coops.updateCoop(farmId, id, body);
  });

  // Deleting a building must never touch the birds' ledger — Flock.coopId
  // is ON DELETE SET NULL, so housed flocks simply become unhoused.
  app.delete("/farms/:farmId/coops/:id", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId, id } = coopParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    await coops.deleteCoop(farmId, id);
    return reply.code(204).send();
  });

  app.get("/farms/:farmId/coops/:id/history", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = coopParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    const { range } = historyQuery.parse(req.query);
    return coops.getCoopHistory(farmId, id, range === "24h" ? 24 : 24 * 7);
  });
}
