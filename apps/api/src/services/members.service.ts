import { randomBytes } from "node:crypto";
import { hash } from "bcryptjs";
import type { FarmRole } from "@prisma/client";
import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

const BCRYPT_ROUNDS = 10;

export async function listMembers(farmId: string) {
  const memberships = await getPrisma().farmMembership.findMany({
    where: { farmId },
    include: { user: { select: { id: true, email: true, name: true } } },
    orderBy: { createdAt: "asc" },
  });
  return memberships.map((m) => ({
    userId: m.userId,
    email: m.user.email,
    name: m.user.name,
    role: m.role,
    memberSince: m.createdAt,
  }));
}

export interface InviteInput {
  email: string;
  role: Extract<FarmRole, "manager" | "worker">;
  name?: string;
}

// US-USR-002: no email infrastructure exists in this system (personal-scale
// deployment, per nfr.md — 1-2 concurrent users). An existing user is
// granted membership immediately; a brand-new user gets an account with a
// system-generated temporary password, returned once in this response for
// the Owner to share out-of-band (text message, in person) — deliberately
// simple rather than standing up token-based invite emails for this scale.
export async function inviteMember(farmId: string, input: InviteInput) {
  const prisma = getPrisma();
  let user = await prisma.user.findUnique({ where: { email: input.email } });
  let temporaryPassword: string | undefined;

  if (!user) {
    temporaryPassword = randomBytes(6).toString("base64url");
    const passwordHash = await hash(temporaryPassword, BCRYPT_ROUNDS);
    user = await prisma.user.create({ data: { email: input.email, passwordHash, name: input.name ?? input.email } });
  }

  const existing = await prisma.farmMembership.findUnique({ where: { userId_farmId: { userId: user.id, farmId } } });
  if (existing) throw new AppError(409, "already_member", "This user is already a member of the farm");

  const membership = await prisma.farmMembership.create({ data: { userId: user.id, farmId, role: input.role } });
  return { userId: user.id, email: user.email, name: user.name, role: membership.role, temporaryPassword };
}

export async function removeMember(farmId: string, targetUserId: string, requestingUserId: string) {
  if (targetUserId === requestingUserId) {
    throw new AppError(400, "cannot_remove_self", "Use another owner's account to remove yourself");
  }
  const prisma = getPrisma();
  const existing = await prisma.farmMembership.findUnique({ where: { userId_farmId: { userId: targetUserId, farmId } } });
  if (!existing) throw new AppError(404, "not_found", "Membership not found");

  if (existing.role === "owner") {
    const ownerCount = await prisma.farmMembership.count({ where: { farmId, role: "owner" } });
    if (ownerCount <= 1) throw new AppError(409, "last_owner", "Cannot remove the farm's only owner");
  }
  await prisma.farmMembership.delete({ where: { userId_farmId: { userId: targetUserId, farmId } } });
}
