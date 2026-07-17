import type { FarmRole } from "@prisma/client";
import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

// BR-013: every farm-scoped service call goes through this gate.
// Non-membership returns 404 (not 403) so farm existence isn't leaked.
const roleRank: Record<FarmRole, number> = { worker: 1, manager: 2, owner: 3 };

export async function requireMembership(userId: string, farmId: string, minRole: FarmRole = "worker") {
  const membership = await getPrisma().farmMembership.findUnique({
    where: { userId_farmId: { userId, farmId } },
  });
  if (!membership) {
    throw new AppError(404, "not_found", "Farm not found");
  }
  if (roleRank[membership.role] < roleRank[minRole]) {
    throw new AppError(403, "forbidden", `Requires ${minRole} role or higher`);
  }
  return membership;
}
