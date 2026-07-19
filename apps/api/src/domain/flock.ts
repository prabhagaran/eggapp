// Flock stage/age derivation (US-FLK-002, domain-knowledge.md §5). Pure
// functions — no I/O — matching alerting.ts's layering: domain rules live
// here, services call in, routes never compute this themselves.
import type { FlockPurpose } from "@prisma/client";

export type FlockStage =
  | "brooding"
  | "grower"
  | "pre_lay"
  | "layer"
  | "broiler_starter"
  | "broiler_grower"
  | "broiler_finisher";

const DAYS_PER_WEEK = 7;

// Breeder purpose reuses the layer-type progression — domain-knowledge
// has no separate breeder schedule; breeders are adult layer-type birds
// kept for hatching eggs rather than production.
export function deriveStage(purpose: FlockPurpose, ageDays: number): FlockStage {
  if (purpose === "broiler") {
    if (ageDays < 2 * DAYS_PER_WEEK) return "broiler_starter";
    if (ageDays < 5 * DAYS_PER_WEEK) return "broiler_grower";
    return "broiler_finisher";
  }
  if (ageDays < 6 * DAYS_PER_WEEK) return "brooding";
  if (ageDays < 16 * DAYS_PER_WEEK) return "grower";
  if (ageDays < 18 * DAYS_PER_WEEK) return "pre_lay";
  return "layer";
}

// What feed type a stage expects (domain-knowledge §7) — used for
// US-FED-002's non-blocking stage-mismatch warning. Matched as a
// case-insensitive substring against the logged feedType (a free-text
// field, not an enum — product names vary), so "Layer Pellets 17%"
// still matches "layer".
export function expectedFeedKeyword(stage: FlockStage): string {
  switch (stage) {
    case "brooding":
    case "broiler_starter":
      return "starter";
    case "grower":
    case "pre_lay":
    case "broiler_grower":
      return "grower";
    case "layer":
      return "layer";
    case "broiler_finisher":
      return "finisher";
  }
}

export function birthDate(flock: {
  hatchedAt: Date | null;
  acquisitionDate: Date | null;
  acquisitionAgeDays: number | null;
}): Date | null {
  if (flock.hatchedAt) return flock.hatchedAt;
  if (flock.acquisitionDate && flock.acquisitionAgeDays != null) {
    return new Date(flock.acquisitionDate.getTime() - flock.acquisitionAgeDays * 86_400_000);
  }
  return null;
}

export function ageDaysFrom(birth: Date, now: Date = new Date()): number {
  return Math.floor((now.getTime() - birth.getTime()) / 86_400_000);
}

// A manual stageOverride always wins over the age-derived stage — the
// escape hatch for when acquisition/hatch data was entered wrong and the
// derived stage doesn't match reality.
export function resolveStage(
  purpose: FlockPurpose,
  ageDays: number | null,
  stageOverride: FlockStage | null,
): FlockStage | null {
  if (stageOverride) return stageOverride;
  return ageDays != null ? deriveStage(purpose, ageDays) : null;
}
