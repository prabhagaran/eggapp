import type { FastifyInstance } from "fastify";
import { getPrisma } from "../../infra/db.js";

export async function farmRoutes(app: FastifyInstance) {
  app.get("/farms", { preHandler: [app.authenticate] }, async (req) => {
    const memberships = await getPrisma().farmMembership.findMany({
      where: { userId: req.user.sub },
      include: { farm: true },
    });
    return memberships.map((m) => ({
      id: m.farm.id,
      name: m.farm.name,
      timezone: m.farm.timezone,
      location: m.farm.location,
      role: m.role,
    }));
  });
}
