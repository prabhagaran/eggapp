"use client";
import Link from "next/link";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../lib/api";
import type { Batch, Flock, Species } from "../../lib/types";
import { useAuthedFarm } from "../../lib/useAuthedFarm";

const STAGE_LABEL: Record<string, string> = {
  brooding: "Brooding",
  grower: "Grower",
  pre_lay: "Pre-lay",
  layer: "Layer",
  broiler_starter: "Broiler starter",
  broiler_grower: "Broiler grower",
  broiler_finisher: "Broiler finisher",
};

export default function FlocksPage() {
  const farmId = useAuthedFarm();
  const [rows, setRows] = useState<Flock[] | null>(null);
  const [species, setSpecies] = useState<Species[]>([]);
  const [completedHatches, setCompletedHatches] = useState<{ id: string; label: string }[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [origin, setOrigin] = useState<"hatch" | "manual">("manual");

  const reload = useCallback(() => {
    if (farmId) api<Flock[]>(`/v1/farms/${farmId}/flocks`).then(setRows);
  }, [farmId]);
  useEffect(() => {
    reload();
    api<Species[]>("/v1/species").then(setSpecies);
    if (farmId) {
      api<Batch[]>(`/v1/farms/${farmId}/batches?status=completed`).then((batches) =>
        setCompletedHatches(
          batches
            .filter((b) => b.hatch && !b.hatch.flock)
            .map((b) => ({ id: b.hatch!.id, label: `${b.species?.name ?? b.speciesId} batch` })),
        ),
      );
    }
  }, [farmId, reload]);

  async function onCreate(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const form = e.currentTarget;
    const f = new FormData(form);
    setError(null);
    try {
      const body: Record<string, unknown> = {
        name: f.get("name"),
        speciesId: f.get("speciesId"),
        purpose: f.get("purpose"),
        placedCount: Number(f.get("placedCount")),
      };
      if (origin === "manual") {
        body.acquisitionDate = new Date(String(f.get("acquisitionDate"))).toISOString();
        body.acquisitionAgeDays = Number(f.get("acquisitionAgeDays"));
        body.acquisitionNote = f.get("acquisitionNote") || undefined;
      } else {
        body.hatchEventId = f.get("hatchBatchId");
      }
      await api(`/v1/farms/${farmId}/flocks`, { method: "POST", body });
      form.reset();
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  if (!farmId) return null;
  return (
    <>
      <h1>Flocks</h1>
      {error && <p className="alert-error">{error}</p>}

      <div className="card">
        <form className="stack" onSubmit={onCreate}>
          <label>
            Name
            <input name="name" required placeholder="Coop 1 layers" />
          </label>
          <label>
            Species
            <select name="speciesId" required defaultValue="">
              <option value="" disabled>
                —
              </option>
              {species.map((s) => (
                <option key={s.id} value={s.id}>
                  {s.name}
                </option>
              ))}
            </select>
          </label>
          <label>
            Purpose
            <select name="purpose" required defaultValue="layer">
              <option value="layer">Layer</option>
              <option value="broiler">Broiler</option>
              <option value="breeder">Breeder</option>
            </select>
          </label>
          <label>
            Placed count
            <input name="placedCount" type="number" min={1} required />
          </label>

          <div className="row">
            <label>
              <input type="radio" name="origin" checked={origin === "manual"} onChange={() => setOrigin("manual")} />
              Manually acquired
            </label>
            <label>
              <input
                type="radio"
                name="origin"
                checked={origin === "hatch"}
                onChange={() => setOrigin("hatch")}
                disabled={completedHatches.length === 0}
              />
              From a completed hatch
            </label>
          </div>

          {origin === "manual" ? (
            <>
              <label>
                Acquisition date
                <input name="acquisitionDate" type="date" required defaultValue={new Date().toISOString().slice(0, 10)} />
              </label>
              <label>
                Age at acquisition (days)
                <input name="acquisitionAgeDays" type="number" min={0} required />
              </label>
              <label>
                Provenance note
                <input name="acquisitionNote" placeholder="e.g. bought from Riverside Hatchery" />
              </label>
            </>
          ) : (
            <label>
              Hatch batch
              <select name="hatchBatchId" required defaultValue="">
                <option value="" disabled>
                  —
                </option>
                {completedHatches.map((h) => (
                  <option key={h.id} value={h.id}>
                    {h.label}
                  </option>
                ))}
              </select>
            </label>
          )}

          <button className="primary">Create flock</button>
        </form>
      </div>

      <div className="grid">
        {(rows ?? []).map((flock) => (
          <Link key={flock.id} href={`/flocks/${flock.id}`} className="card" style={{ display: "block" }}>
            <b>{flock.name}</b>
            <div className="muted">
              {flock.species?.name ?? flock.speciesId} · {flock.purpose}
            </div>
            <div className="metrics" style={{ marginTop: "0.4rem" }}>
              <span className="stat">
                <b>{flock.currentCount}</b>
                <span className="muted">current</span>
              </span>
              <span className="stat">
                <b>{flock.ageDays ?? "—"}</b>
                <span className="muted">days old</span>
              </span>
              <span className="badge accent">{flock.stage ? STAGE_LABEL[flock.stage] ?? flock.stage : "—"}</span>
            </div>
          </Link>
        ))}
      </div>
      {rows && rows.length === 0 && <p className="muted">No flocks yet — add one above.</p>}
    </>
  );
}
