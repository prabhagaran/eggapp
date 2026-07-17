"use client";
import Link from "next/link";
import { useEffect, useState } from "react";
import { api } from "../lib/api";
import type { Batch, Incubator } from "../lib/types";
import { dayOf, useAuthedFarm } from "../lib/useAuthedFarm";

const ACTIVE: Batch["status"][] = ["planned", "setting", "incubating", "lockdown", "hatching"];

export default function Dashboard() {
  const farmId = useAuthedFarm();
  const [incubators, setIncubators] = useState<Incubator[] | null>(null);
  const [batches, setBatches] = useState<Batch[] | null>(null);

  useEffect(() => {
    if (!farmId) return;
    api<Incubator[]>(`/v1/farms/${farmId}/incubators`).then(setIncubators);
    api<Batch[]>(`/v1/farms/${farmId}/batches`).then(setBatches);
  }, [farmId]);

  if (!farmId) return null;
  const active = (batches ?? []).filter((b) => ACTIVE.includes(b.status));

  return (
    <>
      <h1>Dashboard</h1>

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
          </div>
        ))}
      </div>
    </>
  );
}
