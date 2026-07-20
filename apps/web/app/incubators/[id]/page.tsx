"use client";
// US-ENV-002: history charts (24h / 7d / full-batch window) with min/max/avg.
// No charting library — payloads are small (downsampled server-side to
// ≤500 points), so a hand-rolled SVG polyline keeps this dependency-free.
import { useParams } from "next/navigation";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../../lib/api";
import type { Batch, DeviceConfig, Incubator } from "../../../lib/types";
import { useAuthedFarm } from "../../../lib/useAuthedFarm";

interface HistoryPoint {
  ts: string;
  tempC: number | null;
  humidityPct: number | null;
}

interface HistoryStat {
  min: number | null;
  max: number | null;
  avg: number | null;
}

interface History {
  range: string;
  from: string | null;
  to: string | null;
  summary: { temp: HistoryStat; humidity: HistoryStat };
  points: HistoryPoint[];
}

type RangeOption = "24h" | "7d" | "batch";

function fmt(n: number | null, unit: string) {
  return n == null ? "—" : `${n.toFixed(1)}${unit}`;
}

export default function IncubatorHistoryPage() {
  const farmId = useAuthedFarm();
  const { id } = useParams<{ id: string }>();
  const [incubator, setIncubator] = useState<Incubator | null>(null);
  const [activeBatch, setActiveBatch] = useState<Batch | null>(null);
  const [range, setRange] = useState<RangeOption>("24h");
  const [history, setHistory] = useState<History | null>(null);
  const [error, setError] = useState<string | null>(null);

  const reloadIncubator = useCallback(() => {
    if (!farmId || !id) return;
    api<Incubator>(`/v1/farms/${farmId}/incubators/${id}`).then(setIncubator);
  }, [farmId, id]);

  useEffect(() => {
    if (!farmId || !id) return;
    reloadIncubator();
    api<Batch[]>(`/v1/farms/${farmId}/batches`).then((all) => {
      const forThis = all.find((b) => b.incubatorId === id && b.status !== "aborted" && b.status !== "closed");
      setActiveBatch(forThis ?? null);
    });
    // Telemetry (incl. actuator on/off + override state) lands every ~60s;
    // poll a bit faster, same idiom as the incubators list page, so
    // SetpointsCard/ActuatorsCard reflect fresh device state without a
    // manual page refresh.
    const t = setInterval(reloadIncubator, 15_000);
    return () => clearInterval(t);
  }, [farmId, id, reloadIncubator]);

  const reload = useCallback(() => {
    if (!farmId || !id) return;
    if (range === "batch" && !activeBatch) return;
    setError(null);
    const qs = range === "batch" ? `range=batch&batchId=${activeBatch!.id}` : `range=${range}`;
    api<History>(`/v1/farms/${farmId}/incubators/${id}/history?${qs}`)
      .then(setHistory)
      .catch((err) => setError(err instanceof Error ? err.message : "Failed to load history"));
  }, [farmId, id, range, activeBatch]);
  useEffect(reload, [reload]);

  if (!farmId) return null;
  return (
    <>
      <h1>{incubator?.name ?? "Incubator"}</h1>
      {error && <p className="alert-error">{error}</p>}

      {incubator?.device && <SetpointsCard farmId={farmId} incubatorId={id} device={incubator.device} />}
      {incubator?.device && <ActuatorsCard farmId={farmId} incubatorId={id} device={incubator.device} />}

      <h2>History</h2>
      <div className="row" style={{ marginBottom: "1rem" }}>
        <button className={range === "24h" ? "primary" : ""} onClick={() => setRange("24h")}>
          24 hours
        </button>
        <button className={range === "7d" ? "primary" : ""} onClick={() => setRange("7d")}>
          7 days
        </button>
        {activeBatch && (
          <button className={range === "batch" ? "primary" : ""} onClick={() => setRange("batch")}>
            Full batch
          </button>
        )}
      </div>

      {history && (
        <>
          <div className="metrics card">
            <span className="stat">
              <b>{fmt(history.summary.temp.min, "°C")}</b>
              <span className="muted">temp min</span>
            </span>
            <span className="stat">
              <b>{fmt(history.summary.temp.avg, "°C")}</b>
              <span className="muted">temp avg</span>
            </span>
            <span className="stat">
              <b>{fmt(history.summary.temp.max, "°C")}</b>
              <span className="muted">temp max</span>
            </span>
            <span className="stat">
              <b>{fmt(history.summary.humidity.min, "%")}</b>
              <span className="muted">humidity min</span>
            </span>
            <span className="stat">
              <b>{fmt(history.summary.humidity.avg, "%")}</b>
              <span className="muted">humidity avg</span>
            </span>
            <span className="stat">
              <b>{fmt(history.summary.humidity.max, "%")}</b>
              <span className="muted">humidity max</span>
            </span>
          </div>

          {history.points.length === 0 ? (
            <p className="muted">No telemetry in this window.</p>
          ) : (
            <>
              <div className="card">
                <div className="muted" style={{ marginBottom: "0.4rem" }}>
                  Temperature (°C)
                </div>
                <LineChart points={history.points} pick={(p) => p.tempC} color="var(--danger)" />
              </div>
              <div className="card">
                <div className="muted" style={{ marginBottom: "0.4rem" }}>
                  Humidity (%)
                </div>
                <LineChart points={history.points} pick={(p) => p.humidityPct} color="var(--accent)" />
              </div>
            </>
          )}
        </>
      )}
    </>
  );
}

