"use client";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../lib/api";
import type { Alert } from "../../lib/types";
import { useAuthedFarm } from "../../lib/useAuthedFarm";

function fmtDateTime(iso: string) {
  return new Date(iso).toLocaleString();
}

export default function AlertsPage() {
  const farmId = useAuthedFarm();
  const [alerts, setAlerts] = useState<Alert[] | null>(null);
  const [error, setError] = useState<string | null>(null);

  const reload = useCallback(() => {
    if (farmId) api<Alert[]>(`/v1/farms/${farmId}/alerts`).then(setAlerts);
  }, [farmId]);

  useEffect(() => {
    reload();
    const t = setInterval(reload, 15_000); // US-NOT poll cadence — no push channel yet
    return () => clearInterval(t);
  }, [reload]);

  async function onAck(id: string) {
    setError(null);
    try {
      await api(`/v1/farms/${farmId}/alerts/${id}/ack`, { method: "POST" });
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  if (!farmId) return null;
  const open = (alerts ?? []).filter((a) => a.state === "open");
  const rest = (alerts ?? []).filter((a) => a.state !== "open");

  return (
    <>
      <h1>Alerts</h1>
      {error && <p className="alert-error">{error}</p>}

      <h2>Open</h2>
      {open.length === 0 && <p className="muted">No open alerts.</p>}
      {open.map((a) => (
        <div key={a.id} className="card" style={{ borderColor: a.severity === "critical" ? "var(--danger)" : undefined }}>
          <span className={`badge ${a.severity === "critical" ? "danger" : "warn"}`}>{a.severity}</span>{" "}
          <span className="muted">{fmtDateTime(a.triggeredAt)}</span>
          <div>{a.message}</div>
          <button className="secondary" onClick={() => onAck(a.id)}>
            Acknowledge
          </button>
        </div>
      ))}

      <h2>History</h2>
      {rest.length === 0 ? (
        <p className="muted">No prior alerts.</p>
      ) : (
        <div className="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Severity</th>
                <th>State</th>
                <th>Triggered</th>
                <th>Message</th>
              </tr>
            </thead>
            <tbody>
              {rest.map((a) => (
                <tr key={a.id}>
                  <td>
                    <span className={`badge ${a.severity === "critical" ? "danger" : "warn"}`}>{a.severity}</span>
                  </td>
                  <td>{a.state}</td>
                  <td>{fmtDateTime(a.triggeredAt)}</td>
                  <td>{a.message}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </>
  );
}
