"use client";
import { useParams } from "next/navigation";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../../lib/api";
import type { BatchDetail } from "../../../lib/types";
import { batchBadgeClass, dayOf, fmtDate, useAuthedFarm } from "../../../lib/useAuthedFarm";

export default function BatchDetailPage() {
  const farmId = useAuthedFarm();
  const { id } = useParams<{ id: string }>();
  const [batch, setBatch] = useState<BatchDetail | null>(null);
  const [error, setError] = useState<string | null>(null);

  const reload = useCallback(() => {
    if (farmId && id) api<BatchDetail>(`/v1/farms/${farmId}/batches/${id}`).then(setBatch);
  }, [farmId, id]);
  useEffect(reload, [reload]);

  async function post(path: string, body?: unknown) {
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/batches/${id}${path}`, { method: "POST", body });
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onCandle(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    await post("/candlings", {
      dayNo: Number(f.get("dayNo")),
      candledAt: new Date().toISOString(),
      fertile: Number(f.get("fertile")),
      clear: Number(f.get("clear")),
      bloodRing: Number(f.get("bloodRing")),
      unsure: Number(f.get("unsure")),
      discrepancyNote: f.get("discrepancyNote") || undefined,
    });
  }

  async function onHatch(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    await post("/hatch", {
      hatchedAt: new Date().toISOString(),
      hatched: Number(f.get("hatched")),
      pippedDead: Number(f.get("pippedDead")),
      deadInShell: Number(f.get("deadInShell")),
      unhatched: Number(f.get("unhatched")),
      discrepancyNote: f.get("discrepancyNote") || undefined,
    });
  }

  if (!farmId || !batch) return null;
  const day = dayOf(batch.setAt);
  const nextCandle = batch.candlingDays.find((d) => !batch.candlings.some((c) => c.dayNo === d));
  const active = !["completed", "closed", "aborted"].includes(batch.status);

  return (
    <>
      <h1>
        {batch.species.name} · {batch.incubator.name}{" "}
        <span className={batchBadgeClass(batch.status)}>{batch.status}</span>
      </h1>
      {error && <p className="alert-error">{error}</p>}

      <div className="card metrics">
        <span className="stat">
          <b>{batch.viableCount}</b>
          <span className="muted">viable</span>
        </span>
        <span className="stat">
          <b>{day != null ? `${day}/${batch.species.incubationDays}` : "—"}</b>
          <span className="muted">day</span>
        </span>
        <span className="stat">
          <b>{batch.fertilityPct != null ? `${batch.fertilityPct}%` : "—"}</b>
          <span className="muted">fertility</span>
        </span>
        <span className="stat">
          <b>{batch.hatchOfSetPct != null ? `${batch.hatchOfSetPct}%` : "—"}</b>
          <span className="muted">hatch of set</span>
        </span>
        <span className="stat">
          <b>{batch.hatchOfFertilePct != null ? `${batch.hatchOfFertilePct}%` : "—"}</b>
          <span className="muted">hatch of fertile</span>
        </span>
      </div>

      <div className="card">
        <b>Schedule</b>
        <div className="muted">
          set {fmtDate(batch.setAt)} · candling days {batch.candlingDays.join(", ")} · lockdown{" "}
          {fmtDate(batch.lockdownAt)} · expected hatch {fmtDate(batch.expectedHatchAt)}
        </div>
        {batch.abortReason && <p className="alert-error">Aborted: {batch.abortReason}</p>}
      </div>

      {(batch.status === "planned" || batch.status === "setting") && (
        <div className="card">
          <button className="primary" onClick={() => post("/set", {})}>
            Eggs are set — start incubation
          </button>
          <p className="muted">Fixes the lockdown and expected-hatch dates from the species schedule.</p>
        </div>
      )}

      {batch.status === "incubating" && (
        <>
          <div className="card">
            <h2 style={{ marginTop: 0 }}>Record candling {nextCandle ? `(day ${nextCandle})` : ""}</h2>
            <form className="stack" onSubmit={onCandle}>
              <div className="row">
                <label>
                  Day no
                  <input name="dayNo" type="number" min={1} required defaultValue={nextCandle} />
                </label>
                <label>
                  Fertile
                  <input name="fertile" type="number" min={0} required />
                </label>
                <label>
                  Clear
                  <input name="clear" type="number" min={0} required defaultValue={0} />
                </label>
                <label>
                  Blood ring
                  <input name="bloodRing" type="number" min={0} required defaultValue={0} />
                </label>
                <label>
                  Unsure
                  <input name="unsure" type="number" min={0} required defaultValue={0} />
                </label>
              </div>
              <label>
                Discrepancy note (only if counts don't sum to {batch.viableCount})
                <input name="discrepancyNote" />
              </label>
              <button className="primary">Save candling</button>
            </form>
          </div>
          <div className="card">
            <button className="secondary" onClick={() => post("/status", { status: "lockdown" })}>
              Enter lockdown (turner off, humidity up)
            </button>
          </div>
        </>
      )}

      {(batch.status === "lockdown" || batch.status === "hatching") && (
        <div className="card">
          <h2 style={{ marginTop: 0 }}>Record hatch outcome</h2>
          <form className="stack" onSubmit={onHatch}>
            <div className="row">
              <label>
                Hatched
                <input name="hatched" type="number" min={0} required />
              </label>
              <label>
                Pipped, died
                <input name="pippedDead" type="number" min={0} required defaultValue={0} />
              </label>
              <label>
                Dead in shell
                <input name="deadInShell" type="number" min={0} required defaultValue={0} />
              </label>
              <label>
                Unhatched
                <input name="unhatched" type="number" min={0} required defaultValue={0} />
              </label>
            </div>
            <label>
              Discrepancy note (only if counts don't sum to {batch.viableCount})
              <input name="discrepancyNote" />
            </label>
            <button className="primary">Save hatch — completes the batch</button>
          </form>
        </div>
      )}

      {batch.status === "completed" && (
        <div className="card">
          <button className="secondary" onClick={() => post("/status", { status: "closed" })}>
            Close batch
          </button>
        </div>
      )}

      {active && (
        <div className="card">
          <button
            className="danger"
            onClick={() => {
              const reason = window.prompt("Abort reason (required):");
              if (reason) post("/status", { status: "aborted", reason });
            }}
          >
            Abort batch…
          </button>
        </div>
      )}

      <h2>Candling sessions</h2>
      {batch.candlings.length === 0 ? (
        <p className="muted">None recorded.</p>
      ) : (
        <div className="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Day</th>
                <th>Date</th>
                <th>Fertile</th>
                <th>Clear</th>
                <th>Blood ring</th>
                <th>Unsure</th>
                <th>Note</th>
              </tr>
            </thead>
            <tbody>
              {batch.candlings.map((c) => (
                <tr key={c.id}>
                  <td>{c.dayNo}</td>
                  <td>{fmtDate(c.candledAt)}</td>
                  <td>{c.fertile}</td>
                  <td>{c.clear}</td>
                  <td>{c.bloodRing}</td>
                  <td>{c.unsure}</td>
                  <td className="muted">{c.discrepancyNote ?? "—"}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      {batch.hatch && (
        <>
          <h2>Hatch</h2>
          <div className="card metrics">
            <span className="stat">
              <b>{batch.hatch.hatched}</b>
              <span className="muted">hatched</span>
            </span>
            <span className="stat">
              <b>{batch.hatch.pippedDead}</b>
              <span className="muted">pipped, died</span>
            </span>
            <span className="stat">
              <b>{batch.hatch.deadInShell}</b>
              <span className="muted">dead in shell</span>
            </span>
            <span className="stat">
              <b>{batch.hatch.unhatched}</b>
              <span className="muted">unhatched</span>
            </span>
          </div>
        </>
      )}

      <h2>Egg sources</h2>
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Collected</th>
              <th>Used</th>
              <th>Source</th>
            </tr>
          </thead>
          <tbody>
            {batch.sources.map((s) => (
              <tr key={s.collectionId}>
                <td>{fmtDate(s.collection.collectedOn)}</td>
                <td>{s.count}</td>
                <td className="muted">{s.collection.sourceNote ?? "—"}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </>
  );
}
