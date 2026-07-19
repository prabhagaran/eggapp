"use client";
import Link from "next/link";
import { useRouter } from "next/navigation";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../lib/api";
import type { Batch, EggCollection, Incubator, Species } from "../../lib/types";
import { batchBadgeClass, dayOf, fmtDate, useAuthedFarm } from "../../lib/useAuthedFarm";

function ageDays(collectedOn: string) {
  return Math.floor((Date.now() - Date.parse(collectedOn)) / 86_400_000);
}

export default function BatchesPage() {
  const farmId = useAuthedFarm();
  const router = useRouter();
  const [rows, setRows] = useState<Batch[] | null>(null);
  const [species, setSpecies] = useState<Species[]>([]);
  const [incubators, setIncubators] = useState<Incubator[]>([]);
  const [collections, setCollections] = useState<EggCollection[]>([]);
  const [picked, setPicked] = useState<Record<string, number>>({});
  const [error, setError] = useState<string | null>(null);

  const reload = useCallback(() => {
    if (!farmId) return;
    api<Batch[]>(`/v1/farms/${farmId}/batches`).then(setRows);
    api<EggCollection[]>(`/v1/farms/${farmId}/collections`).then((cs) =>
      setCollections(cs.filter((c) => c.availableCount > 0)),
    );
    api<Incubator[]>(`/v1/farms/${farmId}/incubators`).then(setIncubators);
  }, [farmId]);
  useEffect(() => {
    reload();
    api<Species[]>("/v1/species").then(setSpecies);
  }, [reload]);

  async function onCreate(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    const sources = Object.entries(picked)
      .filter(([, n]) => n > 0)
      .map(([collectionId, count]) => ({ collectionId, count }));
    if (sources.length === 0) {
      setError("Pick at least one collection (set a count > 0)");
      return;
    }
    setError(null);
    try {
      const res = await api<{ batch: Batch; warnings: string[] }>(`/v1/farms/${farmId}/batches`, {
        method: "POST",
        body: {
          incubatorId: f.get("incubatorId"),
          speciesId: f.get("speciesId"),
          sources,
          overrideNote: f.get("overrideNote") || undefined,
        },
      });
      if (res.warnings.length > 0) window.alert(`Created with warnings:\n${res.warnings.join("\n")}`);
      router.push(`/batches/${res.batch.id}`);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  if (!farmId) return null;
  const oldEggsPicked = collections.some((c) => picked[c.id] && ageDays(c.collectedOn) > 14);

  return (
    <>
      <h1>Batches</h1>
      {error && <p className="alert-error">{error}</p>}

      <div className="card">
        <h2 style={{ marginTop: 0 }}>New batch</h2>
        {collections.length === 0 ? (
          <p className="muted">
            No collections with available eggs. <Link href="/collections">Record a collection first →</Link>
          </p>
        ) : incubators.length === 0 ? (
          <p className="muted">
            No incubators. <Link href="/incubators">Add one first →</Link>
          </p>
        ) : (
          <form className="stack" style={{ maxWidth: 640 }} onSubmit={onCreate}>
            <div className="row">
              <label>
                Incubator
                <select name="incubatorId" required>
                  {incubators.map((i) => (
                    <option key={i.id} value={i.id}>
                      {i.name}
                    </option>
                  ))}
                </select>
              </label>
              <label>
                Species
                <select name="speciesId" required defaultValue="chicken">
                  {species.map((s) => (
                    <option key={s.id} value={s.id}>
                      {s.name} ({s.incubationDays} d)
                    </option>
                  ))}
                </select>
              </label>
            </div>
            <div className="table-wrap">
              <table>
                <thead>
                  <tr>
                    <th>Collection</th>
                    <th>Age</th>
                    <th>Available</th>
                    <th>Use</th>
                  </tr>
                </thead>
                <tbody>
                  {collections.map((c) => {
                    const age = ageDays(c.collectedOn);
                    return (
                      <tr key={c.id}>
                        <td>
                          {fmtDate(c.collectedOn)} <span className="muted">{c.sourceNote ?? ""}</span>
                        </td>
                        <td>
                          <span className={`badge ${age > 14 ? "danger" : age > 7 ? "warn" : "ok"}`}>{age} d</span>
                        </td>
                        <td>{c.availableCount}</td>
                        <td>
                          <input
                            type="number"
                            min={0}
                            max={c.availableCount}
                            value={picked[c.id] ?? 0}
                            onChange={(e) =>
                              setPicked((p) => ({ ...p, [c.id]: Number(e.target.value) }))
                            }
                          />
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>
            {oldEggsPicked && (
              <label>
                Override note (required — eggs older than 14 days)
                <input name="overrideNote" required placeholder="why these eggs are being set anyway" />
              </label>
            )}
            <button className="primary">Create batch</button>
          </form>
        )}
      </div>

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Batch</th>
              <th>Status</th>
              <th>Day</th>
              <th>Viable</th>
              <th>Fertility</th>
              <th>Hatch of set</th>
            </tr>
          </thead>
          <tbody>
            {(rows ?? []).map((b) => {
              const day = dayOf(b.setAt);
              return (
                <tr key={b.id}>
                  <td>
                    <Link href={`/batches/${b.id}`}>
                      {b.species?.name ?? b.speciesId} · {b.incubator?.name} · {fmtDate(b.setAt ?? null)}
                    </Link>
                  </td>
                  <td>
                    <span className={batchBadgeClass(b.status)}>{b.status}</span>
                  </td>
                  <td>{day != null && b.species ? `${day}/${b.species.incubationDays}` : "—"}</td>
                  <td>{b.viableCount}</td>
                  <td>{b.fertilityPct != null ? `${b.fertilityPct}%` : "—"}</td>
                  <td>{b.hatchOfSetPct != null ? `${b.hatchOfSetPct}%` : "—"}</td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
      {rows && rows.length === 0 && <p className="muted">No batches yet.</p>}
    </>
  );
}
