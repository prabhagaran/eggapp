import fastifyJwt from "@fastify/jwt";
import type { FastifyReply, FastifyRequest } from "fastify";
import fp from "fastify-plugin";
import { config } from "../config.js";

export interface TokenPayload {
  sub: string;
  type: "access" | "refresh";
  ver?: number;
}

declare module "@fastify/jwt" {
  interface FastifyJWT {
    payload: TokenPayload;
    user: TokenPayload;
  }
}

declare module "fastify" {
  interface FastifyInstance {
    authenticate: (req: FastifyRequest, reply: FastifyReply) => Promise<void>;
  }
}

export const authPlugin = fp(async (app) => {
  app.register(fastifyJwt, { secret: config.jwtSecret });

  app.decorate("authenticate", async (req: FastifyRequest, reply: FastifyReply) => {
    try {
      await req.jwtVerify();
    } catch {
      return reply
        .code(401)
        .send({ error: { code: "unauthorized", message: "Missing or invalid token" } });
    }
    if (req.user.type !== "access") {
      return reply
        .code(401)
        .send({ error: { code: "unauthorized", message: "Access token required" } });
    }
  });
});
