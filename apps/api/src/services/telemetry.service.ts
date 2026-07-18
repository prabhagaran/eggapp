// US-ENV-002: history charts (24h / 7d / full-batch window). Raw telemetry
// is retained 90 days per nfr.md, which comfortably covers every range
// here (longest possible batch window is 28 days, for duck eggs) — no
// rollup table needed at this scale.
import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

export type HistoryRange = "24h" | "7d" | "batch";

// Downsample to keep chart payloads reasonable — a full batch window at
// the firmware's 60s telemetry cadence is up to ~40k raw rows, too many
// to usefully plot as individual points. Summary stats below are always
// computed from the full raw set, never the downsampled one.
const MAX_CHART_POINTS = 500;

interface Reading {
  ts: Date;
  tempC: number | null;
  humidityPct: number | null;
}

function summarize(readings: Reading[], pick: (r: Reading) => number | null) {
  const values = readings.map(pick).filter((v): v is number => v != null);
  if (values.length === 0) return { min: null, max: null, avg: null };
  const sum = values.reduce((a, b) => a + b, 0);
  return { min: Math.min(...values), max: Math.max(...values), avg: sum / values.length };
}

function downsample(readings: Reading[]): Reading[] {
  if (readings.length <= MAX_CHART_POINTS) return readings;
  const from = readings[0]!.ts.getTime();
  const to = readings[readings.length - 1]!.ts.getTime();
  const bucketMs = Math.max(1, (to - from) / MAX_CHART_POINTS);

  const buckets = new Map<number, { tempSum: number; tempN: number; humSum: number; humN: number; tsSum: number; n: number }>();
  for (const r of readings) {
    const key = Math.floor((r.ts.getTime() - from) / bucketMs);
    const b = buckets.get(key) ?? { tempSum: 0, tempN: 0, humSum: 0, humN: 0, tsSum: 0, n: 0 };
    if (r.tempC != null) { b.tempSum += r.tempC; b.tempN += 1; }
    if (r.humidityPct != null) { b.humSum += r.humidityPct; b.humN += 1; }
    b.tsSum += r.ts.getTime();
    b.n += 1;
    buckets.set(key, b);
  }
  return [...buckets.values()]
    .map((b) => ({
      ts: new Date(b.tsSum / b.n),
      tempC: b.tempN > 0 ? b.tempSum / b.tempN : null,
      humidityPct: b.humN > 0 ? b.humSum / b.humN : null,
    }))
    .sort((a, b) => a.ts.getTime() - b.ts.getTime());
}

export async function getIncubatorHistory(
  farmId: string,
  incubatorId: string,
  range: HistoryRange,
  batchId?: string,
) {
  const prisma = getPrisma();
  const incubator = await prisma.incubator.findFirst({ where: { id: incubatorId, farmId } });
  if (!incubator) throw new AppError(404, "not_found", "Incubator not found");
  if (!incubator.deviceId) return { range, from: null, to: null, summary: emptySummary(), points: [] };

  let from: Date;
  let to: Date;
  if (range === "batch") {
    if (!batchId) throw new AppError(400, "batch_id_required", "batchId is required for range=batch");
    const batch = await prisma.eggBatch.findFirst({
      where: { id: batchId, farmId, incubatorId },
      include: { hatch: { select: { hatchedAt: true } } },
    });
    if (!batch || !batch.setAt) throw new AppError(404, "not_found", "Batch not found or not yet set");
    from = batch.setAt;
    to = batch.hatch?.hatchedAt ?? new Date();
  } else {
    to = new Date();
    from = new Date(to.getTime() - (range === "24h" ? 24 * 60 * 60_000 : 7 * 24 * 60 * 60_000));
  }

  const readings = await prisma.telemetryReading.findMany({
    where: { deviceId: incubator.deviceId, ts: { gte: from, lte: to } },
    orderBy: { ts: "asc" },
    select: { ts: true, tempC: true, humidityPct: true },
  });

  return {
    range,
    from: from.toISOString(),
    to: to.toISOString(),
    summary: {
      temp: summarize(readings, (r) => r.tempC),
      humidity: summarize(readings, (r) => r.humidityPct),
    },
    points: downsample(readings).map((r) => ({ ts: r.ts.toISOString(), tempC: r.tempC, humidityPct: r.humidityPct })),
  };
}

function emptySummary() {
  return { temp: { min: null, max: null, avg: null }, humidity: { min: null, max: null, avg: null } };
}
