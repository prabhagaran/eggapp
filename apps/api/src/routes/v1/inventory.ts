import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { INVENTORY_ITEM_KINDS } from "@eggapp/shared-types";
import { requireMembership } from "../../services/access.js";
import * as inventory from "../../services/inventory.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const itemParams = farmParams.extend({ id: z.uuid() });

const createSchema = z.object({
  kind: z.enum(INVENTORY_ITEM_KINDS),
  name: z.string().min(1).max(120),
  unit: z.string().min(1).max(20),
  quantity: z.number().min(0),
  lotNumber: z.string().max(60).optional(),
  expiry: z.coerce.date().optional(),
  lowStockThreshold: z.number().min(0).optional(),
});

const adjustSchema = z.object({
  delta: z.number().refine((v) => v !== 0, "delta must be non-zero"),
  note: z.string().max(300).optional(),
});

export async function inventoryRoutes(app: FastifyInstance) {
  app.post("/farms/:farmId/inventory", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = createSchema.parse(req.body);
    const item = await inventory.createItem(farmId, body);
    return reply.code(201).send(item);
  });

  app.get("/farms/:farmId/inventory", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return inventory.listItems(farmId);
  });

  app.get("/farms/:farmId/inventory/:id", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = itemParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return inventory.getItem(farmId, id);
  });

  app.post("/farms/:farmId/inventory/:id/adjust", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = itemParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = adjustSchema.parse(req.body);
    return inventory.adjustStock(farmId, id, body.delta, body.note);
  });

  app.delete("/farms/:farmId/inventory/:id", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId, id } = itemParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    await inventory.deleteItem(farmId, id);
    return reply.code(204).send();
  });
}
