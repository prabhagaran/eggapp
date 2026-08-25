"use client";
import {
  Area,
  AreaChart,
  Bar,
  BarChart,
  CartesianGrid,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";
import type { BatchStatus } from "@eggapp/shared-types";
import type { IncubatorHistory } from "../lib/types";

// Design tokens mirrored from globals.css — recharts renders to SVG/canvas
// and can't read CSS custom properties, so the hex values are pinned here.
const ACCENT = "#1f7a4d";
const ACCENT_SOFT = "#e3f3e7";
const HUMIDITY = "#2a6fd6";
const HUMIDITY_SOFT = "#e7effb";
const WARN = "#a6690b";
const DANGER = "#b3261e";
const MUTED = "#77816f";
const BORDER = "#e8ebe3";

const tooltipStyle = {
  background: "#fff",
  border: `1px solid ${BORDER}`,
  borderRadius: 10,
  boxShadow: "0 10px 30px rgba(15, 40, 25, 0.12)",
  fontSize: "0.8rem",
  padding: "0.5rem 0.7rem",
};

// ── Stat-tile sparkline ──────────────────────────────────────────────
// One series, no axes/legend/tooltip per the figure spec — a trend
// glance, not a chart to read values off of.
export function Sparkline({ data, color = ACCENT }: { data: { value: number }[]; color?: string }) {
  if (data.length < 2) return null;
  return (
    <div style={{ width: "100%", height: 36 }}>
      <ResponsiveContainer>
        <AreaChart data={data} margin={{ top: 2, right: 0, bottom: 0, left: 0 }}>
          <defs>
            <linearGradient id="sparkFill" x1="0" y1="0" x2="0" y2="1">
              <stop offset="0%" stopColor={color} stopOpacity={0.25} />
              <stop offset="100%" stopColor={color} stopOpacity={0} />
            </linearGradient>
          </defs>
          <Area type="monotone" dataKey="value" stroke={color} strokeWidth={2} fill="url(#sparkFill)" isAnimationActive={false} />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
}

// ── Batch pipeline funnel ────────────────────────────────────────────
// Order encodes the pipeline stage, so one hue is enough — see
// choosing-a-form.md: an ordinal ramp is for when order isn't already
// legible from the axis, which it is here.
const PIPELINE_STAGES: { status: BatchStatus; label: string }[] = [
  { status: "planned", label: "Planned" },
  { status: "setting", label: "Setting" },
  { status: "incubating", label: "Incubating" },
  { status: "lockdown", label: "Lockdown" },
  { status: "hatching", label: "Hatching" },
  { status: "completed", label: "Completed" },
];

export function BatchPipelineChart({ counts }: { counts: Record<string, number> }) {
  const data = PIPELINE_STAGES.map((s) => ({ label: s.label, count: counts[s.status] ?? 0 }));
  const max = Math.max(1, ...data.map((d) => d.count));
  return (
    <div style={{ width: "100%", height: 220 }}>
      <ResponsiveContainer>
        <BarChart data={data} layout="vertical" margin={{ top: 4, right: 28, bottom: 4, left: 4 }}>
          <XAxis type="number" domain={[0, max]} hide />
          <YAxis
            type="category"
            dataKey="label"
            width={90}
            axisLine={false}
            tickLine={false}
            tick={{ fill: MUTED, fontSize: 12 }}
          />
          <Tooltip
            cursor={{ fill: ACCENT_SOFT }}
            contentStyle={tooltipStyle}
            formatter={(value) => [`${value} batch${value === 1 ? "" : "es"}`, undefined]}
            labelFormatter={() => ""}
          />
          <Bar dataKey="count" fill={ACCENT} radius={[0, 4, 4, 0]} maxBarSize={22} label={{ position: "right", fill: MUTED, fontSize: 12 }} />
        </BarChart>
      </ResponsiveContainer>
    </div>
  );
}

// ── Alerts by severity ───────────────────────────────────────────────
// Status colors are reserved and always paired with a label — never
// color alone — so each row carries its own text, not a shared legend.
export function AlertsSeverityBars({ warning, critical }: { warning: number; critical: number }) {
  const max = Math.max(1, warning, critical);
  const rows = [
    { label: "Critical", value: critical, color: DANGER },
    { label: "Warning", value: warning, color: WARN },
  ];
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "0.55rem" }}>
      {rows.map((r) => (
        <div key={r.label} style={{ display: "flex", alignItems: "center", gap: "0.6rem" }}>
          <span style={{ width: 56, fontSize: "0.78rem", color: MUTED, flexShrink: 0 }}>{r.label}</span>
          <div style={{ flex: 1, height: 10, borderRadius: 999, background: "var(--surface-muted)", overflow: "hidden" }}>
            <div
              style={{
                width: `${(r.value / max) * 100}%`,
                height: "100%",
                background: r.color,
                borderRadius: 999,
                transition: "width 0.3s",
              }}
            />
          </div>
          <span style={{ width: 20, textAlign: "right", fontSize: "0.8rem", fontWeight: 700, flexShrink: 0 }}>{r.value}</span>
        </div>
      ))}
    </div>
  );
}