const CONFIG_STATE_LABEL: Record<string, string> = {
  sent: "sent — waiting for device",
  received: "received by device — applying",
  applied: "applied ✓",
  unconfirmed: "unconfirmed — device may be offline",
};

// US-INC-003: edit the four control-loop values already visible in
// telemetry (setTemp/setHum/setTempHyst/setHumHyst → Device.current*).
// Turner/fan/pump/mode aren't included — telemetry doesn't surface their
// current values yet, so a remote edit couldn't show what it's changing
// from (see docs/iot/mqtt-topics.md).
function SetpointsCard({
  farmId,
  incubatorId,
  device,
}: {
  farmId: string;
  incubatorId: string;
  device: NonNullable<Incubator["device"]>;
}) {
  const [form, setForm] = useState({
    tempSetpoint: device.currentTempSetpoint ?? "",
    tempHysteresis: device.currentTempHysteresis ?? "",
    humSetpoint: device.currentHumSetpoint ?? "",
    humHysteresis: device.currentHumHysteresis ?? "",
  });
  const [config, setConfig] = useState<DeviceConfig | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [saving, setSaving] = useState(false);

  const reloadConfig = useCallback(() => {
    api<DeviceConfig | null>(`/v1/farms/${farmId}/incubators/${incubatorId}/config`).then(setConfig);
  }, [farmId, incubatorId]);
  useEffect(reloadConfig, [reloadConfig]);

  // While a command hasn't reached a final state, poll to show sent -> received -> applied live.
  useEffect(() => {
    if (!config || config.state === "applied" || config.state === "unconfirmed") return;
    const t = setInterval(reloadConfig, 3_000);
    return () => clearInterval(t);
  }, [config, reloadConfig]);

  async function onSubmit(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    setError(null);
    setSaving(true);
    try {
      await api(`/v1/farms/${farmId}/incubators/${incubatorId}/config`, {
        method: "POST",
        body: {
          tempSetpoint: Number(form.tempSetpoint),
          tempHysteresis: Number(form.tempHysteresis),
          humSetpoint: Number(form.humSetpoint),
          humHysteresis: Number(form.humHysteresis),
        },
      });
      reloadConfig();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to send setpoints");
    } finally {
      setSaving(false);
    }
  }

  return (
    <div className="card">
      <h2 style={{ marginTop: 0 }}>Setpoints</h2>
      {error && <p className="alert-error">{error}</p>}
      <form className="row" onSubmit={onSubmit}>
        <label>
          Temp setpoint (°C)
          <input
            type="number"
            step="0.1"
            value={form.tempSetpoint}
            onChange={(e) => setForm({ ...form, tempSetpoint: e.target.value })}
            required
          />
        </label>
        <label>
          Temp hysteresis (°C)
          <input
            type="number"
            step="0.1"
            value={form.tempHysteresis}
            onChange={(e) => setForm({ ...form, tempHysteresis: e.target.value })}
            required
          />
        </label>
        <label>
          Humidity setpoint (%)
          <input
            type="number"
            step="1"
            value={form.humSetpoint}
            onChange={(e) => setForm({ ...form, humSetpoint: e.target.value })}
            required
          />
        </label>
        <label>
          Humidity hysteresis (%)
          <input
            type="number"
            step="1"
            value={form.humHysteresis}
            onChange={(e) => setForm({ ...form, humHysteresis: e.target.value })}
            required
          />
        </label>
        <button className="primary" disabled={saving}>
          {saving ? "Sending…" : "Send to device"}
        </button>
      </form>
      {config && (
        <p className={config.state === "unconfirmed" ? "badge warn" : "muted"} style={{ marginTop: "0.5rem" }}>
          v{config.version}: {CONFIG_STATE_LABEL[config.state] ?? config.state}
        </p>
      )}
    </div>
  );
}

const ACTUATORS = [
  { key: "fan", label: "Fan" },
  { key: "turner", label: "Turner" },
  { key: "humidifier", label: "Humidifier" },
  { key: "pump", label: "Pump" },
] as const;

