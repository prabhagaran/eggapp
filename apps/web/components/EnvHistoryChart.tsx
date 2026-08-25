"use client";
// US-ENV-002: the temperature and humidity history charts.
//
// Two stacked single-series panels, never one dual-axis chart: °C and %RH
// are different scales, and overlaying them on a shared y-axis makes the
// crossings look meaningful when they are an artifact of the units. The
// panels share a syncId, so one crosshair reads both at the same instant —
// which is the only reason anyone wanted them overlaid.
import { useMemo } from "react";
import {
  Area,
  AreaChart,
  CartesianGrid,
  ReferenceArea,
  ReferenceLine,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";

export interface EnvPoint {
  ts: string;
  tempC: number | null;
  humidityPct: number | null;
}

export interface EnvStat {
  min: number | null;
  max: number | null;
  avg: number | null;
}

export interface EnvBand {
  setpoint: number | null;
  hysteresis: number | null;
}

export type EnvRange = "24h" | "7d" | "batch";

// Warm for heat, cool for water — and validated as a pair against the white
// chart surface (all-pairs CVD ΔE 24.7, normal-vision 33.6).
const TEMP = "#eb6834";
const HUMIDITY = "#2a78d6";
const GRID = "#e8ebe3";
const AXIS_TEXT = "#77816f";
const SURFACE = "#ffffff";
const BAND_INK = "#123524";

// ── Scales ───────────────────────────────────────────────────────────
// The optimal band is part of the story, so it stays inside the domain
// even when the readings never leave it — a band you cannot see is a band
// that cannot reassure you.
function niceDomain(values: number[], band: [number, number] | null): [number, number] {
  const all = band ? [...values, ...band] : values;
  let lo = Math.min(...all);
  let hi = Math.max(...all);
  if (hi - lo < 1e-6) {
    lo -= 1;
    hi += 1;
  }
  const pad = (hi - lo) * 0.18;
  return [lo - pad, hi + pad];
}

function bandEdges(band?: EnvBand): [number, number] | null {
  if (!band || band.setpoint == null) return null;
  const h = band.hysteresis ?? 0;
  return [band.setpoint - h, band.setpoint + h];
}

// ── Time axis ────────────────────────────────────────────────────────
// A 24h window wants clock time; a week or a whole batch wants the day,
// otherwise every tick reads "12:00" and the axis stops locating anything.
function tickFormatterFor(range: EnvRange) {
  if (range === "24h") {
    return (ts: number) => new Date(ts).toLocaleTimeString(undefined, { hour: "numeric", minute: "2-digit" });
  }
  if (range === "7d") {
    return (ts: number) => new Date(ts).toLocaleString(undefined, { weekday: "short", hour: "numeric" });
  }
  return (ts: number) => new Date(ts).toLocaleDateString(undefined, { month: "short", day: "numeric" });
}

function fullStamp(ts: number) {
  return new Date(ts).toLocaleString(undefined, {
    month: "short",
    day: "numeric",
    hour: "numeric",
    minute: "2-digit",
  });
}

function fmt(n: number | null | undefined, unit: string, decimals: number) {
  return n == null ? "—" : `${n.toFixed(decimals)}${unit}`;
}

// ── Panel ────────────────────────────────────────────────────────────
interface PanelProps {
  data: { ts: number; value: number | null }[];
  title: string;
  unit: string;
  color: string;
  band: [number, number] | null;
  setpoint: number | null;
  summary?: EnvStat;
  decimals: number;
  range: EnvRange;
  showXAxis: boolean;
  gradientId: string;
}

function MetricPanel({
  data,
  title,
  unit,
  color,
  band,
  setpoint,
  summary,
  decimals,
  range,
  showXAxis,
  gradientId,
}: PanelProps) {
  const values = data.map((d) => d.value).filter((v): v is number => v != null);
  const latest = [...data].reverse().find((d) => d.value != null)?.value ?? null;

  if (values.length < 2) {
    return (
      <div className="env-panel">
        <div className="env-panel-head">
          <span className="env-panel-title">
            <i className="env-key" style={{ background: color }} aria-hidden="true" />
            {title}
          </span>
        </div>
        <p className="muted" style={{ margin: "0.4rem 0 0" }}>
          Not enough readings in this window to chart.
        </p>
      </div>
    );
  }

  const domain = niceDomain(values, band);
  const tickFormatter = tickFormatterFor(range);

  // Only readings outside the optimal band get a dot. A marker on every
  // point is noise; a marker on the excursions is the thing you opened
  // this chart to find.
  const outOfBand = (v: number) => (band ? v < band[0] || v > band[1] : false);

  return (
    <div className="env-panel">
      <div className="env-panel-head">
        <span className="env-panel-title">
          <i className="env-key" style={{ background: color }} aria-hidden="true" />
          {title}
        </span>
        <span className="env-panel-now" style={{ color }}>
          {fmt(latest, unit, decimals)}
        </span>
        {summary && (
          <span className="env-panel-stats">
            <span>
              min <b>{fmt(summary.min, unit, decimals)}</b>
            </span>
            <span>
              avg <b>{fmt(summary.avg, unit, decimals)}</b>
            </span>
            <span>
              max <b>{fmt(summary.max, unit, decimals)}</b>
            </span>
          </span>
        )}
      </div>

      <div className="env-plot">
        <ResponsiveContainer>
          <AreaChart data={data} syncId="env-history" margin={{ top: 10, right: 16, bottom: 0, left: 0 }}>
            <defs>
              <linearGradient id={gradientId} x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stopColor={color} stopOpacity={0.22} />
                <stop offset="100%" stopColor={color} stopOpacity={0.02} />
              </linearGradient>
            </defs>

            <CartesianGrid stroke={GRID} strokeWidth={1} vertical={false} />

            {band && (
              <ReferenceArea
                y1={band[0]}
                y2={band[1]}
                fill={BAND_INK}
                fillOpacity={0.05}
                stroke="none"
                ifOverflow="extendDomain"
              />
            )}
            {setpoint != null && (
              <ReferenceLine
                y={setpoint}
                stroke={BAND_INK}
                strokeOpacity={0.35}
                strokeDasharray="4 4"
                ifOverflow="extendDomain"
                label={{
                  value: `target ${setpoint.toFixed(decimals)}${unit}`,
                  position: "insideTopRight",
                  fill: AXIS_TEXT,
                  fontSize: 11,
                }}
              />
            )}

            <XAxis
              dataKey="ts"
              type="number"
              scale="time"
              domain={["dataMin", "dataMax"]}
              tickFormatter={tickFormatter}
              tick={{ fill: AXIS_TEXT, fontSize: 11 }}
              tickMargin={8}
              minTickGap={56}
              axisLine={{ stroke: GRID }}
              tickLine={false}
              height={showXAxis ? 28 : 8}
              hide={!showXAxis}
            />
            <YAxis
              domain={domain}
              tick={{ fill: AXIS_TEXT, fontSize: 11 }}
              tickCount={5}
              tickFormatter={(v: number) => v.toFixed(decimals)}
              width={44}
              axisLine={false}
              tickLine={false}
            />

            <Tooltip
              cursor={{ stroke: AXIS_TEXT, strokeOpacity: 0.45, strokeWidth: 1 }}
              isAnimationActive={false}
              content={({ active, payload, label }) => {
                if (!active || !payload?.length) return null;
                const v = payload[0]!.value as number | null;
                return (
                  <div className="env-tip">
                    <b style={{ color }}>{fmt(v, unit, decimals)}</b>
                    <span>{title}</span>
                    <span className="env-tip-ts">{fullStamp(Number(label))}</span>
                  </div>
                );
              }}
            />

            <Area
              type="monotone"
              dataKey="value"
              stroke={color}
              strokeWidth={2}
              strokeLinecap="round"
              strokeLinejoin="round"
              fill={`url(#${gradientId})`}
              isAnimationActive={false}
              connectNulls
              activeDot={{ r: 4, fill: color, stroke: SURFACE, strokeWidth: 2 }}
              dot={(props: { cx?: number; cy?: number; payload?: { value: number | null }; index?: number }) => {
                const v = props.payload?.value;
                // Recharts wants an element back, never null.
                if (v == null || props.cx == null || props.cy == null || !outOfBand(v)) {
                  return <g key={`d${props.index}`} />;
                }
                return (
                  <circle
                    key={`d${props.index}`}
                    cx={props.cx}
                    cy={props.cy}
                    r={3.5}
                    fill={color}
                    stroke={SURFACE}
                    strokeWidth={2}
                  />
                );
              }}
            />
          </AreaChart>
        </ResponsiveContainer>
      </div>
    </div>
  );
}

// ── Public chart ─────────────────────────────────────────────────────
export function EnvHistoryChart({
  points,
  range,
  tempBand,
  humidityBand,
  summary,
}: {
  points: EnvPoint[];
  range: EnvRange;
  tempBand?: EnvBand;
  humidityBand?: EnvBand;
  summary?: { temp: EnvStat; humidity: EnvStat };
}) {
  const { temp, humidity } = useMemo(() => {
    const sorted = [...points].sort((a, b) => Date.parse(a.ts) - Date.parse(b.ts));
    return {
      temp: sorted.map((p) => ({ ts: Date.parse(p.ts), value: p.tempC })),
      humidity: sorted.map((p) => ({ ts: Date.parse(p.ts), value: p.humidityPct })),
    };
  }, [points]);

  const tEdges = bandEdges(tempBand);
  const hEdges = bandEdges(humidityBand);

  return (
    <div className="env-chart card">
      <MetricPanel
        data={temp}
        title="Temperature"
        unit="°C"
        color={TEMP}
        band={tEdges}
        setpoint={tempBand?.setpoint ?? null}
        summary={summary?.temp}
        decimals={1}
        range={range}
        showXAxis={false}
        gradientId="envTempFill"
      />
      <div className="env-divider" />
      <MetricPanel
        data={humidity}
        title="Humidity"
        unit="%"
        color={HUMIDITY}
        band={hEdges}
        setpoint={humidityBand?.setpoint ?? null}
        summary={summary?.humidity}
        decimals={0}
        range={range}
        showXAxis
        gradientId="envHumFill"
      />
      {(tEdges || hEdges) && (
        <p className="env-legend-note">
          <i className="env-band-key" aria-hidden="true" /> shaded band = setpoint ± hysteresis; dots mark readings
          outside it.
        </p>
      )}

      {/* The hover readout enhances; it never gates. Every plotted value is
          reachable here without a pointer. */}
      <details className="env-table">
        <summary>Show readings ({temp.length})</summary>
        <div className="table-wrap" style={{ maxHeight: 320, overflowY: "auto", marginTop: "0.6rem" }}>
          <table>
            <thead>
              <tr>
                <th>Time</th>
                <th>Temperature (°C)</th>
                <th>Humidity (%)</th>
              </tr>
            </thead>
            <tbody>
              {temp.map((t, i) => (
                <tr key={t.ts}>
                  <td>{fullStamp(t.ts)}</td>
                  <td>{fmt(t.value, "", 1)}</td>
                  <td>{fmt(humidity[i]?.value ?? null, "", 0)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </details>
    </div>
  );
}

export const envChartColors = { TEMP, HUMIDITY };
