import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { requireMembership } from "../../services/access.js";
import * as members from "../../services/members.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const memberParams = farmParams.extend({ userId: z.uuid() });

const inviteSchema = z.object({
  email: z.email(),
  role: z.enum(["manager", "worker"]),
  name: z.string().max(120).optional(),
});

export async function memberRoutes(app: FastifyInstance) {
  app.get("/farms/:farmId/members", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "manager");
    return members.listMembers(farmId);
  });

  // BR-013: user/farm admin is Owner-only.
  app.post("/farms/:farmId/members", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "owner");
    const body = inviteSchema.parse(req.body);
    const result = await members.inviteMember(farmId, body);
    return reply.code(201).send(result);
  });

  app.delete("/farms/:farmId/members/:userId", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId, userId } = memberParams.parse(req.params);
    await requireMembership(req.user.sub, farmId, "owner");
    await members.removeMember(farmId, userId, req.user.sub);
    return reply.code(204).send();
  });
}