// Follow-up to SetpointsCard's US-INC-003 pattern, now that telemetry
// mirrors fan/turner/humidifier/pump on-off + override state (see
// docs/iot/mqtt-topics.md "Commands"). Each row toggles that one
// actuator's remote override on/off via the same /config endpoint and
// sent->received->applied pipeline.
function ActuatorsCard({
  farmId,
  incubatorId,
  device,
}: {
  farmId: string;
  incubatorId: string;
  device: NonNullable<Incubator["device"]>;
}) {
  const [config, setConfig] = useState<DeviceConfig | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [pending, setPending] = useState<string | null>(null);

  const reloadConfig = useCallback(() => {
    api<DeviceConfig | null>(`/v1/farms/${farmId}/incubators/${incubatorId}/config`).then(setConfig);
  }, [farmId, incubatorId]);
  useEffect(reloadConfig, [reloadConfig]);

  useEffect(() => {
    if (!config || config.state === "applied" || config.state === "unconfirmed") return;
    const t = setInterval(reloadConfig, 3_000);
    return () => clearInterval(t);
  }, [config, reloadConfig]);

  async function toggle(key: (typeof ACTUATORS)[number]["key"], nextOn: boolean) {
    setError(null);
    setPending(key);
    try {
      const result = await api<DeviceConfig>(`/v1/farms/${farmId}/incubators/${incubatorId}/config`, {
        method: "POST",
        body: { [`${key}Override`]: true, [`${key}On`]: nextOn },
      });
      setConfig(result);
    } catch (err) {
      setError(err instanceof Error ? err.message : `Failed to toggle ${key}`);
    } finally {
      setPending(null);
    }
  }

  async function setAuto(key: (typeof ACTUATORS)[number]["key"]) {
    setError(null);
    setPending(key);
    try {
      const result = await api<DeviceConfig>(`/v1/farms/${farmId}/incubators/${incubatorId}/config`, {
        method: "POST",
        body: { [`${key}Override`]: false },
      });
      setConfig(result);
    } catch (err) {
      setError(err instanceof Error ? err.message : `Failed to reset ${key} to auto`);
    } finally {
      setPending(null);
    }
  }

  return (
    <div className="card">
      <h2 style={{ marginTop: 0 }}>Actuators</h2>
      {error && <p className="alert-error">{error}</p>}
      <div className="row">
        {ACTUATORS.map(({ key, label }) => {
          const on = device[`current${label}On` as keyof typeof device] as boolean | null;
          const override = device[`current${label}Override` as keyof typeof device] as boolean | null;
          return (
            <div key={key} className="card" style={{ minWidth: "160px" }}>
              <div style={{ fontWeight: 600 }}>{label}</div>
              <p className="muted" style={{ margin: "0.25rem 0" }}>
                {on == null ? "no telemetry yet" : on ? "on" : "off"} — {override ? "manual" : "auto"}
              </p>
              <div className="row">
                <button
                  className={on ? "" : "primary"}
                  disabled={pending === key}
                  onClick={() => toggle(key, false)}
                >
                  Off
                </button>
                <button className={on ? "primary" : ""} disabled={pending === key} onClick={() => toggle(key, true)}>
                  On
                </button>
                {override && (
                  <button disabled={pending === key} onClick={() => setAuto(key)}>
                    Auto
                  </button>
                )}
              </div>
            </div>
          );
        })}
      </div>
      {config && (
        <p className={config.state === "unconfirmed" ? "badge warn" : "muted"} style={{ marginTop: "0.5rem" }}>
          v{config.version}: {CONFIG_STATE_LABEL[config.state] ?? config.state}
        </p>
      )}
    </div>
  );
}

function LineChart({
  points,
  pick,
  color,
}: {
  points: HistoryPoint[];
  pick: (p: HistoryPoint) => number | null;
  color: string;
}) {
  const width = 880;
  const height = 220;
  const padding = 24;

  const series = points.map((p) => ({ x: Date.parse(p.ts), y: pick(p) })).filter((p) => p.y != null) as {
    x: number;
    y: number;
  }[];
  if (series.length < 2) return <p className="muted">Not enough data points yet.</p>;

  const xMin = series[0]!.x;
  const xMax = series[series.length - 1]!.x;
  const yMin = Math.min(...series.map((p) => p.y));
  const yMax = Math.max(...series.map((p) => p.y));
  const yPad = (yMax - yMin) * 0.1 || 1;

  const toX = (x: number) => padding + ((x - xMin) / (xMax - xMin || 1)) * (width - 2 * padding);
  const toY = (y: number) =>
    height - padding - ((y - (yMin - yPad)) / (yMax + yPad - (yMin - yPad) || 1)) * (height - 2 * padding);

  const path = series.map((p, i) => `${i === 0 ? "M" : "L"}${toX(p.x).toFixed(1)},${toY(p.y).toFixed(1)}`).join(" ");

  return (
    <svg viewBox={`0 0 ${width} ${height}`} style={{ width: "100%", height: "auto", display: "block" }}>
      <line x1={padding} y1={height - padding} x2={width - padding} y2={height - padding} stroke="var(--border)" />
      <path d={path} fill="none" stroke={color} strokeWidth={2} />
      <text x={padding} y={14} className="muted" fontSize={11} fill="var(--muted)">
        {yMax.toFixed(1)}
      </text>
      <text x={padding} y={height - padding - 4} className="muted" fontSize={11} fill="var(--muted)">
        {yMin.toFixed(1)}
      </text>
    </svg>
  );
}
