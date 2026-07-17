// Alert evaluation (BR-006) — called from MQTT ingest after each telemetry
// write. No business rules live in the ingest adapter; this is where they
// belong (system-architecture.md layering).
import type { AlertSeverity as PrismaAlertSeverity } from "@prisma/client";
import {
  type AlertMetric,
  type Bounds,
  classify,
  effectiveBounds,
  type IncubationStage,
} from "../domain/alerting.js";
import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

// Sustain tracking (BR-006: warning requires the breach to persist for
// sustainMinutes; critical fires immediately). In-memory, keyed per
// incubator+metric — fine for a single-process personal deployment; a
// restart just resets the sustain window, which delays a warning alert
// by at most sustainMinutes, never causes a false one.
const breachSince = new Map<string, number>();
const DEFAULT_SUSTAIN_MINUTES = 10;

async function effectiveBoundsFor(
  farmId: string,
  incubatorId: string,
  species: Parameters<typeof effectiveBounds>[0],
  stage: IncubationStage,
  metric: AlertMetric,
): Promise<{ bounds: Bounds; sustainMinutes: number }> {
  const rule = await getPrisma().alertRule.findFirst({
    where: { farmId, incubatorId, metric, enabled: true },
  });
  if (rule && rule.warnMin != null && rule.warnMax != null && rule.critMin != null && rule.critMax != null) {
    return {
      bounds: { warnMin: rule.warnMin, warnMax: rule.warnMax, critMin: rule.critMin, critMax: rule.critMax },
      sustainMinutes: rule.sustainMinutes,
    };
  }
  return { bounds: effectiveBounds(species, stage, metric), sustainMinutes: DEFAULT_SUSTAIN_MINUTES };
}

async function evaluateMetric(
  farmId: string,
  incubatorId: string,
  metric: AlertMetric,
  value: number | null,
  species: Parameters<typeof effectiveBounds>[0],
  stage: IncubationStage,
) {
  const prisma = getPrisma();
  const key = `${incubatorId}:${metric}`;
  const { bounds, sustainMinutes } = await effectiveBoundsFor(farmId, incubatorId, species, stage, metric);
  const severity = classify(value, bounds);

  // metric isn't a column on Alert; scope by message prefix so temp and
  // humidity alerts on the same incubator are tracked independently
  // (an incubator can be breaching both at once). Simpler than adding a
  // metric column for a v1 with exactly two metrics.
  const openForMetric = await prisma.alert.findFirst({
    where: { incubatorId, farmId, state: "open", message: { startsWith: `[${metric}]` } },
  });

  if (severity === "ok") {
    breachSince.delete(key);
    if (openForMetric) {
      await prisma.alert.update({ where: { id: openForMetric.id }, data: { state: "resolved", resolvedAt: new Date() } });
    }
    return;
  }

  let confirmed = severity === "critical"; // critical fires immediately (BR-006)
  if (!confirmed) {
    const since = breachSince.get(key) ?? Date.now();
    breachSince.set(key, since);
    confirmed = Date.now() - since >= sustainMinutes * 60_000;
  } else {
    breachSince.delete(key); // critical doesn't need sustain bookkeeping
  }
  if (!confirmed) return;

  if (openForMetric) {
    if (openForMetric.severity === severity) return; // already alerting at this severity
    // Severity changed (e.g. warning -> critical): resolve and re-raise.
    await prisma.alert.update({ where: { id: openForMetric.id }, data: { state: "resolved", resolvedAt: new Date() } });
  }

  const label = metric === "temp_c" ? "Temperature" : "Humidity";
  const unit = metric === "temp_c" ? "°C" : "%";
  await prisma.alert.create({
    data: {
      farmId,
      incubatorId,
      severity: severity as PrismaAlertSeverity,
      state: "open",
      message: `[${metric}] ${label} out of range: ${value}${unit} (bounds ${bounds.warnMin}-${bounds.warnMax}${unit} warn / ${bounds.critMin}-${bounds.critMax}${unit} crit)`,
    },
  });
}

export async function evaluateTelemetry(
  deviceId: string,
  reading: { tempC: number | null; humidityPct: number | null },
) {
  const prisma = getPrisma();
  const incubator = await prisma.incubator.findUnique({
    where: { deviceId },
    include: {
      defaultSpecies: true,
      batches: { where: { status: { in: ["incubating", "lockdown", "hatching"] } }, take: 1 },
    },
  });
  // No eggs to protect: unbound incubator (shouldn't happen — telemetry
  // is looked up by device already), no active batch, or no species set.
  if (!incubator || incubator.batches.length === 0 || !incubator.defaultSpecies) return;

  const batch = incubator.batches[0]!;
  const stage: IncubationStage = batch.status === "incubating" ? "incubating" : "lockdown";

  await evaluateMetric(incubator.farmId, incubator.id, "temp_c", reading.tempC, incubator.defaultSpecies, stage);
  await evaluateMetric(incubator.farmId, incubator.id, "humidity_pct", reading.humidityPct, incubator.defaultSpecies, stage);
}

export async function listAlerts(farmId: string, state?: "open" | "acked" | "resolved") {
  return getPrisma().alert.findMany({
    where: { farmId, ...(state ? { state } : {}) },
    orderBy: { triggeredAt: "desc" },
    take: 100,
  });
}

export async function ackAlert(farmId: string, id: string, userId: string) {
  const prisma = getPrisma();
  const alert = await prisma.alert.findFirst({ where: { id, farmId } });
  if (!alert) throw new AppError(404, "not_found", "Alert not found");
  if (alert.state !== "open") return alert; // acking a resolved/already-acked alert is a no-op
  return prisma.alert.update({
    where: { id },
    data: { state: "acked", ackedAt: new Date(), ackedById: userId },
  });
}
