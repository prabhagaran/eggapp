import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { requireMembership } from "../../services/access.js";
import * as incubators from "../../services/incubator.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const incubatorParams = farmParams.extend({ id: z.uuid() });

const createSchema = z.object({
  name: z.string().min(1).max(120),
  capacity: z.number().int().positive(),
  defaultSpeciesId: z.string().min(1).optional(),
});

export async function incubatorRoutes(app: FastifyInstance) {
  app.post("/farms/:farmId/incubators", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = createSchema.parse(req.body);
    const incubator = await incubators.createIncubator(farmId, body);
    return reply.code(201).send(incubator);
  });

  app.get("/farms/:farmId/incubators", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return incubators.listIncubators(farmId);
  });

  app.get("/farms/:farmId/incubators/:id", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = incubatorParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return incubators.getIncubator(farmId, id);
  });
}
