import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { requireMembership } from "../../services/access.js";
import * as incubators from "../../services/incubator.service.js";
import { getIncubatorHistory } from "../../services/telemetry.service.js";
import { getLatestConfig, pushSetpoints } from "../../services/deviceConfig.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const incubatorParams = farmParams.extend({ id: z.uuid() });
const historyQuery = z.object({
  range: z.enum(["24h", "7d", "batch"]),
  batchId: z.uuid().optional(),
});

const setpointSchema = z
  .object({
    tempSetpoint: z.number().optional(),
    tempHysteresis: z.number().optional(),
    humSetpoint: z.number().optional(),
    humHysteresis: z.number().optional(),
  })
  .refine((v) => Object.keys(v).length > 0, { message: "At least one setpoint field is required" });

const createSchema = z.object({
  name: z.string().min(1).max(120),
  capacity: z.number().int().positive(),
  defaultSpeciesId: z.string().min(1).optional(),
});

const updateSchema = z
  .object({
    name: z.string().min(1).max(120).optional(),
    capacity: z.number().int().positive().optional(),
    defaultSpeciesId: z.string().min(1).nullable().optional(),
  })
  .refine((v) => Object.keys(v).length > 0, { message: "At least one field is required" });

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

  app.patch("/farms/:farmId/incubators/:id", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = incubatorParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = updateSchema.parse(req.body);
    return incubators.updateIncubator(farmId, id, body);
  });

  // US-ENV-002
  app.get("/farms/:farmId/incubators/:id/history", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = incubatorParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    const { range, batchId } = historyQuery.parse(req.query);
    return getIncubatorHistory(farmId, id, range, batchId);
  });

  // US-INC-003 — setpoint changes affect real hardware, manager+ only (same bar as creating an incubator).
  app.post("/farms/:farmId/incubators/:id/config", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId, id } = incubatorParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = setpointSchema.parse(req.body);
    const deviceConfig = await pushSetpoints(farmId, id, body);
    return reply.code(201).send(deviceConfig);
  });

  app.get("/farms/:farmId/incubators/:id/config", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = incubatorParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return getLatestConfig(farmId, id);
  });
}
