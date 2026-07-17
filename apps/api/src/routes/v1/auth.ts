import type { FastifyInstance } from "fastify";
import { z } from "zod";
import * as auth from "../../services/auth.service.js";

const loginSchema = z.object({
  email: z.email().transform((s) => s.toLowerCase()),
  password: z.string().min(1),
});

const refreshSchema = z.object({ refreshToken: z.string().min(1) });
const pushTokenSchema = z.object({ fcmToken: z.string().min(1) });

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

  // US-NOT-002: registers/refreshes this user's current device token.
  // One token per user (personal scale, single phone) — a fresh login or
  // FCM token rotation just overwrites it.
  app.post("/me/push-token", { preHandler: [app.authenticate] }, async (req, reply) => {
    const body = pushTokenSchema.parse(req.body);
    await auth.setPushToken(req.user.sub, body.fcmToken);
    return reply.code(204).send();
  });
}
