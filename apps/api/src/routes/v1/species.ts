import type { FastifyInstance } from "fastify";
import { getPrisma } from "../../infra/db.js";

export async function speciesRoutes(app: FastifyInstance) {
  app.get("/species", { preHandler: [app.authenticate] }, async () => {
    return getPrisma().species.findMany({ orderBy: { name: "asc" } });
  });
}
