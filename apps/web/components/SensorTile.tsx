"use client";
import type { ReactNode } from "react";

export type TileStatus = "normal" | "warn" | "critical" | "unavailable";

// A reading's band is defined by its optimal range. `min`/`max` are the
// edges of normal; `warnBand` is how far outside that still counts as a
// warning before it becomes critical. Either edge may be omitted for
// one-sided metrics (CO2 has a ceiling but no meaningful floor).
export interface Range {
  min?: number;
  max?: number;
  warnBand?: number;
}

export function classify(value: number | null | undefined, range?: Range): TileStatus {
  if (value == null) return "unavailable";
  if (!range) return "normal";
  const band = range.warnBand ?? 0;
  const { min, max } = range;
  if (min != null && value < min) return value < min - band ? "critical" : "warn";
  if (max != null && value > max) return value > max + band ? "critical" : "warn";
  return "normal";
}

const LABEL: Record<TileStatus, string> = {
  normal: "NORMAL",
  warn: "WARNING",
  critical: "CRITICAL",
  unavailable: "NO SENSOR",
};

export function rangeHint(range?: Range, unit?: string): string | null {
  if (!range) return null;
  const u = unit ? ` ${unit}` : "";
  if (range.min != null && range.max != null) return `Optimal: ${range.min}–${range.max}${u}`;
  if (range.max != null) return `Optimal: <${range.max}${u}`;
  if (range.min != null) return `Optimal: >${range.min}${u}`;
  return null;
}

/**
 * On/off actuator tile — same shell as SensorTile so a mixed grid reads as
 * one system. `state` of null means the device never reported this relay
 * (firmware predating the actuator fields); that renders as "—", never as
 * OFF, since "we don't know" and "it is off" are different claims about
 * hardware.
 */
export function StateTile({
  icon,
  label,
  state,
  hint,
  activeIsGood = true,
}: {
  icon: ReactNode;
  label: string;
  state: boolean | null | undefined;
  hint?: string;
  activeIsGood?: boolean;
}) {
  const unknown = state == null;
  // An actuator being on isn't inherently good or bad — a running heater is
  // normal mid-cycle. Colour by "is it doing something", not by alarm.
  const tone: TileStatus = unknown ? "unavailable" : state && !activeIsGood ? "warn" : "normal";

  return (
    <div className={`sensor-tile${unknown ? " is-unavailable" : ""}`}>
      <div className="sensor-tile-top">
        <span className={`sensor-icon ${state && !unknown ? tone : "unavailable"}`}>{icon}</span>
        <span className={`sensor-pill ${state && !unknown ? tone : "unavailable"}`}>
          {unknown ? "NO DATA" : state ? "ACTIVE" : "IDLE"}
        </span>
      </div>
      <div className="sensor-label">{label}</div>
      <div className="sensor-value">
        {unknown ? <b className="na">—</b> : <b className={state ? "" : "na"}>{state ? "ON" : "OFF"}</b>}
      </div>
      <div className="sensor-hint">{unknown ? "Not reported by this device" : (hint ?? " ")}</div>
    </div>
  );
}

export function SensorTile({
  icon,
  label,
  value,
  unit,
  range,
  hint,
  decimals = 1,
}: {
  icon: ReactNode;
  label: string;
  value: number | null | undefined;
  unit?: string;
  range?: Range;
  hint?: string;
  decimals?: number;
}) {
  const status = classify(value, range);
  const unavailable = status === "unavailable";

  return (
    <div className={`sensor-tile${unavailable ? " is-unavailable" : ""}`}>
      <div className="sensor-tile-top">
        <span className={`sensor-icon ${status}`}>{icon}</span>
        <span className={`sensor-pill ${status}`}>{LABEL[status]}</span>
      </div>
      <div className="sensor-label">{label}</div>
      <div className="sensor-value">
        {/* An em dash, never a 0 — a missing sensor must not read as a
            real measurement of zero. */}
        {unavailable ? <b className="na">—</b> : <b>{value!.toFixed(decimals)}</b>}
        {!unavailable && unit && <span className="sensor-unit">{unit}</span>}
      </div>
      <div className="sensor-hint">
        {unavailable ? "Not fitted on this device" : (hint ?? rangeHint(range, unit) ?? " ")}
      </div>
    </div>
  );
}
