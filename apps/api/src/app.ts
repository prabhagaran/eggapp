import cors from "@fastify/cors";
import { Prisma } from "@prisma/client";
import Fastify, { type FastifyError } from "fastify";
import { ZodError } from "zod";
import { AppError } from "./lib/errors.js";
import { authPlugin } from "./plugins/auth.js";
import { healthRoutes } from "./routes/health.js";
import { v1Routes } from "./routes/v1/index.js";

export function buildApp() {
  const app = Fastify({
    logger: { level: process.env.LOG_LEVEL ?? "info" },
  });

  app.setErrorHandler((err, req, reply) => {
    if (err instanceof AppError) {
      return reply.code(err.status).send({ error: { code: err.code, message: err.message } });
    }
    if (err instanceof ZodError) {
      const message = err.issues
        .map((i) => `${i.path.join(".") || "body"}: ${i.message}`)
        .join("; ");
      return reply.code(400).send({ error: { code: "validation", message } });
    }
    if (err instanceof Prisma.PrismaClientKnownRequestError) {
      if (err.code === "P2002") {
        return reply.code(409).send({ error: { code: "conflict", message: "Already exists" } });
      }
      if (err.code === "P2025") {
        return reply.code(404).send({ error: { code: "not_found", message: "Not found" } });
      }
    }
    const fastifyErr = err as FastifyError;
    if (fastifyErr.statusCode && fastifyErr.statusCode < 500) {
      return reply
        .code(fastifyErr.statusCode)
        .send({ error: { code: "request_error", message: fastifyErr.message } });
    }
    req.log.error(err);
    return reply.code(500).send({ error: { code: "internal", message: "Internal server error" } });
  });

  // Personal LAN app behind JWT auth — origin isn't the security
  // boundary here, so reflect any origin rather than maintaining an
  // allowlist for every place the dashboard might be opened from
  // (dev laptop, phone on the LAN, the Radxa itself later).
  app.register(cors, { origin: true });
  app.register(authPlugin);
  app.register(healthRoutes);
  app.register(v1Routes, { prefix: "/v1" });

  return app;
}
