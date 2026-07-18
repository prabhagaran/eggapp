"use client";
import { useCallback, useEffect, useState } from "react";
import { api } from "../../lib/api";
import type { InviteResult, Member } from "../../lib/types";
import { fmtDate, useAuthedFarm } from "../../lib/useAuthedFarm";

export default function TeamPage() {
  const farmId = useAuthedFarm();
  const [rows, setRows] = useState<Member[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [invited, setInvited] = useState<InviteResult | null>(null);

  const reload = useCallback(() => {
    if (farmId) api<Member[]>(`/v1/farms/${farmId}/members`).then(setRows).catch(() => {});
  }, [farmId]);
  useEffect(reload, [reload]);

  async function onInvite(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const form = e.currentTarget;
    const f = new FormData(form);
    setError(null);
    setInvited(null);
    try {
      const result = await api<InviteResult>(`/v1/farms/${farmId}/members`, {
        method: "POST",
        body: { email: f.get("email"), role: f.get("role"), name: f.get("name") || undefined },
      });
      form.reset();
      setInvited(result);
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  async function onRemove(userId: string) {
    if (!window.confirm("Remove this member from the farm?")) return;
    try {
      await api(`/v1/farms/${farmId}/members/${userId}`, { method: "DELETE" });
      reload();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed");
    }
  }

  if (!farmId) return null;
  return (
    <>
      <h1>Team</h1>
      {error && <p className="alert-error">{error}</p>}
      {invited && (
        <div className="alert-warn">
          Added <b>{invited.email}</b> as {invited.role}.
          {invited.temporaryPassword && (
            <>
              {" "}
              No email was sent (this deployment has no email service) — share this temporary password with them
              directly: <code>{invited.temporaryPassword}</code>
            </>
          )}
        </div>
      )}

      <div className="card">
        <form className="stack" onSubmit={onInvite}>
          <label>
            Email
            <input name="email" type="email" required />
          </label>
          <label>
            Name (optional, only used if this is a brand-new account)
            <input name="name" />
          </label>
          <label>
            Role
            <select name="role" required defaultValue="worker">
              <option value="manager">Manager</option>
              <option value="worker">Worker</option>
            </select>
          </label>
          <button className="primary">Add to farm</button>
        </form>
      </div>

      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Email</th>
              <th>Role</th>
              <th>Member since</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {(rows ?? []).map((m) => (
              <tr key={m.userId}>
                <td>{m.name || "—"}</td>
                <td>{m.email}</td>
                <td>
                  <span className="badge accent">{m.role}</span>
                </td>
                <td>{fmtDate(m.memberSince)}</td>
                <td>
                  {m.role !== "owner" && (
                    <button className="danger" onClick={() => onRemove(m.userId)}>
                      Remove
                    </button>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </>
  );
}
