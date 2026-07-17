import type { FastifyInstance } from "fastify";
import { z } from "zod";
import * as auth from "../../services/auth.service.js";

const loginSchema = z.object({
  email: z.email().transform((s) => s.toLowerCase()),
  password: z.string().min(1),
});

const refreshSchema = z.object({ refreshToken: z.string().min(1) });

export async function authRoutes(app: FastifyInstance) {
  app.post("/auth/login", async (req) => {
    const body = loginSchema.parse(req.body);
    return auth.login(app, body.email, body.password);
  });

  app.post("/auth/refresh", async (req) => {
    const body = refreshSchema.parse(req.body);
    return auth.refresh(app, body.refreshToken);
  });

  app.post("/auth/logout-all", { preHandler: [app.authenticate] }, async (req, reply) => {
    await auth.logoutAll(req.user.sub);
    return reply.code(204).send();
  });

  app.get("/me", { preHandler: [app.authenticate] }, async (req) => {
    return auth.me(req.user.sub);
  });
}
