"use client";
import Link from "next/link";
import { usePathname, useRouter } from "next/navigation";
import { useEffect, useState } from "react";
import { api, clearAuth, hasToken } from "../lib/api";
import type { Alert } from "../lib/types";

export function NavBar() {
  const pathname = usePathname();
  const router = useRouter();
  const [authed, setAuthed] = useState(false);
  const [openAlerts, setOpenAlerts] = useState(0);

  useEffect(() => {
    setAuthed(hasToken());
  }, [pathname]);

  useEffect(() => {
    if (!authed) return;
    const farmId = localStorage.getItem("farmId");
    if (!farmId) return;
    const poll = () =>
      api<Alert[]>(`/v1/farms/${farmId}/alerts?state=open`)
        .then((a) => setOpenAlerts(a.length))
        .catch(() => {});
    poll();
    const t = setInterval(poll, 15_000);
    return () => clearInterval(t);
  }, [authed, pathname]);

  if (pathname === "/login" || pathname === "/setup" || !authed) return null;

  return (
    <nav className="topnav">
      <span className="brand">🥚 eggAPP</span>
      <Link href="/">Dashboard</Link>
      <Link href="/batches">Batches</Link>
      <Link href="/collections">Collections</Link>
      <Link href="/incubators">Incubators</Link>
      <Link href="/flocks">Flocks</Link>
      <Link href="/vaccination-templates">Vaccination</Link>
      <Link href="/devices">Devices</Link>
      <Link href="/alerts">
        Alerts{openAlerts > 0 && <span className="badge danger" style={{ marginLeft: "0.3rem" }}>{openAlerts}</span>}
      </Link>
      <span className="spacer" />
      <button
        onClick={() => {
          clearAuth();
          router.push("/login");
        }}
      >
        Log out
      </button>
    </nav>
  );
}