// ── Incubator temp/humidity mini chart ─────────────────────
// Two stacked single-series sparkplots, not one two-series line chart:
// °C and %RH have no common scale, so plotting them against one y-axis
// makes their crossings look like events. Same colours and the same
// syncId'd crosshair as the full chart on the incubator page, at
// dashboard-card size.
const ENV_TEMP = "#eb6834";
const ENV_HUMIDITY = "#2a78d6";

function MiniEnvPanel({
  data,
  label,
  unit,
  color,
  decimals,
  gradientId,
  syncId,
  showAxis,
}: {
  data: { ts: number; value: number | null }[];
  label: string;
  unit: string;
  color: string;
  decimals: number;
  gradientId: string;
  syncId: string;
  showAxis: boolean;
}) {
  const values = data.map((d) => d.value).filter((v): v is number => v != null);
  if (values.length < 2) return null;
  const latest = [...data].reverse().find((d) => d.value != null)!.value!;
  const lo = Math.min(...values);
  const hi = Math.max(...values);
  const pad = (hi - lo) * 0.25 || 0.5;

  return (
    <div>
      <div className="env-mini-head">
        <i className="env-key" style={{ background: color }} aria-hidden="true" />
        <span>{label}</span>
        <b style={{ color }}>
          {latest.toFixed(decimals)}
          {unit}
        </b>
      </div>
      <div style={{ width: "100%", height: showAxis ? 74 : 62 }}>
        <ResponsiveContainer>
          <AreaChart data={data} syncId={syncId} margin={{ top: 4, right: 6, bottom: 0, left: 0 }}>
            <defs>
              <linearGradient id={gradientId} x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stopColor={color} stopOpacity={0.22} />
                <stop offset="100%" stopColor={color} stopOpacity={0.02} />
              </linearGradient>
            </defs>
            <CartesianGrid stroke={BORDER} vertical={false} />
            <XAxis
              dataKey="ts"
              type="number"
              scale="time"
              domain={["dataMin", "dataMax"]}
              tickFormatter={(ts: number) =>
                new Date(ts).toLocaleTimeString(undefined, { hour: "numeric", minute: "2-digit" })
              }
              tick={{ fill: MUTED, fontSize: 10 }}
              tickMargin={6}
              minTickGap={48}
              axisLine={{ stroke: BORDER }}
              tickLine={false}
              height={showAxis ? 22 : 4}
              hide={!showAxis}
            />
            <YAxis
              domain={[lo - pad, hi + pad]}
              ticks={[lo, hi]}
              tickFormatter={(v: number) => v.toFixed(decimals)}
              tick={{ fill: MUTED, fontSize: 10 }}
              width={30}
              axisLine={false}
              tickLine={false}
            />
            <Tooltip
              cursor={{ stroke: MUTED, strokeOpacity: 0.45, strokeWidth: 1 }}
              isAnimationActive={false}
              content={({ active, payload, label: ts }) => {
                if (!active || !payload?.length) return null;
                const v = payload[0]!.value as number | null;
                return (
                  <div className="env-tip">
                    <b style={{ color }}>{v == null ? "—" : `${v.toFixed(decimals)}${unit}`}</b>
                    <span>{label}</span>
                    <span className="env-tip-ts">
                      {new Date(Number(ts)).toLocaleString(undefined, {
                        month: "short",
                        day: "numeric",
                        hour: "numeric",
                        minute: "2-digit",
                      })}
                    </span>
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
              dot={false}
              activeDot={{ r: 3.5, fill: color, stroke: "#fff", strokeWidth: 2 }}
              isAnimationActive={false}
              connectNulls
            />
          </AreaChart>
        </ResponsiveContainer>
      </div>
    </div>
  );
}

export function IncubatorEnvChart({ history, id }: { history: IncubatorHistory; id: string }) {
  const sorted = [...history.points].sort((a, b) => Date.parse(a.ts) - Date.parse(b.ts));
  const temp = sorted.map((p) => ({ ts: Date.parse(p.ts), value: p.tempC }));
  const humidity = sorted.map((p) => ({ ts: Date.parse(p.ts), value: p.humidityPct }));

  if (sorted.length < 2) {
    return (
      <p className="muted" style={{ margin: "0.5rem 0" }}>
        Not enough recent telemetry to chart yet.
      </p>
    );
  }
  return (
    <div className="env-mini">
      <MiniEnvPanel
        data={temp}
        label="Temp"
        unit="°C"
        color={ENV_TEMP}
        decimals={1}
        gradientId={`miniTemp-${id}`}
        syncId={`mini-${id}`}
        showAxis={false}
      />
      <MiniEnvPanel
        data={humidity}
        label="Humidity"
        unit="%"
        color={ENV_HUMIDITY}
        decimals={0}
        gradientId={`miniHum-${id}`}
        syncId={`mini-${id}`}
        showAxis
      />
    </div>
  );
}

export const chartColors = { ACCENT, ACCENT_SOFT, HUMIDITY, HUMIDITY_SOFT, WARN, DANGER, MUTED, ENV_TEMP, ENV_HUMIDITY };
