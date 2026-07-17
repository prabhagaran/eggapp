"use client";
import Link from "next/link";
import { usePathname, useRouter } from "next/navigation";
import { useEffect, useState } from "react";
import { clearAuth, hasToken } from "../lib/api";

export function NavBar() {
  const pathname = usePathname();
  const router = useRouter();
  const [authed, setAuthed] = useState(false);

  useEffect(() => {
    setAuthed(hasToken());
  }, [pathname]);

  if (pathname === "/login" || pathname === "/setup" || !authed) return null;

  return (
    <nav className="topnav">
      <span className="brand">🥚 eggAPP</span>
      <Link href="/">Dashboard</Link>
      <Link href="/batches">Batches</Link>
      <Link href="/collections">Collections</Link>
      <Link href="/incubators">Incubators</Link>
      <Link href="/devices">Devices</Link>
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
