import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { requireMembership } from "../../services/access.js";
import { ackAlert, listAlerts } from "../../services/alert.service.js";

const farmParams = z.object({ farmId: z.uuid() });
const alertParams = farmParams.extend({ id: z.uuid() });
const listQuery = z.object({ state: z.enum(["open", "acked", "resolved"]).optional() });

export async function alertRoutes(app: FastifyInstance) {
  app.get("/farms/:farmId/alerts", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    const { state } = listQuery.parse(req.query);
    return listAlerts(farmId, state);
  });

  app.post("/farms/:farmId/alerts/:id/ack", { preHandler: [app.authenticate] }, async (req) => {
    const { farmId, id } = alertParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    return ackAlert(farmId, id, req.user.sub);
  });
}
