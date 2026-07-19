"use client";
import Link from "next/link";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../lib/api";
import type { Incubator, Species } from "../../lib/types";
import { fmtAge, isFresh, useAuthedFarm } from "../../lib/useAuthedFarm";

export default function IncubatorsPage() {
  const farmId = useAuthedFarm();
  const [rows, setRows] = useState<Incubator[] | null>(null);
  const [species, setSpecies] = useState<Species[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [editingId, setEditingId] = useState<string | null>(null);

  const reload = useCallback(() => {
    if (farmId) api<Incubator[]>(`/v1/farms/${farmId}/incubators`).then(setRows);
  }, [farmId]);
  useEffect(() => {
    reload();
    api<Species[]>("/v1/species").then(setSpecies);
    // Telemetry lands every ~60s; poll a bit faster so a fresh reading
    // shows up without the user having to manually refresh the page.
    const t = setInterval(reload, 15_000);
    return () => clearInterval(t);
  }, [reload]);

  async function onCreate(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const form = e.currentTarget;
    const f = new FormData(form);
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/incubators`, {
        method: "POST",
        body: {
          name: f.get("name"),
          capacity: Number(f.get("capacity")),
          defaultSpeciesId: f.get("defaultSpeciesId") || undefined,
        },
      });
      form.reset();
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onSaveEdit(e: React.FormEvent<HTMLFormElement>, incId: string) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/incubators/${incId}`, {
        method: "PATCH",
        body: {
          name: f.get("name"),
          capacity: Number(f.get("capacity")),
          defaultSpeciesId: f.get("defaultSpeciesId") || null,
        },
      });
      setEditingId(null);
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  if (!farmId) return null;
  return (
    <>
      <h1>Incubators</h1>
      {error && <p className="alert-error">{error}</p>}

      <div className="card">
        <form className="row" onSubmit={onCreate}>
          <label>
            Name
            <input name="name" required placeholder="Incubator A" />
          </label>
          <label>
            Capacity (eggs)
            <input name="capacity" type="number" min={1} required />
          </label>
          <label>
            Default species
            <select name="defaultSpeciesId" defaultValue="">
              <option value="">—</option>
              {species.map((s) => (
                <option key={s.id} value={s.id}>
                  {s.name}
                </option>
              ))}
            </select>
          </label>
          <button className="primary">Add incubator</button>
        </form>
      </div>

      <div className="grid">
        {(rows ?? []).map((inc) =>
          editingId === inc.id ? (
            <div key={inc.id} className="card">
              <form className="stack" onSubmit={(e) => onSaveEdit(e, inc.id)}>
                <label>
                  Name
                  <input name="name" required defaultValue={inc.name} />
                </label>
                <label>
                  Capacity (eggs)
                  <input name="capacity" type="number" min={1} required defaultValue={inc.capacity} />
                </label>
                <label>
                  Default species
                  <select name="defaultSpeciesId" defaultValue={inc.defaultSpeciesId ?? ""}>
                    <option value="">—</option>
                    {species.map((s) => (
                      <option key={s.id} value={s.id}>
                        {s.name}
                      </option>
                    ))}
                  </select>
                </label>
                <div className="row">
                  <button className="primary">Save</button>
                  <button type="button" className="secondary" onClick={() => setEditingId(null)}>
                    Cancel
                  </button>
                </div>
              </form>
            </div>
          ) : (
          <div key={inc.id} className="card">
            <div style={{ display: "flex", justifyContent: "space-between" }}>
              <b>{inc.name}</b>
              <button className="secondary" onClick={() => setEditingId(inc.id)}>
                Edit
              </button>
            </div>
            <div className="muted">
              capacity {inc.capacity}
              {inc.defaultSpeciesId
                ? ` · ${species.find((s) => s.id === inc.defaultSpeciesId)?.name ?? inc.defaultSpeciesId}`
                : ""}
            </div>
            {inc.device ? (
              <div>
                <span className={`badge ${inc.device.status === "active" ? "ok" : ""}`}>{inc.device.status}</span>{" "}
                <span className="muted">{inc.device.name ?? inc.device.hardwareId}</span>
              </div>
            ) : (
              <div>
                <span className="badge warn">no device bound</span>{" "}
                <span className="muted">bind one on the Devices page</span>
              </div>
            )}
            {inc.latestTelemetry && (
              <div className="metrics" style={{ marginTop: "0.4rem" }}>
                <span className="stat">
                  <b>{inc.latestTelemetry.tempC != null ? `${inc.latestTelemetry.tempC.toFixed(1)}°C` : "—"}</b>
                  <span className="muted">temp</span>
                </span>
                <span className="stat">
                  <b>{inc.latestTelemetry.humidityPct != null ? `${Math.round(inc.latestTelemetry.humidityPct)}%` : "—"}</b>
                  <span className="muted">humidity</span>
                </span>
                <span className={isFresh(inc.latestTelemetry.ts) ? "muted" : "badge warn"}>
                  {fmtAge(inc.latestTelemetry.ts)}
                </span>
              </div>
            )}
            <div style={{ marginTop: "0.5rem" }}>
              <Link href={`/incubators/${inc.id}`}>History →</Link>
            </div>
          </div>
          ),
        )}
      </div>
      {rows && rows.length === 0 && <p className="muted">No incubators yet — add your first above.</p>}
    </>
  );
}
