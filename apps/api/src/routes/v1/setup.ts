import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { getPrisma } from "../../infra/db.js";
import { setupFirstRun } from "../../services/auth.service.js";

const setupSchema = z.object({
  ownerName: z.string().min(1).max(120),
  email: z.email().transform((s) => s.toLowerCase()),
  password: z.string().min(8).max(200),
  farmName: z.string().min(1).max(120),
  timezone: z.string().min(1).max(64), // IANA name, e.g. "Asia/Kolkata"
  location: z.string().max(200).optional(),
});

export async function setupRoutes(app: FastifyInstance) {
  // US-FRM-001 — unauthenticated by design; refuses once any user exists.
  app.post("/setup", async (req, reply) => {
    const body = setupSchema.parse(req.body);
    const result = await setupFirstRun(app, body);
    return reply.code(201).send(result);
  });

  // Lets clients route between the setup wizard and login on first load.
  app.get("/setup/status", async () => {
    return { initialized: (await getPrisma().user.count()) > 0 };
  });
}
