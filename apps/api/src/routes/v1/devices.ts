import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { requireMembership } from "../../services/access.js";
import * as devices from "../../services/device.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const deviceParams = farmParams.extend({ id: z.uuid() });

const registerSchema = z.object({
  hardwareId: z.string().min(1).max(64),
  name: z.string().max(120).optional(),
});

// A device binds to exactly one of an incubator or a coop (ADR 0009).
// Modelled as a union so "neither" and "both" are rejected by the schema
// rather than needing a runtime check in the handler.
const bindSchema = z.union([
  z.object({ incubatorId: z.uuid(), coopId: z.undefined().optional() }),
  z.object({ coopId: z.uuid(), incubatorId: z.undefined().optional() }),
]);

export async function deviceRoutes(app: FastifyInstance) {
  app.post("/farms/:farmId/devices", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = registerSchema.parse(req.body);
    const device = await devices.registerDevice(farmId, body);
    return reply.code(201).send(device);
  });

  app.get("/farms/:farmId/devices", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return devices.listDevices(farmId);
  });

  app.post("/farms/:farmId/devices/:id/bind", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = deviceParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    const body = bindSchema.parse(req.body);
    return body.incubatorId
      ? devices.bindDevice(farmId, id, body.incubatorId)
      : devices.bindDeviceToCoop(farmId, id, body.coopId!);
  });

  app.post("/farms/:farmId/devices/:id/unbind", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = deviceParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    return devices.unbindDevice(farmId, id);
  });

  app.post(
    "/farms/:farmId/devices/:id/decommission",
    { preHandler: [app.authenticate] },
    async (req) => {
      const { farmId, id } = deviceParams.parse(req.params);
      await requireMembership(req.user.sub, farmId, "manager");
      return devices.decommissionDevice(farmId, id);
    },
  );
}
