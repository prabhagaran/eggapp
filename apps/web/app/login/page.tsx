"use client";
import { useRouter } from "next/navigation";
import { useEffect, useState } from "react";
import { api, setFarmId, setTokens } from "../../lib/api";
import type { Farm } from "../../lib/types";

export default function LoginPage() {
  const router = useRouter();
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    api<{ initialized: boolean }>("/v1/setup/status", { auth: false })
      .then((s) => {
        if (!s.initialized) router.replace("/setup");
      })
      .catch(() => setError("API unreachable — is the server running?"));
  }, [router]);

  async function onSubmit(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    setBusy(true);
    setError(null);
    const f = new FormData(e.currentTarget);
    try {
      const res = await api<{ accessToken: string; refreshToken: string }>("/v1/auth/login", {
        method: "POST",
        auth: false,
        body: { email: f.get("email"), password: f.get("password") },
      });
      setTokens(res.accessToken, res.refreshToken);
      const farms = await api<Farm[]>("/v1/farms");
      if (farms[0]) setFarmId(farms[0].id);
      router.replace("/");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Login failed");
      setBusy(false);
    }
  }

  return (
    <div className="card" style={{ maxWidth: 400, margin: "3rem auto" }}>
      <h1>Log in</h1>
      {error && <p className="alert-error">{error}</p>}
      <form className="stack" onSubmit={onSubmit}>
        <label>
          Email
          <input name="email" type="email" required />
        </label>
        <label>
          Password
          <input name="password" type="password" required />
        </label>
        <button className="primary" disabled={busy}>
          {busy ? "Logging in…" : "Log in"}
        </button>
      </form>
    </div>
  );
}
