import type { User } from "@prisma/client";
import type { FastifyInstance } from "fastify";
import { compare, hash } from "bcryptjs";
import { config } from "../config.js";
import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

const BCRYPT_ROUNDS = 10;

function publicUser(user: User) {
  return { id: user.id, email: user.email, name: user.name };
}

function signTokens(app: FastifyInstance, user: User) {
  return {
    accessToken: app.jwt.sign({ sub: user.id, type: "access" }, { expiresIn: config.accessTokenTtl }),
    refreshToken: app.jwt.sign(
      { sub: user.id, type: "refresh", ver: user.tokenVersion },
      { expiresIn: config.refreshTokenTtl },
    ),
  };
}

export interface SetupInput {
  ownerName: string;
  email: string;
  password: string;
  farmName: string;
  timezone: string;
  location?: string;
}

// US-FRM-001: first-run wizard. Refuses once any user exists.
export async function setupFirstRun(app: FastifyInstance, input: SetupInput) {
  const prisma = getPrisma();
  if ((await prisma.user.count()) > 0) {
    throw new AppError(409, "already_initialized", "Setup has already been completed");
  }
  const passwordHash = await hash(input.password, BCRYPT_ROUNDS);
  const { user, farm } = await prisma.$transaction(async (tx) => {
    const user = await tx.user.create({
      data: { email: input.email, passwordHash, name: input.ownerName },
    });
    const farm = await tx.farm.create({
      data: { name: input.farmName, timezone: input.timezone, location: input.location },
    });
    await tx.farmMembership.create({
      data: { userId: user.id, farmId: farm.id, role: "owner" },
    });
    return { user, farm };
  });
  return { user: publicUser(user), farm, ...signTokens(app, user) };
}

export async function login(app: FastifyInstance, email: string, password: string) {
  const user = await getPrisma().user.findUnique({ where: { email } });
  if (!user || !(await compare(password, user.passwordHash))) {
    throw new AppError(401, "invalid_credentials", "Invalid email or password");
  }
  return { user: publicUser(user), ...signTokens(app, user) };
}

export async function refresh(app: FastifyInstance, refreshToken: string) {
  let payload: { sub: string; type: string; ver?: number };
  try {
    payload = app.jwt.verify(refreshToken);
  } catch {
    throw new AppError(401, "unauthorized", "Invalid refresh token");
  }
  if (payload.type !== "refresh") {
    throw new AppError(401, "unauthorized", "Not a refresh token");
  }
  const user = await getPrisma().user.findUnique({ where: { id: payload.sub } });
  if (!user || payload.ver !== user.tokenVersion) {
    throw new AppError(401, "unauthorized", "Refresh token revoked");
  }
  return { user: publicUser(user), ...signTokens(app, user) };
}

// Bumping tokenVersion invalidates every outstanding refresh token.
export async function logoutAll(userId: string) {
  await getPrisma().user.update({
    where: { id: userId },
    data: { tokenVersion: { increment: 1 } },
  });
}

export async function me(userId: string) {
  const user = await getPrisma().user.findUnique({
    where: { id: userId },
    include: { memberships: { include: { farm: true } } },
  });
  if (!user) throw new AppError(404, "not_found", "User not found");
  return {
    ...publicUser(user),
    farms: user.memberships.map((m) => ({
      id: m.farm.id,
      name: m.farm.name,
      timezone: m.farm.timezone,
      location: m.farm.location,
      role: m.role,
    })),
  };
}
