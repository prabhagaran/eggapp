"use client";
import Link from "next/link";
import { useEffect, useState } from "react";
import {
  Egg,
  Bird,
  Thermometer,
  BellRing,
  Droplets,
  Wind,
  Gauge,
  Sun,
  Wheat,
  Droplet,
} from "lucide-react";
import { api } from "../lib/api";
import { AlertsSeverityBars, BatchPipelineChart, IncubatorEnvChart, Sparkline } from "../components/DashboardCharts";
import { SensorTile } from "../components/SensorTile";
import type { Alert, Batch, Coop, Flock, Incubator, IncubatorHistory } from "../lib/types";
import { dayOf, fmtAge, isFresh, useAuthedFarm } from "../lib/useAuthedFarm";

const ACTIVE: Batch["status"][] = ["planned", "setting", "incubating", "lockdown", "hatching"];

// Optimal bands for the coop's live tiles (ADR 0009). Unlike an incubator
// — whose correct temperature depends on the species profile it's running,
// so its band comes from the device's own setpoints — a bird house has
// genuine species-independent comfort and welfare ranges, so fixed bands
// are right here.
const COOP_TEMP_RANGE = { min: 18, max: 30, warnBand: 4 };
const COOP_HUM_RANGE = { min: 50, max: 70, warnBand: 10 };
const CO2_RANGE = { max: 450, warnBand: 550 };      // >1000 ppm = critical
const NH3_RANGE = { max: 10, warnBand: 10 };        // >20 ppm = critical
const FEED_RANGE = { min: 20, warnBand: 10 };       // <10% = critical
const WATER_RANGE = { min: 25, warnBand: 15 };      // <10% = critical

// Last 8 ISO week buckets (Mon–Sun), oldest first — enough to show a
// meaningful trend without the sparkline turning into a full chart.
function weeklyBuckets(dates: string[], weeks = 8) {
  const now = new Date();
  const startOfWeek = (d: Date) => {
    const day = (d.getDay() + 6) % 7; // 0 = Monday
    const s = new Date(d);
    s.setHours(0, 0, 0, 0);
    s.setDate(s.getDate() - day);
    return s;
  };
  const thisWeekStart = startOfWeek(now);
  const buckets = Array.from({ length: weeks }, (_, i) => {
    const start = new Date(thisWeekStart);
    start.setDate(start.getDate() - (weeks - 1 - i) * 7);
    return { start, value: 0 };
  });
  for (const iso of dates) {
    const d = new Date(iso);
    for (let i = buckets.length - 1; i >= 0; i--) {
      if (d >= buckets[i]!.start) {
        buckets[i]!.value += 1;
        break;
      }
    }
  }
  return buckets.map((b) => ({ value: b.value }));
}

