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
