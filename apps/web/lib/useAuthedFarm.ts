"use client";
// Shared page guard: redirects to /login when unauthenticated, resolves the
// active farm id (first membership if not yet chosen).
import { useRouter } from "next/navigation";
import { useEffect, useState } from "react";
import { api, getFarmId, hasToken, setFarmId } from "./api";
import type { Farm } from "./types";

export function useAuthedFarm() {
  const router = useRouter();
  const [farmId, setId] = useState<string | null>(null);

  useEffect(() => {
    if (!hasToken()) {
      router.replace("/login");
      return;
    }
    const stored = getFarmId();
    if (stored) {
      setId(stored);
      return;
    }
    api<Farm[]>("/v1/farms").then((farms) => {
      if (farms[0]) {
        setFarmId(farms[0].id);
        setId(farms[0].id);
      } else {
        router.replace("/setup");
      }
    });
  }, [router]);

  return farmId;
}

export function fmtDate(iso: string | null | undefined) {
  return iso ? new Date(iso).toLocaleDateString() : "—";
}

export function dayOf(setAt: string | null): number | null {
  if (!setAt) return null;
  return Math.floor((Date.now() - Date.parse(setAt)) / 86_400_000);
}

// US-INC-002/ENV-001 target ≤60s freshness; 90s gives one missed-interval
// margin before flagging stale (publish cadence is 60s).
export function isFresh(ts: string): boolean {
  return Date.now() - Date.parse(ts) < 90_000;
}

export function fmtAge(ts: string): string {
  const s = Math.floor((Date.now() - Date.parse(ts)) / 1000);
  if (s < 60) return `${s}s ago`;
  return `${Math.floor(s / 60)}m ago`;
}

// Matches apps/android's batchTone() — same status, same meaning, both surfaces.
export function batchBadgeClass(status: string): string {
  if (status === "completed" || status === "hatching") return "badge ok";
  if (status === "aborted" || status === "closed") return "badge";
  return "badge accent";
}
