import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { requireMembership } from "../../services/access.js";
import * as feedwater from "../../services/feedwater.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const flockParams = farmParams.extend({ flockId: z.uuid() });

const feedSchema = z.object({
  loggedAt: z.coerce.date(),
  feedType: z.string().min(1).max(120),
  quantityKg: z.number().positive(),
  clientId: z.uuid().optional(),
});

const waterSchema = z.object({
  loggedAt: z.coerce.date(),
  quantityLiters: z.number().positive(),
  clientId: z.uuid().optional(),
});

export async function feedWaterRoutes(app: FastifyInstance) {
  app.post("/farms/:farmId/flocks/:flockId/feed-logs", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId, flockId } = flockParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    const body = feedSchema.parse(req.body);
    const { log, replay } = await feedwater.recordFeedLog(farmId, flockId, { ...body, recordedById: req.user.sub });
    return reply.code(replay ? 200 : 201).send(log);
  });

  app.get("/farms/:farmId/flocks/:flockId/feed-logs", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, flockId } = flockParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return feedwater.listFeedLogs(farmId, flockId);
  });

  app.post("/farms/:farmId/flocks/:flockId/water-logs", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId, flockId } = flockParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    const body = waterSchema.parse(req.body);
    const { log, replay } = await feedwater.recordWaterLog(farmId, flockId, { ...body, recordedById: req.user.sub });
    return reply.code(replay ? 200 : 201).send(log);
  });

  app.get("/farms/:farmId/flocks/:flockId/water-logs", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, flockId } = flockParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return feedwater.listWaterLogs(farmId, flockId);
  });
}
