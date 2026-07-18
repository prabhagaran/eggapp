"use client";
import Link from "next/link";
import { useEffect, useState } from "react";
import { Egg, Bird, Thermometer, BellRing } from "lucide-react";
import { api } from "../lib/api";
import type { Alert, Batch, Flock, Incubator } from "../lib/types";
import { dayOf, fmtAge, isFresh, useAuthedFarm } from "../lib/useAuthedFarm";

const ACTIVE: Batch["status"][] = ["planned", "setting", "incubating", "lockdown", "hatching"];

export default function Dashboard() {
  const farmId = useAuthedFarm();
  const [incubators, setIncubators] = useState<Incubator[] | null>(null);
  const [batches, setBatches] = useState<Batch[] | null>(null);
  const [flocks, setFlocks] = useState<Flock[] | null>(null);
  const [alerts, setAlerts] = useState<Alert[] | null>(null);

  useEffect(() => {
    if (!farmId) return;
    const reload = () => {
      api<Incubator[]>(`/v1/farms/${farmId}/incubators`).then(setIncubators);
      api<Batch[]>(`/v1/farms/${farmId}/batches`).then(setBatches);
      api<Flock[]>(`/v1/farms/${farmId}/flocks`).then(setFlocks);
      api<Alert[]>(`/v1/farms/${farmId}/alerts?state=open`).then(setAlerts);
    };
    reload();
    // Telemetry lands every ~60s; poll a bit faster so live readings
    // update without a manual page refresh.
    const t = setInterval(reload, 15_000);
    return () => clearInterval(t);
  }, [farmId]);

  if (!farmId) return null;
  const active = (batches ?? []).filter((b) => ACTIVE.includes(b.status));
  const birds = (flocks ?? []).reduce((sum, f) => sum + f.currentCount, 0);
  const devicesOnline = (incubators ?? []).filter((i) => i.device?.status === "active").length;

  return (
    <>
      <h1>Dashboard</h1>
      <p className="muted">Plan, monitor, and act on your farm with ease.</p>

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
          </div>
        ))}
      </div>
    </>
  );
}