export default function Dashboard() {
  const farmId = useAuthedFarm();
  const [incubators, setIncubators] = useState<Incubator[] | null>(null);
  const [batches, setBatches] = useState<Batch[] | null>(null);
  const [flocks, setFlocks] = useState<Flock[] | null>(null);
  const [alerts, setAlerts] = useState<Alert[] | null>(null);
  const [coops, setCoops] = useState<Coop[] | null>(null);
  const [envHistory, setEnvHistory] = useState<Record<string, IncubatorHistory>>({});

  useEffect(() => {
    if (!farmId) return;
    const reload = () => {
      api<Incubator[]>(`/v1/farms/${farmId}/incubators`).then(setIncubators);
      api<Batch[]>(`/v1/farms/${farmId}/batches`).then(setBatches);
      api<Flock[]>(`/v1/farms/${farmId}/flocks`).then(setFlocks);
      api<Alert[]>(`/v1/farms/${farmId}/alerts?state=open`).then(setAlerts);
      api<Coop[]>(`/v1/farms/${farmId}/coops`).then(setCoops);
    };
    reload();
    // Telemetry lands every ~60s; poll a bit faster so live readings
    // update without a manual page refresh.
    const t = setInterval(reload, 15_000);
    return () => clearInterval(t);
  }, [farmId]);

  // One history fetch per incubator that has a device bound — separate
  // effect so it doesn't re-fire on every 15s poll above, just when the
  // incubator list itself changes (new one added/removed).
  useEffect(() => {
    if (!farmId || !incubators) return;
    for (const inc of incubators) {
      if (!inc.device) continue;
      api<IncubatorHistory>(`/v1/farms/${farmId}/incubators/${inc.id}/history?range=24h`).then((h) =>
        setEnvHistory((prev) => ({ ...prev, [inc.id]: h })),
      );
    }
  }, [farmId, incubators?.map((i) => i.id).join(",")]);

  if (!farmId) return null;
  const active = (batches ?? []).filter((b) => ACTIVE.includes(b.status));
  const birds = (flocks ?? []).reduce((sum, f) => sum + f.currentCount, 0);
  const devicesOnline = (incubators ?? []).filter((i) => i.device?.status === "active").length;
  const warningAlerts = (alerts ?? []).filter((a) => a.severity === "warning").length;
  const criticalAlerts = (alerts ?? []).filter((a) => a.severity === "critical").length;

  const batchTrend = batches ? weeklyBuckets(batches.map((b) => b.createdAt)) : [];
  const pipelineCounts = (batches ?? []).reduce<Record<string, number>>((acc, b) => {
    acc[b.status] = (acc[b.status] ?? 0) + 1;
    return acc;
  }, {});

  // The live-monitoring band shows one coop at a time (ADR 0009 — these
  // are house measurements, not incubator ones). Pick the coop with the
  // freshest reading rather than the first in the list, so a stale or
  // offline node doesn't mask a live one.
  const monitored = (coops ?? [])
    .filter((c) => c.latestTelemetry)
    .sort((a, b) => Date.parse(b.latestTelemetry!.ts) - Date.parse(a.latestTelemetry!.ts))[0];
  const t = monitored?.latestTelemetry;
  const live = t ? isFresh(t.ts) : false;

  return (
    <>
      <h1>Dashboard</h1>
      <p className="muted">Plan, monitor, and act on your farm with ease.</p>

      {/* A farm with no coop monitor gets no tiles at all — the correct
          empty state. Rendering the grid with every value dashed would
          imply broken sensors rather than "you haven't set this up". */}
      {monitored ? (
        <>
          <section className="live-band">
            <div>
              <b>Real-Time Coop Monitoring</b>
              <span>
                Live data from {monitored.device?.name ?? monitored.device?.hardwareId ?? "device"} — {monitored.name}
              </span>
            </div>
            <div className={`live-chip${live ? "" : " stale"}`}>
              <span className="dot" />
              <span>
                <b>{live ? "Connected" : "No recent data"}</b>
                <span>Last update: {fmtAge(t!.ts)}</span>
              </span>
            </div>
          </section>

          <h2 className="section-rule">Environmental Conditions</h2>
          <div className="sensor-grid">
            <SensorTile icon={<Thermometer />} label="Temperature" value={t!.tempC} unit="°C" range={COOP_TEMP_RANGE} />
            <SensorTile icon={<Droplets />} label="Humidity" value={t!.humidityPct} unit="%" range={COOP_HUM_RANGE}
              decimals={0} />
            <SensorTile icon={<Wind />} label="CO₂ Level" value={t!.co2Ppm} unit="ppm" range={CO2_RANGE} decimals={0} />
            <SensorTile icon={<Gauge />} label="Ammonia" value={t!.ammoniaPpm} unit="ppm" range={NH3_RANGE} />
          </div>

          <h2 className="section-rule">Resource Management</h2>
          <div className="sensor-grid">
            <SensorTile icon={<Sun />} label="Light Intensity" value={t!.lightLux} unit="lux" decimals={0}
              hint="Ambient light" />
            <SensorTile icon={<Wheat />} label="Feed Level" value={t!.feedLevelPct} unit="%" range={FEED_RANGE}
              hint="Refill at <20%" />
            <SensorTile icon={<Droplet />} label="Water Level" value={t!.waterLevelPct} unit="%" range={WATER_RANGE}
              hint="Refill at <25%" />
          </div>
        </>
      ) : (
        coops != null && (
          <section className="live-band">
            <div>
              <b>Real-Time Coop Monitoring</b>
              <span>
                {coops.length === 0
                  ? "No coops yet — add one to monitor house conditions"
                  : "No coop is reporting telemetry yet — bind a monitor device"}
              </span>
            </div>
            <Link href="/coops" className="live-chip">
              <span>
                <b>{coops.length === 0 ? "Add a coop →" : "Set up monitoring →"}</b>
              </span>
            </Link>
          </section>
        )
      )}

      <h2 className="section-rule">Farm Overview</h2>
      <div className="stat-row">
        <div className="card dark stat-card">
          <div className="stat-top">
            <span className="stat-label">Total Birds</span>
            <span className="stat-icon"><Bird /></span>
          </div>
          <span className="stat-value">{flocks ? birds : "—"}</span>
          <span className="muted">across {flocks?.length ?? 0} flocks</span>
        </div>
        <div className="card stat-card">
          <div className="stat-top">
            <span className="stat-label">Active Batches</span>
            <span className="stat-icon"><Egg /></span>
          </div>
          <span className="stat-value">{batches ? active.length : "—"}</span>
          <span className="muted">of {batches?.length ?? 0} total</span>
          {batches && batchTrend.some((b) => b.value > 0) && <Sparkline data={batchTrend} />}
        </div>
        <div className="card stat-card">
          <div className="stat-top">
            <span className="stat-label">Incubators Online</span>
            <span className="stat-icon"><Thermometer /></span>
          </div>
          <span className="stat-value">{incubators ? devicesOnline : "—"}</span>
          <span className="muted">of {incubators?.length ?? 0} total</span>
        </div>
        <div className="card stat-card">
          <div className="stat-top">
            <span className="stat-label">Open Alerts</span>
            <span className="stat-icon"><BellRing /></span>
          </div>
          <span className="stat-value">{alerts ? alerts.length : "—"}</span>
          <span className="muted">
            <Link href="/alerts">review →</Link>
          </span>
        </div>
      </div>

      <div className="grid-2">
        <div className="card">
          <b>Batch pipeline</b>
          <p className="muted" style={{ marginTop: "0.15rem" }}>Where every batch sits right now</p>
          {batches ? <BatchPipelineChart counts={pipelineCounts} /> : <p className="muted">Loading…</p>}
        </div>
        <div className="card">
          <b>Open alerts by severity</b>
          <p className="muted" style={{ marginTop: "0.15rem" }}>{alerts ? alerts.length : "—"} open right now</p>
          {alerts && (
            <div style={{ marginTop: "1rem" }}>
              <AlertsSeverityBars warning={warningAlerts} critical={criticalAlerts} />
            </div>
          )}
        </div>
      </div>

      <h2>Active batches</h2>
      {batches && active.length === 0 && (
        <p className="muted">
          No active batches. <Link href="/batches">Start one →</Link>
        </p>
      )}
      <div className="grid">
        {active.map((b) => {
          const day = dayOf(b.setAt);
          return (
            <Link key={b.id} href={`/batches/${b.id}`} className="card">
              <b>
                {b.species?.name ?? b.speciesId} — {b.incubator?.name}
              </b>
              <div className="muted">
                <span className="badge accent">{b.status}</span>{" "}
                {day != null && b.species ? `Day ${day} of ${b.species.incubationDays}` : "not set yet"}
              </div>
              <div>{b.viableCount} viable eggs</div>
              {b.fertilityPct != null && <div className="muted">Fertility {b.fertilityPct}%</div>}
            </Link>
          );
        })}
      </div>

      <h2>Incubators</h2>
      {incubators && incubators.length === 0 && (
        <p className="muted">
          No incubators yet. <Link href="/incubators">Add one →</Link>
        </p>
      )}
      <div className="grid">
        {(incubators ?? []).map((inc) => (
          <div key={inc.id} className="card">
            <b>{inc.name}</b>
            <div className="muted">capacity {inc.capacity}</div>
            {inc.device ? (
              <div>
                <span
                  className={`badge ${inc.device.status === "active" ? "ok" : inc.device.status === "offline" ? "danger" : ""}`}
                >
                  {inc.device.status}
                </span>{" "}
                <span className="muted">{inc.device.name ?? inc.device.hardwareId}</span>
              </div>
            ) : (
              <span className="badge warn">no device</span>
            )}
            {inc.latestTelemetry && (
              <div className="metrics" style={{ marginTop: "0.4rem" }}>
                <span className="stat">
                  <b>{inc.latestTelemetry.tempC != null ? `${inc.latestTelemetry.tempC.toFixed(1)}°C` : "—"}</b>
                </span>
                <span className="stat">
                  <b>{inc.latestTelemetry.humidityPct != null ? `${Math.round(inc.latestTelemetry.humidityPct)}%` : "—"}</b>
                </span>
                <span
                  className={`muted ${isFresh(inc.latestTelemetry.ts) ? "" : "alert-warn"}`}
                  style={{ alignSelf: "center" }}
                >
                  {fmtAge(inc.latestTelemetry.ts)}
                </span>
              </div>
            )}
            {inc.device && envHistory[inc.id] && (
              <div style={{ marginTop: "0.6rem" }}>
                <IncubatorEnvChart history={envHistory[inc.id]!} />
              </div>
            )}
          </div>
        ))}
      </div>
    </>
  );
}
