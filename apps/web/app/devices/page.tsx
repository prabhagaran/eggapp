"use client";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../lib/api";
import type { Device, Incubator } from "../../lib/types";
import { fmtDate, useAuthedFarm } from "../../lib/useAuthedFarm";

export default function DevicesPage() {
  const farmId = useAuthedFarm();
  const [rows, setRows] = useState<Device[] | null>(null);
  const [incubators, setIncubators] = useState<Incubator[]>([]);
  const [error, setError] = useState<string | null>(null);

  const reload = useCallback(() => {
    if (!farmId) return;
    api<Device[]>(`/v1/farms/${farmId}/devices`).then(setRows);
    api<Incubator[]>(`/v1/farms/${farmId}/incubators`).then(setIncubators);
  }, [farmId]);
  useEffect(reload, [reload]);

  async function call(path: string, body?: unknown) {
    setError(null);
    try {
      await api(path, { method: "POST", body });
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onRegister(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const form = e.currentTarget;
    const f = new FormData(form);
    await call(`/v1/farms/${farmId}/devices`, {
      hardwareId: f.get("hardwareId"),
      name: f.get("name") || undefined,
    });
    form.reset();
  }

  if (!farmId) return null;
  const freeIncubators = incubators.filter((i) => !i.deviceId);

  return (
    <>
      <h1>Devices</h1>
      <p className="muted">
        Registering here creates the platform record. Physical provisioning (WiFi credentials,
        pairing) happens via BLE from the Android app; MQTT credentials per
        infra/docker/mosquitto/README.md.
      </p>
      {error && <p className="alert-error">{error}</p>}

      <div className="card">
        <form className="row" onSubmit={onRegister}>
          <label>
            Hardware ID (ESP32 MAC)
            <input name="hardwareId" required placeholder="A0:B7:65:12:34:56" />
          </label>
          <label>
            Name
            <input name="name" placeholder="Incubator ESP #1" />
          </label>
          <button className="primary">Register device</button>
        </form>
      </div>

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Device</th>
              <th>Status</th>
              <th>Last seen</th>
              <th>Bound to</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {(rows ?? []).map((d) => (
              <tr key={d.id}>
                <td>
                  <b>{d.name ?? d.hardwareId}</b>
                  <div className="muted">{d.hardwareId}</div>
                </td>
                <td>
                  <span
                    className={`badge ${d.status === "active" ? "ok" : d.status === "offline" ? "danger" : ""}`}
                  >
                    {d.status}
                  </span>
                </td>
                <td className="muted">{fmtDate(d.lastSeenAt)}</td>
                <td>{d.incubator?.name ?? <span className="muted">—</span>}</td>
                <td>
                  {d.status !== "decommissioned" && (
                    <div className="row">
                      {d.incubator ? (
                        <button
                          className="secondary"
                          onClick={() => call(`/v1/farms/${farmId}/devices/${d.id}/unbind`)}
                        >
                          Unbind
                        </button>
                      ) : (
                        freeIncubators.length > 0 && (
                          <select
                            defaultValue=""
                            onChange={(e) => {
                              if (e.target.value)
                                call(`/v1/farms/${farmId}/devices/${d.id}/bind`, {
                                  incubatorId: e.target.value,
                                });
                            }}
                          >
                            <option value="">Bind to…</option>
                            {freeIncubators.map((i) => (
                              <option key={i.id} value={i.id}>
                                {i.name}
                              </option>
                            ))}
                          </select>
                        )
                      )}
                      <button
                        className="danger"
                        onClick={() => {
                          if (window.confirm(`Decommission ${d.name ?? d.hardwareId}? This revokes it permanently.`))
                            call(`/v1/farms/${farmId}/devices/${d.id}/decommission`);
                        }}
                      >
                        Decommission
                      </button>
                    </div>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      {rows && rows.length === 0 && <p className="muted">No devices registered yet.</p>}
    </>
  );
}
