"use client";
import { useRouter } from "next/navigation";
import { useEffect, useState } from "react";
import { api, setFarmId, setTokens } from "../../lib/api";
import type { Farm } from "../../lib/types";

export default function SetupPage() {
  const router = useRouter();
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    api<{ initialized: boolean }>("/v1/setup/status", { auth: false })
      .then((s) => {
        if (s.initialized) router.replace("/login");
      })
      .catch(() => setError("API unreachable — is the server running?"));
  }, [router]);

  async function onSubmit(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    setBusy(true);
    setError(null);
    const f = new FormData(e.currentTarget);
    try {
      const res = await api<{ accessToken: string; refreshToken: string; farm: Farm }>("/v1/setup", {
        method: "POST",
        auth: false,
        body: {
          ownerName: f.get("ownerName"),
          email: f.get("email"),
          password: f.get("password"),
          farmName: f.get("farmName"),
          timezone: Intl.DateTimeFormat().resolvedOptions().timeZone,
          location: f.get("location") || undefined,
        },
      });
      setTokens(res.accessToken, res.refreshToken);
      setFarmId(res.farm.id);
      router.replace("/");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Setup failed");
      setBusy(false);
    }
  }

  return (
    <div className="card" style={{ maxWidth: 480, margin: "3rem auto" }}>
      <h1>Welcome to eggAPP</h1>
      <p className="muted">First-run setup: create your account and farm.</p>
      {error && <p className="alert-error">{error}</p>}
      <form className="stack" onSubmit={onSubmit}>
        <label>
          Your name
          <input name="ownerName" required maxLength={120} />
        </label>
        <label>
          Email
          <input name="email" type="email" required />
        </label>
        <label>
          Password (min 8 characters)
          <input name="password" type="password" required minLength={8} />
        </label>
        <label>
          Farm name
          <input name="farmName" required maxLength={120} />
        </label>
        <label>
          Location (optional)
          <input name="location" maxLength={200} />
        </label>
        <button className="primary" disabled={busy}>
          {busy ? "Setting up…" : "Create farm"}
        </button>
      </form>
    </div>
  );
}
