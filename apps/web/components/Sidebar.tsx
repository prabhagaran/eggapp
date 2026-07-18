"use client";
import Link from "next/link";
import { usePathname, useRouter } from "next/navigation";
import { useEffect, useState } from "react";
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
} from "lucide-react";
import { api, clearAuth } from "../lib/api";
import type { Alert } from "../lib/types";

const LINKS = [
  { href: "/", label: "Dashboard", icon: LayoutDashboard },
  { href: "/flocks", label: "Flocks", icon: Bird },
  { href: "/batches", label: "Batches", icon: Egg },
  { href: "/collections", label: "Collections", icon: Package },
  { href: "/incubators", label: "Incubators", icon: Thermometer },
  { href: "/vaccination-templates", label: "Vaccination", icon: Syringe },
  { href: "/devices", label: "Devices", icon: Cpu },
  { href: "/inventory", label: "Inventory", icon: Boxes },
  { href: "/reports", label: "Reports", icon: BarChart3 },
] as const;

export function Sidebar() {
  const pathname = usePathname();
  const router = useRouter();
  const [openAlerts, setOpenAlerts] = useState(0);

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
    <aside className="sidebar">
      <div className="brand">
        <span className="logo">🥚</span>
        eggAPP
      </div>

      <div className="sidebar-section">
        <div className="sidebar-label">Menu</div>
        {LINKS.map(({ href, label, icon: Icon }) => {
          const active = href === "/" ? pathname === "/" : pathname.startsWith(href);
          return (
            <Link key={href} href={href} className={`sidebar-link${active ? " active" : ""}`}>
              <Icon />
              {label}
            </Link>
          );
        })}
        <Link href="/alerts" className={`sidebar-link${pathname.startsWith("/alerts") ? " active" : ""}`}>
          <Bell />
          Alerts
          {openAlerts > 0 && <span className="count">{openAlerts}</span>}
        </Link>
      </div>

      <div className="sidebar-spacer" />

      <div className="sidebar-section">
        <div className="sidebar-label">General</div>
        <Link href="/team" className={`sidebar-link${pathname.startsWith("/team") ? " active" : ""}`}>
          <Users />
          Team
        </Link>
        <button
          className="sidebar-logout"
          onClick={() => {
            clearAuth();
            router.push("/login");
          }}
        >
          <LogOut />
          Log out
        </button>
      </div>
    </aside>
  );
}
