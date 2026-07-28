"use client";
import type { ReactNode } from "react";
import { usePathname } from "next/navigation";
import { useEffect, useState } from "react";
import { hasToken } from "../lib/api";
import { TopTabs } from "./TopTabs";

export function AppShell({ children }: { children: ReactNode }) {
  const pathname = usePathname();
  const [authed, setAuthed] = useState(false);

  useEffect(() => {
    setAuthed(hasToken());
  }, [pathname]);

  const bare = pathname === "/login" || pathname === "/setup" || !authed;
  if (bare) return <main className="bare-main">{children}</main>;

  return (
    <div className="app-shell">
      <TopTabs />
      <main className="app-main">{children}</main>
    </div>
  );
}
