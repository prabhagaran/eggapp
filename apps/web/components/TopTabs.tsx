"use client";
import Link from "next/link";
import { usePathname, useRouter } from "next/navigation";
import { useEffect, useRef, useState } from "react";
import {
  LayoutDashboard,
  Bird,
  Egg,
  Package,
  Thermometer,
  Syringe,
  Cpu,
  Bell,
  Boxes,
  BarChart3,
  Users,
  LogOut,
  MoreHorizontal,
  Home,
} from "lucide-react";
import { api, clearAuth, getFarmId, setFarmId } from "../lib/api";
import type { Alert, Me } from "../lib/types";

// Ordered by expected use. Everything past PRIMARY_COUNT collapses into the
// "More" menu on narrow viewports — 12 destinations do not fit a single row
// the way the 7-tab reference design does, and silently letting them wrap
// into a second row makes the header height jump between pages.
const LINKS = [
  { href: "/", label: "Dashboard", icon: LayoutDashboard },
  { href: "/coops", label: "Coops", icon: Home },
  { href: "/flocks", label: "Flocks", icon: Bird },
  { href: "/batches", label: "Batches", icon: Egg },
  { href: "/incubators", label: "Incubators", icon: Thermometer },
  { href: "/collections", label: "Collections", icon: Package },
  { href: "/alerts", label: "Alerts", icon: Bell },
  { href: "/devices", label: "Devices", icon: Cpu },
  { href: "/vaccination-templates", label: "Vaccination", icon: Syringe },
  { href: "/inventory", label: "Inventory", icon: Boxes },
  { href: "/reports", label: "Reports", icon: BarChart3 },
  { href: "/team", label: "Team", icon: Users },
] as const;

function isActive(pathname: string, href: string): boolean {
  return href === "/" ? pathname === "/" : pathname.startsWith(href);
}

function initials(me: Me | null): string {
  if (!me) return "?";
  const source = me.name?.trim() || me.email;
  const parts = source.split(/[\s@.]+/).filter(Boolean);
  return (parts[0]?.[0] ?? "").concat(parts[1]?.[0] ?? "").toUpperCase() || "?";
}

export function TopTabs() {
  const pathname = usePathname();
  const router = useRouter();
  const [me, setMe] = useState<Me | null>(null);
  const [openAlerts, setOpenAlerts] = useState(0);
  const [moreOpen, setMoreOpen] = useState(false);
  const moreRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    api<Me>("/v1/me").then(setMe).catch(() => {});
  }, [pathname]);

  useEffect(() => {
    const farmId = getFarmId();
    if (!farmId) return;
    const poll = () =>
      api<Alert[]>(`/v1/farms/${farmId}/alerts?state=open`)
        .then((a) => setOpenAlerts(a.length))
        .catch(() => {});
    poll();
    const t = setInterval(poll, 15_000);
    return () => clearInterval(t);
  }, [pathname]);

  // Close the overflow menu on outside click and on navigation — without
  // this it stays open across a route change, hanging over the new page.
  useEffect(() => setMoreOpen(false), [pathname]);
  useEffect(() => {
    if (!moreOpen) return;
    const onDown = (e: MouseEvent) => {
      if (moreRef.current && !moreRef.current.contains(e.target as Node)) setMoreOpen(false);
    };
    document.addEventListener("mousedown", onDown);
    return () => document.removeEventListener("mousedown", onDown);
  }, [moreOpen]);

  async function onCreateFarm() {
    const name = window.prompt("New farm name:");
    if (!name) return;
    const timezone =
      window.prompt("Timezone (e.g. Asia/Kolkata):", Intl.DateTimeFormat().resolvedOptions().timeZone) || "UTC";
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

  return (
    <header className="topnav">
      <div className="topnav-bar">
        <Link href="/" className="topnav-brand">
          <span className="logo">🥚</span>
          <span className="topnav-brand-text">
            <b>eggAPP</b>
            <span>Poultry Farm Management</span>
          </span>
        </Link>

        <div className="topnav-spacer" />

        {me && me.farms.length > 0 && (
          <select
            className="topnav-farm"
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

        <div className="topnav-user">
          <span className="avatar">{initials(me)}</span>
          <span className="who">
            <b>{me?.name || "—"}</b>
            <span>{me?.email ?? ""}</span>
          </span>
        </div>

        <button
          className="topnav-logout"
          onClick={() => {
            clearAuth();
            router.push("/login");
          }}
          aria-label="Log out"
          title="Log out"
        >
          <LogOut />
        </button>
      </div>

      <nav className="topnav-tabs" aria-label="Main">
        {LINKS.map(({ href, label, icon: Icon }) => (
          <Link key={href} href={href} className={`topnav-tab${isActive(pathname, href) ? " active" : ""}`}>
            <Icon />
            {label}
            {href === "/alerts" && openAlerts > 0 && <span className="count">{openAlerts}</span>}
          </Link>
        ))}

        {/* Same links again, reachable from the overflow menu once the row
            is too narrow to show them all. CSS decides which are hidden;
            keeping both in the DOM avoids a resize listener and the
            hydration mismatch that measuring on the client would cause. */}
        <div className="topnav-more" ref={moreRef}>
          <button
            className={`topnav-tab topnav-more-btn${moreOpen ? " active" : ""}`}
            onClick={() => setMoreOpen((v) => !v)}
            aria-expanded={moreOpen}
            aria-haspopup="menu"
          >
            <MoreHorizontal />
            More
          </button>
          {moreOpen && (
            <div className="topnav-more-menu" role="menu">
              {LINKS.map(({ href, label, icon: Icon }) => (
                <Link
                  key={href}
                  href={href}
                  role="menuitem"
                  className={`topnav-more-item${isActive(pathname, href) ? " active" : ""}`}
                >
                  <Icon />
                  {label}
                  {href === "/alerts" && openAlerts > 0 && <span className="count">{openAlerts}</span>}
                </Link>
              ))}
            </div>
          )}
        </div>
      </nav>
    </header>
  );
}
