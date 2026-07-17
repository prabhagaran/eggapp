// Pure alerting rules (BR-006) — no Prisma, no Fastify.
import type { Species } from "@prisma/client";

export type AlertMetric = "temp_c" | "humidity_pct";
export type AlertSeverity = "warning" | "critical";
export type IncubationStage = "incubating" | "lockdown";

export interface Bounds {
  warnMin: number;
  warnMax: number;
  critMin: number;
  critMax: number;
}

// domain-knowledge.md gives temperature tolerance as a fixed delta from
// setpoint ("±0.3°C from setpoint sustained") — a general rule across
// species, not a per-species field, so it's a constant here rather than
// a Species column.
const TEMP_WARN_DELTA_C = 0.3;

// Reconciliation: domain-knowledge.md states humidity tolerance both as
// "±5/±10 pp" (general rule) AND as per-stage min/max ranges (species
// table). Resolved here: the stage min/max range *is* the warning band
// (already stage-aware, per BR-006's explicit requirement), and critical
// extends 10pp beyond each side of it — using the "±10" figure as an
// additional margin past the stage band rather than around a setpoint
// that doesn't exist in the schema.
const HUM_CRITICAL_MARGIN_PCT = 10;

const round1 = (v: number) => Math.round(v * 10) / 10;

/** BR-006 stage-aware defaults. AlertRule overrides (if any) take priority — see alert.service.ts. */
export function effectiveBounds(
  species: Pick<
    Species,
    | "tempSetC"
    | "tempMinC"
    | "tempMaxC"
    | "humidityIncubMinPct"
    | "humidityIncubMaxPct"
    | "humidityLockMinPct"
    | "humidityLockMaxPct"
  >,
  stage: IncubationStage,
  metric: AlertMetric,
): Bounds {
  if (metric === "temp_c") {
    // round1: avoids float artifacts like 37.6 - 0.3 = 37.300000000000004
    // leaking into stored bounds / alert messages.
    return {
      warnMin: round1(species.tempSetC - TEMP_WARN_DELTA_C),
      warnMax: round1(species.tempSetC + TEMP_WARN_DELTA_C),
      critMin: species.tempMinC,
      critMax: species.tempMaxC,
    };
  }
  const warnMin = stage === "lockdown" ? species.humidityLockMinPct : species.humidityIncubMinPct;
  const warnMax = stage === "lockdown" ? species.humidityLockMaxPct : species.humidityIncubMaxPct;
  return {
    warnMin,
    warnMax,
    critMin: warnMin - HUM_CRITICAL_MARGIN_PCT,
    critMax: warnMax + HUM_CRITICAL_MARGIN_PCT,
  };
}

/** Null value (sensor fault) is never classified as a breach — that's a device/data-quality concern, not an environmental one. */
export function classify(value: number | null, bounds: Bounds): AlertSeverity | "ok" {
  if (value === null) return "ok";
  if (value < bounds.critMin || value > bounds.critMax) return "critical";
  if (value < bounds.warnMin || value > bounds.warnMax) return "warning";
  return "ok";
}
