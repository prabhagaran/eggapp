// Pure incubation rules — no Fastify, no Prisma queries (types only).
import type { BatchStatus, Species } from "@prisma/client";
import { AppError } from "../lib/errors.js";

const DAY_MS = 86_400_000;

// BR-001 (setting is an optional intermediate state)
export const BATCH_TRANSITIONS: Record<BatchStatus, readonly BatchStatus[]> = {
  planned: ["setting", "incubating", "aborted"],
  setting: ["incubating", "aborted"],
  incubating: ["lockdown", "aborted"],
  lockdown: ["hatching", "aborted"],
  hatching: ["completed", "aborted"],
  completed: ["closed"],
  closed: [],
  aborted: [],
};

export function assertTransition(from: BatchStatus, to: BatchStatus): void {
  if (!BATCH_TRANSITIONS[from].includes(to)) {
    throw new AppError(409, "invalid_transition", `Cannot transition batch from '${from}' to '${to}'`);
  }
}

// BR-008: schedule derives from species reference at set time.
export function computeSchedule(
  species: Pick<Species, "incubationDays" | "lockdownOffsetDays" | "candlingDays">,
  setAt: Date,
) {
  const dayN = (n: number) => new Date(setAt.getTime() + n * DAY_MS);
  return {
    candlingDays: species.candlingDays,
    lockdownAt: dayN(species.incubationDays - species.lockdownOffsetDays),
    expectedHatchAt: dayN(species.incubationDays),
  };
}

// BR-011: >7 days warn, >14 days blocked without an override note.
export type StorageAgeVerdict = "ok" | "warning" | "blocked";

export function classifyStorageAge(collectedOn: Date, at: Date) {
  const ageDays = Math.floor((at.getTime() - collectedOn.getTime()) / DAY_MS);
  const verdict: StorageAgeVerdict = ageDays > 14 ? "blocked" : ageDays > 7 ? "warning" : "ok";
  return { ageDays, verdict };
}

export interface CandlingCounts {
  fertile: number;
  clear: number;
  bloodRing: number;
  unsure: number;
}

// BR-003: clear/blood-ring leave the viable count; discrepancies are
// surfaced, never absorbed.
export function candlingOutcome(counts: CandlingCounts, currentViable: number) {
  const total = counts.fertile + counts.clear + counts.bloodRing + counts.unsure;
  return { newViable: counts.fertile + counts.unsure, discrepancy: total - currentViable };
}

export interface HatchCounts {
  hatched: number;
  pippedDead: number;
  deadInShell: number;
  unhatched: number;
}

export function hatchDiscrepancy(counts: HatchCounts, viableAtLockdown: number) {
  return counts.hatched + counts.pippedDead + counts.deadInShell + counts.unhatched - viableAtLockdown;
}

// BR-008: no hatch outcome before expectedHatch − 2 days.
export function assertHatchDateAllowed(expectedHatchAt: Date, hatchedAt: Date): void {
  if (hatchedAt.getTime() < expectedHatchAt.getTime() - 2 * DAY_MS) {
    throw new AppError(
      409,
      "too_early",
      "Hatch cannot be recorded more than 2 days before the expected hatch date",
    );
  }
}

// BR-015 metric definitions (domain-knowledge §4), one decimal place.
const pct = (num: number, den: number): number | null =>
  den > 0 ? Math.round((num / den) * 1000) / 10 : null;

export const fertilityPct = (fertile: number, setCount: number) => pct(fertile, setCount);

export function hatchMetrics(setCount: number, fertileCount: number | null, hatched: number) {
  return {
    hatchOfSetPct: pct(hatched, setCount),
    hatchOfFertilePct: fertileCount != null ? pct(hatched, fertileCount) : null,
  };
}
