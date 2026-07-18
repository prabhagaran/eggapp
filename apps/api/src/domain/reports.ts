// Pure report-computation helpers — no I/O, same layering as flock.ts/
// vaccination.ts (domain rules live here, services call in).

// domain-knowledge.md §5: "≤3–5% during first 2 weeks of brooding is
// within normal range; ongoing adult mortality <1%/month."
const EARLY_MORTALITY_WINDOW_DAYS = 14;
const EARLY_MORTALITY_NORM_PCT = 5;
const ADULT_MONTHLY_NORM_PCT = 1;

export type MortalityNormStatus = "within_norm" | "elevated" | "unknown";

export function earlyMortalityStatus(earlyDeaths: number, placedCount: number): MortalityNormStatus {
  if (placedCount <= 0) return "unknown";
  const pct = (earlyDeaths / placedCount) * 100;
  return pct > EARLY_MORTALITY_NORM_PCT ? "elevated" : "within_norm";
}

export function monthlyMortalityStatus(recentDeaths: number, countAtWindowStart: number): MortalityNormStatus {
  if (countAtWindowStart <= 0) return "unknown";
  const pct = (recentDeaths / countAtWindowStart) * 100;
  return pct > ADULT_MONTHLY_NORM_PCT ? "elevated" : "within_norm";
}

export { EARLY_MORTALITY_WINDOW_DAYS };

export function toCsv(rows: Record<string, unknown>[]): string {
  if (rows.length === 0) return "";
  const headers = Object.keys(rows[0]!);
  const escape = (v: unknown) => {
    const s = v == null ? "" : v instanceof Date ? v.toISOString() : String(v);
    return /[",\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
  };
  const lines = [headers.join(","), ...rows.map((r) => headers.map((h) => escape(r[h])).join(","))];
  return lines.join("\n");
}
