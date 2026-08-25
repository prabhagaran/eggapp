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

// Incubation day, counted the way the hatchery (and the ESP32 firmware in
// apps/firmware, calcIncubationDay()) counts it: the day the eggs are set is
// day 1, and the number rolls over at local midnight — not at the set
// timestamp's time of day. Comparing calendar days rather than elapsed
// milliseconds keeps a batch set at 15:30 on "day 1" for the rest of that
// day instead of for the next 24 hours. Date.UTC on local Y/M/D pins each
// date to a fixed instant so a DST shift can't make a day 23 or 25 hours.
export function dayOf(setAt: string | null): number | null {
  if (!setAt) return null;
  const set = new Date(setAt);
  if (Number.isNaN(set.getTime())) return null;
  const midnightUTC = (d: Date) => Date.UTC(d.getFullYear(), d.getMonth(), d.getDate());
  const day = Math.floor((midnightUTC(new Date()) - midnightUTC(set)) / 86_400_000) + 1;
  // A batch scheduled ahead of time reads as day 1, matching the firmware's
  // own nowEpoch < startEpoch guard rather than showing 0 or a negative day.
  return day < 1 ? 1 : day;
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
