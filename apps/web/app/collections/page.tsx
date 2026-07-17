"use client";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../lib/api";
import type { EggCollection } from "../../lib/types";
import { fmtDate, useAuthedFarm } from "../../lib/useAuthedFarm";

function ageDays(collectedOn: string) {
  return Math.floor((Date.now() - Date.parse(collectedOn)) / 86_400_000);
}

export default function CollectionsPage() {
  const farmId = useAuthedFarm();
  const [rows, setRows] = useState<EggCollection[] | null>(null);
  const [error, setError] = useState<string | null>(null);

  const reload = useCallback(() => {
    if (farmId) api<EggCollection[]>(`/v1/farms/${farmId}/collections`).then(setRows);
  }, [farmId]);
  useEffect(reload, [reload]);

  async function onCreate(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const form = e.currentTarget;
    const f = new FormData(form);
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/collections`, {
        method: "POST",
        body: {
          collectedOn: new Date(String(f.get("collectedOn"))).toISOString(),
          count: Number(f.get("count")),
          sourceNote: f.get("sourceNote") || undefined,
        },
      });
      form.reset();
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onDiscard(id: string) {
    const count = Number(window.prompt("How many eggs to discard?"));
    if (!count || count < 1) return;
    const reason = window.prompt("Reason (cracked, dirty, …)?");
    if (!reason) return;
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/collections/${id}/discard`, {
        method: "POST",
        body: { count, reason },
      });
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  if (!farmId) return null;
  return (
    <>
      <h1>Egg collections</h1>
      {error && <p className="alert-error">{error}</p>}

      <div className="card">
        <form className="row" onSubmit={onCreate}>
          <label>
            Collected on
            <input name="collectedOn" type="date" required defaultValue={new Date().toISOString().slice(0, 10)} />
          </label>
          <label>
            Count
            <input name="count" type="number" min={1} required />
          </label>
          <label>
            Source note
            <input name="sourceNote" placeholder="e.g. own hens, coop 1" />
          </label>
          <button className="primary">Record collection</button>
        </form>
      </div>

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Collected</th>
              <th>Age</th>
              <th>Count</th>
              <th>Discarded</th>
              <th>Assigned</th>
              <th>Available</th>
              <th>Source</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {(rows ?? []).map((c) => {
              const age = ageDays(c.collectedOn);
              return (
                <tr key={c.id}>
                  <td>{fmtDate(c.collectedOn)}</td>
                  <td>
                    <span className={`badge ${age > 14 ? "danger" : age > 7 ? "warn" : "ok"}`}>{age} d</span>
                  </td>
                  <td>{c.count}</td>
                  <td>{c.discardedCount}</td>
                  <td>{c.assignedCount}</td>
                  <td>
                    <b>{c.availableCount}</b>
                  </td>
                  <td className="muted">{c.sourceNote ?? "—"}</td>
                  <td>
                    {c.availableCount > 0 && (
                      <button className="secondary" onClick={() => onDiscard(c.id)}>
                        Discard…
                      </button>
                    )}
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
      {rows && rows.length === 0 && <p className="muted">No collections recorded yet.</p>}
    </>
  );
}
