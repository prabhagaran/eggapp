"use client";
import { useParams, useRouter } from "next/navigation";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../../lib/api";
import type { BatchDetail, EggCollection, Incubator, Species } from "../../../lib/types";
import { batchBadgeClass, dayOf, fmtDate, useAuthedFarm } from "../../../lib/useAuthedFarm";

// datetime-local inputs need "YYYY-MM-DDTHH:mm" in local time, no timezone suffix.
function toLocalInputValue(d: Date) {
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

function ageDays(collectedOn: string) {
  return Math.floor((Date.now() - Date.parse(collectedOn)) / 86_400_000);
}

export default function BatchDetailPage() {
  const farmId = useAuthedFarm();
  const router = useRouter();
  const { id } = useParams<{ id: string }>();
  const [batch, setBatch] = useState<BatchDetail | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [setAtInput, setSetAtInput] = useState(() => toLocalInputValue(new Date()));

  const [editing, setEditing] = useState(false);
  const [species, setSpecies] = useState<Species[]>([]);
  const [incubators, setIncubators] = useState<Incubator[]>([]);
  const [collections, setCollections] = useState<EggCollection[]>([]);
  const [editIncubatorId, setEditIncubatorId] = useState("");
  const [editSpeciesId, setEditSpeciesId] = useState("");
  const [editPicked, setEditPicked] = useState<Record<string, number>>({});
  const [editOverrideNote, setEditOverrideNote] = useState("");
  const [deleting, setDeleting] = useState(false);

  const reload = useCallback(() => {
    if (farmId && id) api<BatchDetail>(`/v1/farms/${farmId}/batches/${id}`).then(setBatch);
  }, [farmId, id]);
  useEffect(reload, [reload]);

  useEffect(() => {
    if (!farmId) return;
    api<Species[]>("/v1/species").then(setSpecies);
    api<Incubator[]>(`/v1/farms/${farmId}/incubators`).then(setIncubators);
    api<EggCollection[]>(`/v1/farms/${farmId}/collections`).then(setCollections);
  }, [farmId]);

  function startEdit() {
    if (!batch) return;
    setEditIncubatorId(batch.incubatorId);
    setEditSpeciesId(batch.speciesId);
    setEditPicked(Object.fromEntries(batch.sources.map((s) => [s.collectionId, s.count])));
    setEditOverrideNote("");
    setEditing(true);
  }

  async function post(path: string, body?: unknown) {
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/batches/${id}${path}`, { method: "POST", body });
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onSaveEdit(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const sources = Object.entries(editPicked)
      .filter(([, n]) => n > 0)
      .map(([collectionId, count]) => ({ collectionId, count }));
    if (sources.length === 0) {
      setError("Pick at least one collection (set a count > 0)");
      return;
    }
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/batches/${id}`, {
        method: "PATCH",
        body: {
          incubatorId: editIncubatorId,
          speciesId: editSpeciesId,
          sources,
          overrideNote: editOverrideNote || undefined,
        },
      });
      setEditing(false);
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to save changes");
    }
  }

  async function onDelete() {
    if (!window.confirm("Delete this batch? Its egg sources are released back to their collections.")) return;
    setError(null);
    setDeleting(true);
    try {
      await api(`/v1/farms/${farmId}/batches/${id}`, { method: "DELETE" });
      router.push("/batches");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to delete batch");
      setDeleting(false);
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
  const dayMismatch = day != null && batch.deviceDay != null && Math.abs(day - batch.deviceDay) > 1;
  const nextCandle = batch.candlingDays.find((d) => !batch.candlings.some((c) => c.dayNo === d));
  const active = !["completed", "closed", "aborted"].includes(batch.status);
  const canEdit = batch.status === "planned" || batch.status === "incubating";
  const canDelete = batch.status === "planned" || batch.status === "aborted";

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
        {batch.deviceDay != null && (
          <span className="stat">
            <b>{batch.deviceDay}</b>
            <span className={dayMismatch ? "badge warn" : "muted"}>device day{dayMismatch ? " (mismatch)" : ""}</span>
          </span>
        )}
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
        {batch.deviceExpectedHatchAt && (
          <div className={dayMismatch ? "alert-warn" : "muted"}>
            device reports day {batch.deviceDay}, expects hatch {fmtDate(batch.deviceExpectedHatchAt)}
            {dayMismatch ? " — doesn't match the manual schedule above" : ""}
          </div>
        )}
        {batch.abortReason && <p className="alert-error">Aborted: {batch.abortReason}</p>}
      </div>

      {(canEdit || canDelete) && !editing && (
        <div className="card">
          <div className="row">
            {canEdit && (
              <button className="secondary" onClick={startEdit}>
                Edit batch
              </button>
            )}
            {canDelete && (
              <button className="danger" onClick={onDelete} disabled={deleting}>
                {deleting ? "Deleting…" : "Delete batch"}
              </button>
            )}
          </div>
          <p className="muted">
            {batch.status === "incubating"
              ? "Editing an incubating batch changes its live incubator/species/source assignment — the schedule is recomputed if species changes."
              : batch.status === "aborted"
                ? "Deleting an aborted batch permanently removes its record."
                : "Only possible before eggs are set — editing after that would invalidate the computed schedule."}
          </p>
        </div>
      )}

      {canEdit && editing && (
        <div className="card">
          <h2 style={{ marginTop: 0 }}>Edit batch</h2>
          <form className="stack" style={{ maxWidth: 640 }} onSubmit={onSaveEdit}>
            <div className="row">
              <label>
                Incubator
                <select value={editIncubatorId} onChange={(e) => setEditIncubatorId(e.target.value)} required>
                  {incubators.map((i) => (
                    <option key={i.id} value={i.id}>
                      {i.name}
                    </option>
                  ))}
                </select>
              </label>
              <label>
                Species
                <select value={editSpeciesId} onChange={(e) => setEditSpeciesId(e.target.value)} required>
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
                  {collections
                    // This batch's own current usage of a collection is
                    // already subtracted out of availableCount server-side
                    // — add it back below so editing (including just
                    // re-saving the same count) isn't blocked by the
                    // batch's own prior assignment.
                    .map((c) => {
                      const own = batch.sources.find((s) => s.collectionId === c.id)?.count ?? 0;
                      const effectiveAvailable = c.availableCount + own;
                      if (effectiveAvailable <= 0) return null;
                      const age = ageDays(c.collectedOn);
                      return (
                        <tr key={c.id}>
                          <td>
                            {fmtDate(c.collectedOn)} <span className="muted">{c.sourceNote ?? ""}</span>
                          </td>
                          <td>
                            <span className={`badge ${age > 14 ? "danger" : age > 7 ? "warn" : "ok"}`}>{age} d</span>
                          </td>
                          <td>{effectiveAvailable}</td>
                          <td>
                            <input
                              type="number"
                              min={0}
                              max={effectiveAvailable}
                              value={editPicked[c.id] ?? 0}
                              onChange={(e) =>
                                setEditPicked((p) => ({ ...p, [c.id]: Number(e.target.value) }))
                              }
                            />
                          </td>
                        </tr>
                      );
                    })}
                </tbody>
              </table>
            </div>
            <label>
              Override note (only needed if any picked collection's eggs are older than 14 days)
              <input value={editOverrideNote} onChange={(e) => setEditOverrideNote(e.target.value)} />
            </label>
            <div className="row">
              <button className="primary">Save changes</button>
              <button type="button" className="secondary" onClick={() => setEditing(false)}>
                Cancel
              </button>
            </div>
          </form>
        </div>
      )}

      {(batch.status === "planned" || batch.status === "setting") && (
        <div className="card">
          <label>
            Set at
            <input
              type="datetime-local"
              value={setAtInput}
              onChange={(e) => setSetAtInput(e.target.value)}
            />
          </label>
          <button
            className="primary"
            onClick={() => post("/set", { setAt: new Date(setAtInput).toISOString() })}
          >
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
