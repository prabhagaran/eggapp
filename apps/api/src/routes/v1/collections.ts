import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { requireMembership } from "../../services/access.js";
import * as collections from "../../services/collection.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const collectionParams = farmParams.extend({ id: z.uuid() });

const createSchema = z.object({
  collectedOn: z.coerce.date(),
  count: z.number().int().positive(),
  avgWeightGrams: z.number().positive().optional(),
  sourceNote: z.string().max(500).optional(),
  clientId: z.uuid().optional(), // offline-sync idempotency (BR-010)
});

const discardSchema = z.object({
  count: z.number().int().positive(),
  reason: z.string().min(1).max(300),
});

export async function collectionRoutes(app: FastifyInstance) {
  // Field-captured record — worker role suffices (BR-013).
  app.post("/farms/:farmId/collections", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    const body = createSchema.parse(req.body);
    const { collection, replay } = await collections.createCollection(farmId, body);
    return reply.code(replay ? 200 : 201).send(collection);
  });

  app.get("/farms/:farmId/collections", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return collections.listCollections(farmId);
  });

  app.post(
    "/farms/:farmId/collections/:id/discard",
    { preHandler: [app.authenticate] },
    async (req) => {
      const { farmId, id } = collectionParams.parse(req.params);
      await requireMembership(req.user.sub, farmId);
      const body = discardSchema.parse(req.body);
      return collections.discardEggs(farmId, id, body.count, body.reason);
    },
  );
}
