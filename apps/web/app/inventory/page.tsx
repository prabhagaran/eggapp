"use client";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../lib/api";
import type { InventoryItem } from "../../lib/types";
import { fmtDate, useAuthedFarm } from "../../lib/useAuthedFarm";

const KIND_LABEL: Record<string, string> = { feed: "Feed", vaccine: "Vaccine", consumable: "Consumable" };

export default function InventoryPage() {
  const farmId = useAuthedFarm();
  const [rows, setRows] = useState<InventoryItem[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [kind, setKind] = useState<"feed" | "vaccine" | "consumable">("feed");
  const [editingId, setEditingId] = useState<string | null>(null);

  const reload = useCallback(() => {
    if (farmId) api<InventoryItem[]>(`/v1/farms/${farmId}/inventory`).then(setRows);
  }, [farmId]);
  useEffect(reload, [reload]);

  async function onCreate(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const form = e.currentTarget;
    const f = new FormData(form);
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/inventory`, {
        method: "POST",
        body: {
          kind,
          name: f.get("name"),
          unit: f.get("unit"),
          quantity: Number(f.get("quantity")),
          lotNumber: f.get("lotNumber") || undefined,
          expiry: f.get("expiry") ? new Date(String(f.get("expiry"))).toISOString() : undefined,
          lowStockThreshold: f.get("lowStockThreshold") ? Number(f.get("lowStockThreshold")) : undefined,
        },
      });
      form.reset();
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onAdjust(id: string, sign: 1 | -1) {
    const amountStr = window.prompt(sign === 1 ? "Restock amount:" : "Deduct amount:");
    if (!amountStr) return;
    const amount = Number(amountStr);
    if (!amount || amount <= 0) return;
    try {
      await api(`/v1/farms/${farmId}/inventory/${id}/adjust`, {
        method: "POST",
        body: { delta: sign * amount, note: sign === 1 ? "manual restock" : "manual deduction" },
      });
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onDelete(id: string) {
    if (!window.confirm("Delete this inventory item?")) return;
    try {
      await api(`/v1/farms/${farmId}/inventory/${id}`, { method: "DELETE" });
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onSaveEdit(e: React.FormEvent<HTMLFormElement>, itemId: string) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/inventory/${itemId}`, {
        method: "PATCH",
        body: {
          name: f.get("name"),
          unit: f.get("unit"),
          lotNumber: f.get("lotNumber") || undefined,
          expiry: f.get("expiry") ? new Date(String(f.get("expiry"))).toISOString() : undefined,
          lowStockThreshold: f.get("lowStockThreshold") ? Number(f.get("lowStockThreshold")) : undefined,
        },
      });
      setEditingId(null);
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  if (!farmId) return null;
  const expiringSoon = (item: InventoryItem) => {
    if (!item.expiry) return false;
    return new Date(item.expiry).getTime() - Date.now() < 30 * 86_400_000;
  };

  return (
    <>
      <h1>Inventory</h1>
      {error && <p className="alert-error">{error}</p>}

      <div className="card">
        <form className="stack" onSubmit={onCreate}>
          <label>
            Kind
            <select value={kind} onChange={(e) => setKind(e.target.value as typeof kind)}>
              <option value="feed">Feed</option>
              <option value="vaccine">Vaccine</option>
              <option value="consumable">Consumable</option>
            </select>
          </label>
          <label>
            Name
            <input name="name" required placeholder={kind === "feed" ? "Layer Pellets 17%" : kind === "vaccine" ? "ND + IB live" : "Bedding"} />
          </label>
          <div className="row">
            <label>
              Unit
              <input name="unit" required placeholder={kind === "feed" ? "kg" : kind === "vaccine" ? "doses" : "units"} />
            </label>
            <label>
              Starting quantity
              <input name="quantity" type="number" step="any" min={0} required defaultValue={0} />
            </label>
            <label>
              Low-stock threshold (optional)
              <input name="lowStockThreshold" type="number" step="any" min={0} />
            </label>
          </div>
          {kind === "vaccine" && (
            <div className="row">
              <label>
                Lot number
                <input name="lotNumber" required />
              </label>
              <label>
                Expiry
                <input name="expiry" type="date" required />
              </label>
            </div>
          )}
          <button className="primary">Add item</button>
        </form>
      </div>

      <div className="grid">
        {(rows ?? []).map((item) =>
          editingId === item.id ? (
            <div key={item.id} className="card">
              <form className="stack" onSubmit={(e) => onSaveEdit(e, item.id)}>
                <label>
                  Name
                  <input name="name" required defaultValue={item.name} />
                </label>
                <label>
                  Unit
                  <input name="unit" required defaultValue={item.unit} />
                </label>
                <label>
                  Low-stock threshold (optional)
                  <input name="lowStockThreshold" type="number" step="any" min={0} defaultValue={item.lowStockThreshold ?? ""} />
                </label>
                {item.kind === "vaccine" && (
                  <>
                    <label>
                      Lot number
                      <input name="lotNumber" required defaultValue={item.lotNumber ?? ""} />
                    </label>
                    <label>
                      Expiry
                      <input name="expiry" type="date" required defaultValue={item.expiry?.slice(0, 10) ?? ""} />
                    </label>
                  </>
                )}
                <div className="row">
                  <button className="primary">Save</button>
                  <button type="button" className="secondary" onClick={() => setEditingId(null)}>
                    Cancel
                  </button>
                </div>
              </form>
            </div>
          ) : (
          <div key={item.id} className="card">
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start" }}>
              <div>
                <b>{item.name}</b>
                <div className="muted">{KIND_LABEL[item.kind]}</div>
              </div>
              <span
                className={`badge ${
                  item.lowStockThreshold != null && item.quantity <= item.lowStockThreshold
                    ? "warn"
                    : expiringSoon(item)
                      ? "warn"
                      : "ok"
                }`}
              >
                {item.quantity} {item.unit}
              </span>
            </div>
            {item.lotNumber && <div className="muted">lot {item.lotNumber}</div>}
            {item.expiry && (
              <div className={expiringSoon(item) ? "alert-warn" : "muted"}>expires {fmtDate(item.expiry)}</div>
            )}
            <div className="row" style={{ marginTop: "0.6rem" }}>
              <button className="secondary" onClick={() => onAdjust(item.id, 1)}>
                + Restock
              </button>
              <button className="secondary" onClick={() => onAdjust(item.id, -1)}>
                − Deduct
              </button>
              <button className="secondary" onClick={() => setEditingId(item.id)}>
                Edit
              </button>
              <button className="danger" onClick={() => onDelete(item.id)}>
                Delete
              </button>
            </div>
          </div>
          ),
        )}
      </div>
      {rows && rows.length === 0 && <p className="muted">No inventory items yet — add one above.</p>}
    </>
  );
}
