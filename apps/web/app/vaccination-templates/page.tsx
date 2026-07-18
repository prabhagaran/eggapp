"use client";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../lib/api";
import type { Species, VaccinationTemplateItem } from "../../lib/types";
import { useAuthedFarm } from "../../lib/useAuthedFarm";

// US-VAC-001: editable starting points seeded from domain-knowledge §6 —
// a regional template, not hardcoded truth. This page is where a farm
// diverges from the seed.
export default function VaccinationTemplatesPage() {
  const farmId = useAuthedFarm();
  const [rows, setRows] = useState<VaccinationTemplateItem[] | null>(null);
  const [species, setSpecies] = useState<Species[]>([]);
  const [error, setError] = useState<string | null>(null);

  const reload = useCallback(() => {
    if (farmId) api<VaccinationTemplateItem[]>(`/v1/farms/${farmId}/vaccination-templates`).then(setRows);
  }, [farmId]);
  useEffect(() => {
    reload();
    api<Species[]>("/v1/species").then(setSpecies);
  }, [reload]);

  async function onSeed() {
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/vaccination-templates/seed-defaults`, { method: "POST" });
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onCreate(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const form = e.currentTarget;
    const f = new FormData(form);
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/vaccination-templates`, {
        method: "POST",
        body: {
          speciesId: f.get("speciesId"),
          purpose: f.get("purpose"),
          ageDaysFrom: Number(f.get("ageDaysFrom")),
          ageDaysTo: Number(f.get("ageDaysTo")),
          vaccine: f.get("vaccine"),
          disease: f.get("disease"),
          route: f.get("route"),
        },
      });
      form.reset();
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onDelete(id: string) {
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/vaccination-templates/${id}`, { method: "DELETE" });
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  if (!farmId) return null;
  return (
    <>
      <h1>Vaccination templates</h1>
      {error && <p className="alert-error">{error}</p>}

      {rows && rows.length === 0 && (
        <div className="card">
          <p className="muted">No templates yet.</p>
          <button className="primary" onClick={onSeed}>
            Seed chicken defaults (domain-knowledge §6)
          </button>
        </div>
      )}

      <div className="card">
        <form className="row" onSubmit={onCreate}>
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
            Age from (days)
            <input name="ageDaysFrom" type="number" min={0} required />
          </label>
          <label>
            Age to (days)
            <input name="ageDaysTo" type="number" min={0} required />
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
          <button className="primary">Add item</button>
        </form>
      </div>

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Species</th>
              <th>Purpose</th>
              <th>Age (days)</th>
              <th>Vaccine</th>
              <th>Disease</th>
              <th>Route</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {(rows ?? []).map((t) => (
              <tr key={t.id}>
                <td>{t.speciesId}</td>
                <td>{t.purpose}</td>
                <td>
                  {t.ageDaysFrom}–{t.ageDaysTo}
                </td>
                <td>{t.vaccine}</td>
                <td>{t.disease}</td>
                <td>{t.route}</td>
                <td>
                  <button onClick={() => onDelete(t.id)}>Delete</button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </>
  );
}
