"use client";
import { useCallback, useEffect, useState } from "react";
import { Thermometer, Droplets, Wind, Gauge, Sun, Wheat, Droplet } from "lucide-react";
import { api } from "../../lib/api";
import { SensorTile } from "../../components/SensorTile";
import type { Coop } from "../../lib/types";
import { fmtAge, isFresh, useAuthedFarm } from "../../lib/useAuthedFarm";

// House-level welfare bands — see app/page.tsx for why these are fixed
// here but derived from setpoints for an incubator.
const TEMP_RANGE = { min: 18, max: 30, warnBand: 4 };
const HUM_RANGE = { min: 50, max: 70, warnBand: 10 };
const CO2_RANGE = { max: 450, warnBand: 550 };
const NH3_RANGE = { max: 10, warnBand: 10 };
const FEED_RANGE = { min: 20, warnBand: 10 };
const WATER_RANGE = { min: 25, warnBand: 15 };

export default function CoopsPage() {
  const farmId = useAuthedFarm();
  const [rows, setRows] = useState<Coop[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [editingId, setEditingId] = useState<string | null>(null);

  const reload = useCallback(() => {
    if (farmId) api<Coop[]>(`/v1/farms/${farmId}/coops`).then(setRows);
  }, [farmId]);
  useEffect(() => {
    reload();
    // Telemetry lands every ~60s; poll faster so a fresh reading appears
    // without a manual refresh (same cadence as the incubators page).
    const t = setInterval(reload, 15_000);
    return () => clearInterval(t);
  }, [reload]);

  async function onCreate(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const form = e.currentTarget;
    const f = new FormData(form);
    setError(null);
    try {
      const capacity = f.get("capacity");
      await api(`/v1/farms/${farmId}/coops`, {
        method: "POST",
        body: { name: f.get("name"), capacity: capacity ? Number(capacity) : undefined },
      });
      form.reset();
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onSaveEdit(e: React.FormEvent<HTMLFormElement>, coopId: string) {
    e.preventDefault();
    const f = new FormData(e.currentTarget);
    setError(null);
    try {
      const capacity = f.get("capacity");
      await api(`/v1/farms/${farmId}/coops/${coopId}`, {
        method: "PATCH",
        body: { name: f.get("name"), capacity: capacity ? Number(capacity) : null },
      });
      setEditingId(null);
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onDelete(coop: Coop) {
    // Deleting the building never touches the birds — Flock.coopId is
    // ON DELETE SET NULL — but say so plainly, since "delete the coop my
    // flock lives in" reasonably sounds destructive.
    const housed = coop.flocks.length;
    const warning = housed
      ? `\n\n${housed} flock${housed > 1 ? "s" : ""} will become unhoused. No bird records are deleted.`
      : "";
    if (!window.confirm(`Delete coop "${coop.name}"?${warning}`)) return;
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/coops/${coop.id}`, { method: "DELETE" });
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  if (!farmId) return null;
  return (
    <>
      <h1>Coops</h1>
      <p className="muted">Bird houses and their environmental monitoring.</p>
      {error && <p className="alert-error">{error}</p>}

      <div className="card">
        <form className="row" onSubmit={onCreate}>
          <label>
            Name
            <input name="name" required placeholder="Coop 1" />
          </label>
          <label>
            Capacity (birds, optional)
            <input name="capacity" type="number" min={1} />
          </label>
          <button className="primary">Add coop</button>
        </form>
      </div>

      {(rows ?? []).map((coop) => {
        const t = coop.latestTelemetry;
        return editingId === coop.id ? (
          <div key={coop.id} className="card" style={{ marginTop: "1rem" }}>
            <form className="stack" onSubmit={(e) => onSaveEdit(e, coop.id)}>
              <label>
                Name
                <input name="name" required defaultValue={coop.name} />
              </label>
              <label>
                Capacity (birds)
                <input name="capacity" type="number" min={1} defaultValue={coop.capacity ?? ""} />
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
          <section key={coop.id} style={{ marginTop: "1.5rem" }}>
            <div className="card">
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", gap: "1rem" }}>
                <div>
                  <b>{coop.name}</b>
                  <div className="muted">
                    {coop.capacity != null ? `capacity ${coop.capacity} birds` : "no capacity set"}
                    {coop.flocks.length > 0 && ` · ${coop.flocks.map((f) => f.name).join(", ")}`}
                  </div>
                  <div style={{ marginTop: "0.4rem" }}>
                    {coop.device ? (
                      <>
                        <span className={`badge ${coop.device.status === "active" ? "ok" : ""}`}>
                          {coop.device.status}
                        </span>{" "}
                        <span className="muted">{coop.device.name ?? coop.device.hardwareId}</span>
                        {t && (
                          <span className={isFresh(t.ts) ? "muted" : "badge warn"} style={{ marginLeft: "0.5rem" }}>
                            {fmtAge(t.ts)}
                          </span>
                        )}
                      </>
                    ) : (
                      <>
                        <span className="badge warn">no device bound</span>{" "}
                        <span className="muted">bind a coop monitor on the Devices page</span>
                      </>
                    )}
                  </div>
                </div>
                <div className="row" style={{ flexShrink: 0 }}>
                  <button className="secondary" onClick={() => setEditingId(coop.id)}>
                    Edit
                  </button>
                  <button className="secondary" onClick={() => onDelete(coop)}>
                    Delete
                  </button>
                </div>
              </div>
            </div>

            {t && (
              <div className="sensor-grid" style={{ marginTop: "1rem" }}>
                <SensorTile icon={<Thermometer />} label="Temperature" value={t.tempC} unit="°C" range={TEMP_RANGE} />
                <SensorTile icon={<Droplets />} label="Humidity" value={t.humidityPct} unit="%" range={HUM_RANGE}
                  decimals={0} />
                <SensorTile icon={<Wind />} label="CO₂ Level" value={t.co2Ppm} unit="ppm" range={CO2_RANGE}
                  decimals={0} />
                <SensorTile icon={<Gauge />} label="Ammonia" value={t.ammoniaPpm} unit="ppm" range={NH3_RANGE} />
                <SensorTile icon={<Sun />} label="Light Intensity" value={t.lightLux} unit="lux" decimals={0} />
                <SensorTile icon={<Wheat />} label="Feed Level" value={t.feedLevelPct} unit="%" range={FEED_RANGE} />
                <SensorTile icon={<Droplet />} label="Water Level" value={t.waterLevelPct} unit="%"
                  range={WATER_RANGE} />
              </div>
            )}
          </section>
        );
      })}

      {rows && rows.length === 0 && <p className="muted">No coops yet — add your first above.</p>}
    </>
  );
}
