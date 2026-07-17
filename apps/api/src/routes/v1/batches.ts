import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { requireMembership } from "../../services/access.js";
import * as batches from "../../services/batch.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const batchParams = farmParams.extend({ id: z.uuid() });

const count = z.number().int().min(0);

const createSchema = z.object({
  incubatorId: z.uuid(),
  speciesId: z.string().min(1),
  sources: z.array(z.object({ collectionId: z.uuid(), count: z.number().int().positive() })).min(1),
  overrideNote: z.string().max(300).optional(), // BR-011
});

const setSchema = z.object({ setAt: z.coerce.date().optional() });

const statusSchema = z.object({
  status: z.enum(["setting", "lockdown", "hatching", "aborted", "closed"]),
  reason: z.string().max(300).optional(),
});

const candlingSchema = z.object({
  dayNo: z.number().int().positive(),
  candledAt: z.coerce.date(),
  fertile: count,
  clear: count,
  bloodRing: count,
  unsure: count,
  discrepancyNote: z.string().max(500).optional(),
  clientId: z.uuid().optional(),
});

const hatchSchema = z.object({
  hatchedAt: z.coerce.date(),
  hatched: count,
  pippedDead: count,
  deadInShell: count,
  unhatched: count,
  discrepancyNote: z.string().max(500).optional(),
  clientId: z.uuid().optional(),
});

const listQuery = z.object({
  status: z
    .enum(["planned", "setting", "incubating", "lockdown", "hatching", "completed", "closed", "aborted"])
    .optional(),
});

export async function batchRoutes(app: FastifyInstance) {
  // Planning — manager+ (Web surface per PRD)
  app.post("/farms/:farmId/batches", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = createSchema.parse(req.body);
    const result = await batches.createBatch(farmId, body);
    return reply.code(201).send(result);
  });

  app.get("/farms/:farmId/batches", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    const { status } = listQuery.parse(req.query);
    return batches.listBatches(farmId, status);
  });

  app.get("/farms/:farmId/batches/:id", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = batchParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return batches.getBatch(farmId, id);
  });

  app.post("/farms/:farmId/batches/:id/set", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = batchParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = setSchema.parse(req.body ?? {});
    return batches.setBatch(farmId, id, body.setAt);
  });

  app.post("/farms/:farmId/batches/:id/status", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = batchParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = statusSchema.parse(req.body);
    return batches.transitionBatch(farmId, id, body.status, body.reason);
  });

  // Field captures — worker role suffices (BR-013)
  app.post(
    "/farms/:farmId/batches/:id/candlings",
    { preHandler: [app.authenticate] },
    async (req, reply) => {
      const { farmId, id } = batchParams.parse(req.params);
      await requireMembership(req.user.sub, farmId);
      const body = candlingSchema.parse(req.body);
      const { session, batch, replay } = await batches.recordCandling(farmId, id, {
        ...body,
        recordedById: req.user.sub,
      });
      return reply.code(replay ? 200 : 201).send({ session, batch });
    },
  );

  app.post(
    "/farms/:farmId/batches/:id/hatch",
    { preHandler: [app.authenticate] },
    async (req, reply) => {
      const { farmId, id } = batchParams.parse(req.params);
      await requireMembership(req.user.sub, farmId);
      const body = hatchSchema.parse(req.body);
      const { event, batch, replay } = await batches.recordHatch(farmId, id, {
        ...body,
        recordedById: req.user.sub,
      });
      return reply.code(replay ? 200 : 201).send({ event, batch });
    },
  );
}
