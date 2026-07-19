"use client";
import { useParams, useRouter } from "next/navigation";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../../lib/api";
import type { ComplianceItem, FlockDetail, InventoryItem, Species } from "../../../lib/types";
import { fmtDate, useAuthedFarm } from "../../../lib/useAuthedFarm";

const STAGE_LABEL: Record<string, string> = {
  brooding: "Brooding",
  grower: "Grower",
  pre_lay: "Pre-lay",
  layer: "Layer",
  broiler_starter: "Broiler starter",
  broiler_grower: "Broiler grower",
  broiler_finisher: "Broiler finisher",
};
const STAGE_OPTIONS = Object.entries(STAGE_LABEL);

const COMPLIANCE_BADGE: Record<string, string> = {
  administered: "badge ok",
  due: "badge warn",
  overdue: "badge danger",
  upcoming: "muted",
};

export default function FlockDetailPage() {
  const farmId = useAuthedFarm();
  const router = useRouter();
  const { id } = useParams<{ id: string }>();
  const [flock, setFlock] = useState<FlockDetail | null>(null);
  const [compliance, setCompliance] = useState<ComplianceItem[]>([]);
  const [inventory, setInventory] = useState<InventoryItem[]>([]);
  const [species, setSpecies] = useState<Species[]>([]);
  const [editing, setEditing] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const reload = useCallback(() => {
    if (!farmId || !id) return;
    api<FlockDetail>(`/v1/farms/${farmId}/flocks/${id}`).then(setFlock);
    api<ComplianceItem[]>(`/v1/farms/${farmId}/flocks/${id}/vaccination/compliance`).then(setCompliance);
  }, [farmId, id]);
  useEffect(reload, [reload]);
  useEffect(() => {
    if (farmId) api<InventoryItem[]>(`/v1/farms/${farmId}/inventory`).then(setInventory);
    api<Species[]>("/v1/species").then(setSpecies);
  }, [farmId]);
  const vaccineItems = inventory.filter((i) => i.kind === "vaccine");
  const feedItems = inventory.filter((i) => i.kind === "feed");

  async function onSaveEdit(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/flocks/${id}`, {
        method: "PATCH",
        body: {
          name: f.get("name"),
          speciesId: f.get("speciesId"),
          purpose: f.get("purpose"),
          acquisitionNote: f.get("acquisitionNote") || undefined,
          stageOverride: f.get("stageOverride") || null,
        },
      });
      setEditing(false);
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onDelete() {
    if (!window.confirm(`Delete "${flock?.name}"? Its mortality, vaccination, feed, and water records will be permanently deleted too. This can't be undone.`)) return;
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/flocks/${id}`, { method: "DELETE" });
      router.push("/flocks");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function post(path: string, body: unknown) {
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/flocks/${id}${path}`, { method: "POST", body });
      reload();
      return true;
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
      return false;
    }
  }

  async function onMortality(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    const ok = await post("/mortality", {
      date: new Date(String(f.get("date"))).toISOString(),
      count: Number(f.get("count")),
      cause: f.get("cause"),
      notes: f.get("notes") || undefined,
    });
    if (ok) e.currentTarget.reset();
  }

  async function onVaccination(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    const templateItemId = String(f.get("templateItemId") || "");
    const ok = await post("/vaccination", {
      templateItemId: templateItemId || undefined,
      date: new Date(String(f.get("date"))).toISOString(),
      vaccine: f.get("vaccine"),
      disease: f.get("disease"),
      route: f.get("route"),
      count: Number(f.get("count")),
      administeredBy: f.get("administeredBy"),
      manufacturer: f.get("manufacturer") || undefined,
      lotNumber: f.get("lotNumber") || undefined,
      dose: f.get("dose") || undefined,
      reactions: f.get("reactions") || undefined,
      inventoryItemId: f.get("inventoryItemId") || undefined,
    });
    if (ok) e.currentTarget.reset();
  }

  async function onFeed(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    const ok = await post("/feed-logs", {
      loggedAt: new Date().toISOString(),
      feedType: f.get("feedType"),
      quantityKg: Number(f.get("quantityKg")),
      inventoryItemId: f.get("inventoryItemId") || undefined,
    });
    if (ok) e.currentTarget.reset();
  }

  async function onWater(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    const ok = await post("/water-logs", {
      loggedAt: new Date().toISOString(),
      quantityLiters: Number(f.get("quantityLiters")),
    });
    if (ok) e.currentTarget.reset();
  }

  if (!farmId || !flock) return null;
  return (
    <>
      <div className="row" style={{ justifyContent: "space-between", alignItems: "center" }}>
        <h1>{flock.name}</h1>
        <div className="row">
          <button className="secondary" onClick={() => setEditing((v) => !v)}>
            {editing ? "Cancel" : "Edit"}
          </button>
          <button className="danger" onClick={onDelete}>
            Delete
          </button>
        </div>
      </div>
      {error && <p className="alert-error">{error}</p>}

      {editing && (
        <div className="card">
          <form className="row" onSubmit={onSaveEdit}>
            <label>
              Name
              <input name="name" required defaultValue={flock.name} />
            </label>
            <label>
              Species
              <select name="speciesId" required defaultValue={flock.speciesId}>
                {species.map((s) => (
                  <option key={s.id} value={s.id}>
                    {s.name}
                  </option>
                ))}
              </select>
            </label>
            <label>
              Purpose
              <select name="purpose" required defaultValue={flock.purpose}>
                <option value="layer">Layer</option>
                <option value="broiler">Broiler</option>
                <option value="breeder">Breeder</option>
              </select>
            </label>
            <label>
              Provenance note
              <input name="acquisitionNote" defaultValue={flock.acquisitionNote ?? ""} />
            </label>
            <label>
              Stage override
              <select name="stageOverride" defaultValue={flock.stageOverride ?? ""}>
                <option value="">— auto (derive from age) —</option>
                {STAGE_OPTIONS.map(([value, label]) => (
                  <option key={value} value={value}>
                    {label}
                  </option>
                ))}
              </select>
            </label>
            <button className="primary">Save changes</button>
          </form>
        </div>
      )}

      <div className="metrics card">
        <span className="stat">
          <b>{flock.currentCount}</b>
          <span className="muted">current count</span>
        </span>
        <span className="stat">
          <b>{flock.ageDays ?? "—"}</b>
          <span className="muted">days old</span>
        </span>
        <span className="stat">
          <b>{flock.stage ? STAGE_LABEL[flock.stage] ?? flock.stage : "—"}</b>
          <span className="muted">stage{flock.stageOverride ? " (manual)" : ""}</span>
        </span>
        <span className="stat">
          <b>{flock.placedCount}</b>
          <span className="muted">placed</span>
        </span>
      </div>

      <h2>Mortality / cull / sale</h2>
      <div className="card">
        <form className="row" onSubmit={onMortality}>
          <label>
            Date
            <input name="date" type="date" required defaultValue={new Date().toISOString().slice(0, 10)} />
          </label>
          <label>
            Count
            <input name="count" type="number" min={1} required />
          </label>
          <label>
            Cause
            <select name="cause" required defaultValue="death">
              <option value="death">Death</option>
              <option value="cull">Cull</option>
              <option value="sale">Sale</option>
            </select>
          </label>
          <label>
            Notes
            <input name="notes" placeholder="optional" />
          </label>
          <button className="primary">Record</button>
        </form>
      </div>
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Date</th>
              <th>Count</th>
              <th>Cause</th>
              <th>Notes</th>
            </tr>
          </thead>
          <tbody>
            {flock.mortalityRecords.map((r) => (
              <tr key={r.id}>
                <td>{fmtDate(r.date)}</td>
                <td>{r.count}</td>
                <td>{r.cause}</td>
                <td>{r.notes ?? "—"}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <h2>Vaccination</h2>
      <div className="card">
        <form className="row" onSubmit={onVaccination}>
          <label>
            Template item (optional)
            <select name="templateItemId" defaultValue="">
              <option value="">— manual entry —</option>
              {compliance.map((c) => (
                <option key={c.id} value={c.id}>
                  {c.vaccine} ({c.status})
                </option>
              ))}
            </select>
          </label>
          <label>
            Date
            <input name="date" type="date" required defaultValue={new Date().toISOString().slice(0, 10)} />
          </label>
          <label>
            Vaccine
            <input name="vaccine" required />
          </label>
          <label>
            Disease
            <input name="disease" required />
          </label>
          <label>
            Route
            <input name="route" required />
          </label>
          <label>
            Count
            <input name="count" type="number" min={1} required />
          </label>
          <label>
            Administered by
            <input name="administeredBy" required />
          </label>
          <label>
            Manufacturer
            <input name="manufacturer" placeholder="optional" />
          </label>
          <label>
            Lot number
            <input name="lotNumber" placeholder="optional" />
          </label>
          <label>
            Dose
            <input name="dose" placeholder="optional" />
          </label>
          <label>
            Reactions
            <input name="reactions" placeholder="optional" />
          </label>
          <label>
            Deduct from stock (optional)
            <select name="inventoryItemId" defaultValue="">
              <option value="">— don&apos;t track stock —</option>
              {vaccineItems.map((i) => (
                <option key={i.id} value={i.id}>
                  {i.name} ({i.quantity} {i.unit} left)
                </option>
              ))}
            </select>
          </label>
          <button className="primary">Record vaccination</button>
        </form>
      </div>

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Vaccine</th>
              <th>Disease</th>
              <th>Route</th>
              <th>Due</th>
              <th>Status</th>
            </tr>
          </thead>
          <tbody>
            {compliance.map((c) => (
              <tr key={c.id}>
                <td>{c.vaccine}</td>
                <td>{c.disease}</td>
                <td>{c.route}</td>
                <td>{fmtDate(c.dueDate)}</td>
                <td>
                  <span className={COMPLIANCE_BADGE[c.status] ?? "muted"}>{c.status}</span>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <h2>Feed &amp; water</h2>
      <div className="row">
        <div className="card">
          <form className="stack" onSubmit={onFeed}>
            <b>Log feed</b>
            <label>
              Type
              <input name="feedType" required placeholder="e.g. Layer pellets 17%" />
            </label>
            <label>
              Quantity (kg)
              <input name="quantityKg" type="number" step="0.1" min={0.1} required />
            </label>
            <label>
              Deduct from stock (optional)
              <select name="inventoryItemId" defaultValue="">
                <option value="">— don&apos;t track stock —</option>
                {feedItems.map((i) => (
                  <option key={i.id} value={i.id}>
                    {i.name} ({i.quantity} {i.unit} left)
                  </option>
                ))}
              </select>
            </label>
            <button className="primary">Log feed</button>
          </form>
        </div>
        <div className="card">
          <form className="stack" onSubmit={onWater}>
            <b>Log water</b>
            <label>
              Quantity (L)
              <input name="quantityLiters" type="number" step="0.1" min={0.1} required />
            </label>
            <button className="primary">Log water</button>
          </form>
        </div>
      </div>

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Feed logged</th>
              <th>Type</th>
              <th>kg</th>
              <th>Water logged</th>
              <th>L</th>
            </tr>
          </thead>
          <tbody>
            {Array.from({ length: Math.max(flock.recentFeed.length, flock.recentWater.length) }).map((_, i) => (
              <tr key={i}>
                <td>{flock.recentFeed[i] ? fmtDate(flock.recentFeed[i]!.loggedAt) : ""}</td>
                <td>
                  {flock.recentFeed[i]?.feedType ?? ""}
                  {flock.recentFeed[i]?.stageMismatch && <span className="badge warn" style={{ marginLeft: "0.3rem" }}>stage mismatch</span>}
                </td>
                <td>{flock.recentFeed[i]?.quantityKg ?? ""}</td>
                <td>{flock.recentWater[i] ? fmtDate(flock.recentWater[i]!.loggedAt) : ""}</td>
                <td>{flock.recentWater[i]?.quantityLiters ?? ""}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </>
  );
}
