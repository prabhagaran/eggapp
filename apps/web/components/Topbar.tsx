"use client";
import { usePathname } from "next/navigation";
import Link from "next/link";
import { useEffect, useState } from "react";
import { Search, Bell } from "lucide-react";
import { api, getFarmId, setFarmId } from "../lib/api";
import type { Alert, Me } from "../lib/types";

const TITLES: Record<string, string> = {
  "/": "Dashboard",
  "/flocks": "Flocks",
  "/batches": "Batches",
  "/collections": "Egg Collections",
  "/incubators": "Incubators",
  "/vaccination-templates": "Vaccination Templates",
  "/devices": "Devices",
  "/alerts": "Alerts",
  "/inventory": "Inventory",
  "/reports": "Reports",
  "/team": "Team",
};

function titleFor(pathname: string): string {
  if (TITLES[pathname]) return TITLES[pathname];
  const base = "/" + pathname.split("/")[1];
  return TITLES[base] ?? "eggAPP";
}

function initials(me: Me | null): string {
  if (!me) return "?";
  const source = me.name?.trim() || me.email;
  const parts = source.split(/[\s@.]+/).filter(Boolean);
  return (parts[0]?.[0] ?? "").concat(parts[1]?.[0] ?? "").toUpperCase() || "?";
}

export function Topbar() {
  const pathname = usePathname();
  const [me, setMe] = useState<Me | null>(null);
  const [openAlerts, setOpenAlerts] = useState(0);

  useEffect(() => {
    api<Me>("/v1/me").then(setMe).catch(() => {});
  }, [pathname]);

  async function onCreateFarm() {
    const name = window.prompt("New farm name:");
    if (!name) return;
    const timezone = window.prompt("Timezone (e.g. Asia/Kolkata):", Intl.DateTimeFormat().resolvedOptions().timeZone) || "UTC";
    const location = window.prompt("Location (optional):") || undefined;
    const farm = await api<{ id: string }>("/v1/farms", { method: "POST", body: { name, timezone, location } });
    setFarmId(farm.id);
    // Full reload, not router.push — every page's useAuthedFarm() reads
    // farmId from localStorage once on mount; a client-nav wouldn't
    // re-read it for already-mounted components.
    window.location.href = "/";
  }

  function onSwitchFarm(id: string) {
    if (id === getFarmId()) return;
    setFarmId(id);
    window.location.href = "/";
  }

  useEffect(() => {
    const farmId = localStorage.getItem("farmId");
    if (!farmId) return;
    const poll = () =>
      api<Alert[]>(`/v1/farms/${farmId}/alerts?state=open`)
        .then((a) => setOpenAlerts(a.length))
        .catch(() => {});
    poll();
    const t = setInterval(poll, 15_000);
    return () => clearInterval(t);
  }, [pathname]);

  return (
    <header className="topbar">
      <strong style={{ fontSize: "1rem" }}>{titleFor(pathname)}</strong>
      <div className="topbar-search">
        <Search />
        <input placeholder="Search" disabled />
      </div>
      <div className="topbar-spacer" />
      {me && me.farms.length > 0 && (
        <select
          value={getFarmId() ?? ""}
          onChange={(e) => (e.target.value === "__new__" ? onCreateFarm() : onSwitchFarm(e.target.value))}
          aria-label="Farm"
        >
          {me.farms.map((f) => (
            <option key={f.id} value={f.id}>
              {f.name}
            </option>
          ))}
          <option value="__new__">+ New farm…</option>
        </select>
      )}
      <Link href="/alerts" className="topbar-icon-btn" aria-label="Alerts">
        <Bell />
        {openAlerts > 0 && <span className="dot" />}
      </Link>
      <div className="topbar-user">
        <span className="avatar">{initials(me)}</span>
        <span className="who">
          <b>{me?.name || "—"}</b>
          <span>{me?.email ?? ""}</span>
        </span>
      </div>
    </header>
  );
}
