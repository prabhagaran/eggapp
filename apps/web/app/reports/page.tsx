"use client";
import { useEffect, useState } from "react";
import { api, downloadCsv } from "../../lib/api";
import type {
  Batch,
  BatchEnvironmentReport,
  HatchPerformanceReport,
  Incubator,
  MortalityTrendRow,
  Species,
  VaccinationComplianceReport,
} from "../../lib/types";
import { fmtDate, useAuthedFarm } from "../../lib/useAuthedFarm";

type Tab = "hatch" | "environmental" | "vaccination" | "mortality";
const TABS: { id: Tab; label: string }[] = [
  { id: "hatch", label: "Hatch performance" },
  { id: "environmental", label: "Environmental" },
  { id: "vaccination", label: "Vaccination compliance" },
  { id: "mortality", label: "Mortality trends" },
];

export default function ReportsPage() {
  const farmId = useAuthedFarm();
  const [tab, setTab] = useState<Tab>("hatch");

  if (!farmId) return null;
  return (
    <>
      <h1>Reports</h1>
      <div className="row" style={{ marginBottom: "1rem" }}>
        {TABS.map((t) => (
          <button key={t.id} className={tab === t.id ? "primary" : "secondary"} onClick={() => setTab(t.id)}>
            {t.label}
          </button>
        ))}
      </div>
      {tab === "hatch" && <HatchPerformance farmId={farmId} />}
      {tab === "environmental" && <Environmental farmId={farmId} />}
      {tab === "vaccination" && <VaccinationCompliance farmId={farmId} />}
      {tab === "mortality" && <MortalityTrends farmId={farmId} />}
    </>
  );
}

function HatchPerformance({ farmId }: { farmId: string }) {
  const [species, setSpecies] = useState<Species[]>([]);
  const [incubators, setIncubators] = useState<Incubator[]>([]);
  const [speciesId, setSpeciesId] = useState("");
  const [incubatorId, setIncubatorId] = useState("");
  const [report, setReport] = useState<HatchPerformanceReport | null>(null);

  useEffect(() => {
    api<Species[]>("/v1/species").then(setSpecies);
    api<Incubator[]>(`/v1/farms/${farmId}/incubators`).then(setIncubators);
  }, [farmId]);

  useEffect(() => {
    const params = new URLSearchParams();
    if (speciesId) params.set("speciesId", speciesId);
    if (incubatorId) params.set("incubatorId", incubatorId);
    api<HatchPerformanceReport>(`/v1/farms/${farmId}/reports/hatch-performance?${params}`).then(setReport);
  }, [farmId, speciesId, incubatorId]);

  return (
    <>
      <div className="card">
        <div className="row">
          <label>
            Species
            <select value={speciesId} onChange={(e) => setSpeciesId(e.target.value)}>
              <option value="">All</option>
              {species.map((s) => (
                <option key={s.id} value={s.id}>
                  {s.name}
                </option>
              ))}
            </select>
          </label>
          <label>
            Incubator
            <select value={incubatorId} onChange={(e) => setIncubatorId(e.target.value)}>
              <option value="">All</option>
              {incubators.map((i) => (
                <option key={i.id} value={i.id}>
                  {i.name}
                </option>
              ))}
            </select>
          </label>
          <button
            className="secondary"
            onClick={() => {
              const params = new URLSearchParams({ format: "csv" });
              if (speciesId) params.set("speciesId", speciesId);
              if (incubatorId) params.set("incubatorId", incubatorId);
              downloadCsv(`/v1/farms/${farmId}/reports/hatch-performance?${params}`, "hatch-performance.csv");
            }}
          >
            Export CSV
          </button>
        </div>
        {report && (
          <div className="metrics" style={{ marginTop: "0.6rem" }}>
            <span className="stat">
              <b>{report.trend.avgFertilityPct?.toFixed(1) ?? "—"}%</b>
              <span className="muted">avg fertility</span>
            </span>
            <span className="stat">
              <b>{report.trend.avgHatchOfSetPct?.toFixed(1) ?? "—"}%</b>
              <span className="muted">avg hatch-of-set</span>
            </span>
            <span className="stat">
              <b>{report.trend.avgHatchOfFertilePct?.toFixed(1) ?? "—"}%</b>
              <span className="muted">avg hatch-of-fertile</span>
            </span>
          </div>
        )}
      </div>

      {!report && <p className="muted">Loading…</p>}
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Species</th>
              <th>Incubator</th>
              <th>Set</th>
              <th>Hatched</th>
              <th>Viable</th>
              <th>Fertility%</th>
              <th>Hatch/set%</th>
              <th>Hatch/fertile%</th>
            </tr>
          </thead>
          <tbody>
            {(report?.batches ?? []).map((b) => (
              <tr key={b.batchId}>
                <td>{b.speciesName}</td>
                <td>{b.incubatorName}</td>
                <td>{fmtDate(b.setAt)}</td>
                <td>{fmtDate(b.hatchedAt)}</td>
                <td>{b.viableCount}</td>
                <td>{b.fertilityPct ?? "—"}</td>
                <td>{b.hatchOfSetPct ?? "—"}</td>
                <td>{b.hatchOfFertilePct ?? "—"}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      {report && report.batches.length === 0 && <p className="muted">No completed batches match these filters.</p>}
    </>
  );
}

function Environmental({ farmId }: { farmId: string }) {
  const [batches, setBatches] = useState<Batch[]>([]);
  const [batchId, setBatchId] = useState("");
  const [report, setReport] = useState<BatchEnvironmentReport | null>(null);

  useEffect(() => {
    api<Batch[]>(`/v1/farms/${farmId}/batches`).then(setBatches);
  }, [farmId]);

  useEffect(() => {
    if (!batchId) {
      setReport(null);
      return;
    }
    api<BatchEnvironmentReport>(`/v1/farms/${farmId}/reports/environmental/${batchId}`).then(setReport);
  }, [farmId, batchId]);

  return (
    <>
      <div className="card">
        <div className="row">
          <label>
            Batch
            <select value={batchId} onChange={(e) => setBatchId(e.target.value)}>
              <option value="">Select a batch</option>
              {batches.map((b) => (
                <option key={b.id} value={b.id}>
                  {b.species?.name ?? b.speciesId} — {b.incubator?.name} ({b.status})
                </option>
              ))}
            </select>
          </label>
          {batchId && (
            <button
              className="secondary"
              onClick={() => downloadCsv(`/v1/farms/${farmId}/reports/environmental/${batchId}?format=csv`, `environmental-${batchId}.csv`)}
            >
              Export excursions CSV
            </button>
          )}
        </div>
        {report && (
          <div className="metrics" style={{ marginTop: "0.6rem" }}>
            <span className="stat">
              <b>
                {report.summary.temp.min?.toFixed(1) ?? "—"}–{report.summary.temp.max?.toFixed(1) ?? "—"}°C
              </b>
              <span className="muted">temp range (avg {report.summary.temp.avg?.toFixed(1) ?? "—"}°C)</span>
            </span>
            <span className="stat">
              <b>
                {report.summary.humidity.min?.toFixed(0) ?? "—"}–{report.summary.humidity.max?.toFixed(0) ?? "—"}%
              </b>
              <span className="muted">humidity range (avg {report.summary.humidity.avg?.toFixed(0) ?? "—"}%)</span>
            </span>
          </div>
        )}
      </div>

      {batchId && !report && <p className="muted">Loading…</p>}
      {report && (
        <div className="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Severity</th>
                <th>Message</th>
                <th>Triggered</th>
                <th>Duration</th>
              </tr>
            </thead>
            <tbody>
              {report.excursions.map((ex, i) => (
                <tr key={i}>
                  <td>
                    <span className={`badge ${ex.severity === "critical" ? "danger" : "warn"}`}>{ex.severity}</span>
                  </td>
                  <td>{ex.message}</td>
                  <td>{fmtDate(ex.triggeredAt)}</td>
                  <td>{ex.durationMinutes != null ? `${ex.durationMinutes}m` : "ongoing"}</td>
                </tr>
              ))}
            </tbody>
          </table>
          {report.excursions.length === 0 && <p className="muted" style={{ padding: "0.7rem" }}>No excursions during this batch.</p>}
        </div>
      )}
    </>
  );
}

function VaccinationCompliance({ farmId }: { farmId: string }) {
  const [report, setReport] = useState<VaccinationComplianceReport | null>(null);
  useEffect(() => {
    api<VaccinationComplianceReport>(`/v1/farms/${farmId}/reports/vaccination-compliance`).then(setReport);
  }, [farmId]);

  return (
    <>
      <div className="card">
        <div className="row" style={{ justifyContent: "space-between" }}>
          {report && (
            <div className="metrics">
              <span className="stat">
                <b className="badge ok">{report.totals.administered}</b>
                <span className="muted">administered</span>
              </span>
              <span className="stat">
                <b className="badge danger">{report.totals.overdue}</b>
                <span className="muted">overdue</span>
              </span>
              <span className="stat">
                <b className="badge warn">{report.totals.due}</b>
                <span className="muted">due</span>
              </span>
              <span className="stat">
                <b className="badge">{report.totals.upcoming}</b>
                <span className="muted">upcoming</span>
              </span>
            </div>
          )}
          <button
            className="secondary"
            onClick={() => downloadCsv(`/v1/farms/${farmId}/reports/vaccination-compliance?format=csv`, "vaccination-compliance.csv")}
          >
            Export CSV
          </button>
        </div>
      </div>

      {!report && <p className="muted">Loading…</p>}
      {(report?.flocks ?? []).map((flock) => (
        <div key={flock.flockId} className="card">
          <b>{flock.flockName}</b>
          <div className="table-wrap" style={{ marginTop: "0.5rem" }}>
            <table>
              <thead>
                <tr>
                  <th>Vaccine</th>
                  <th>Disease</th>
                  <th>Due</th>
                  <th>Status</th>
                </tr>
              </thead>
              <tbody>
                {flock.items.map((item) => (
                  <tr key={item.id}>
                    <td>{item.vaccine}</td>
                    <td>{item.disease}</td>
                    <td>{fmtDate(item.dueDate)}</td>
                    <td>
                      <span
                        className={`badge ${
                          item.status === "administered" ? "ok" : item.status === "overdue" ? "danger" : item.status === "due" ? "warn" : ""
                        }`}
                      >
                        {item.status}
                      </span>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
            {flock.items.length === 0 && <p className="muted" style={{ padding: "0.7rem" }}>No vaccination schedule for this flock's species/purpose.</p>}
          </div>
        </div>
      ))}
    </>
  );
}

const STAGE_LABEL: Record<string, string> = {
  brooding: "Brooding",
  grower: "Grower",
  pre_lay: "Pre-lay",
  layer: "Layer",
  broiler_starter: "Broiler starter",
  broiler_grower: "Broiler grower",
  broiler_finisher: "Broiler finisher",
};

function MortalityTrends({ farmId }: { farmId: string }) {
  const [rows, setRows] = useState<MortalityTrendRow[] | null>(null);
  useEffect(() => {
    api<MortalityTrendRow[]>(`/v1/farms/${farmId}/reports/mortality-trends`).then(setRows);
  }, [farmId]);

  return (
    <>
      <div className="row" style={{ marginBottom: "0.6rem" }}>
        <button className="secondary" onClick={() => downloadCsv(`/v1/farms/${farmId}/reports/mortality-trends?format=csv`, "mortality-trends.csv")}>
          Export CSV
        </button>
      </div>
      {!rows && <p className="muted">Loading…</p>}
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Flock</th>
              <th>Stage</th>
              <th>Placed</th>
              <th>Current</th>
              <th>Deaths</th>
              <th>Culls</th>
              <th>Sales</th>
              <th>Early mortality (≤14d)</th>
              <th>Recent monthly</th>
            </tr>
          </thead>
          <tbody>
            {(rows ?? []).map((r) => (
              <tr key={r.flockId}>
                <td>{r.flockName}</td>
                <td>{r.stage ? STAGE_LABEL[r.stage] ?? r.stage : "—"}</td>
                <td>{r.placedCount}</td>
                <td>{r.currentCount}</td>
                <td>{r.byCause.death}</td>
                <td>{r.byCause.cull}</td>
                <td>{r.byCause.sale}</td>
                <td>
                  <span className={`badge ${r.earlyMortality.status === "elevated" ? "danger" : r.earlyMortality.status === "within_norm" ? "ok" : ""}`}>
                    {r.earlyMortality.deaths} · {r.earlyMortality.status.replace("_", " ")}
                  </span>
                </td>
                <td>
                  <span
                    className={`badge ${
                      r.recentMonthlyMortality.status === "elevated" ? "danger" : r.recentMonthlyMortality.status === "within_norm" ? "ok" : ""
                    }`}
                  >
                    {r.recentMonthlyMortality.deaths} · {r.recentMonthlyMortality.status.replace("_", " ")}
                  </span>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
        {rows && rows.length === 0 && <p className="muted" style={{ padding: "0.7rem" }}>No flocks yet.</p>}
      </div>
    </>
  );
}
