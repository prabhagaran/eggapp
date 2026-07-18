import { ageDaysFrom, birthDate, deriveStage } from "../domain/flock.js";
import { EARLY_MORTALITY_WINDOW_DAYS, earlyMortalityStatus, monthlyMortalityStatus } from "../domain/reports.js";
import { computeCompliance } from "../domain/vaccination.js";
import { getPrisma } from "../infra/db.js";
import { getIncubatorHistory } from "./telemetry.service.js";

export interface HatchPerformanceFilters {
  speciesId?: string;
  incubatorId?: string;
  from?: Date;
  to?: Date;
}

// US-RPT-001: per-batch metrics (BR-015 — computed once at completion,
// never recomputed here) plus the farm-wide trend across them.
export async function getHatchPerformance(farmId: string, filters: HatchPerformanceFilters) {
  const batches = await getPrisma().eggBatch.findMany({
    where: {
      farmId,
      status: "completed",
      speciesId: filters.speciesId,
      incubatorId: filters.incubatorId,
      ...(filters.from || filters.to
        ? { setAt: { gte: filters.from, lte: filters.to } }
        : {}),
    },
    include: { species: { select: { name: true } }, incubator: { select: { name: true } }, hatch: { select: { hatchedAt: true } } },
    orderBy: { setAt: "asc" },
  });

  const rows = batches.map((b) => ({
    batchId: b.id,
    speciesId: b.speciesId,
    speciesName: b.species.name,
    incubatorId: b.incubatorId,
    incubatorName: b.incubator.name,
    setAt: b.setAt,
    hatchedAt: b.hatch?.hatchedAt ?? null,
    viableCount: b.viableCount,
    fertilityPct: b.fertilityPct,
    hatchOfSetPct: b.hatchOfSetPct,
    hatchOfFertilePct: b.hatchOfFertilePct,
  }));

  const avg = (pick: (r: (typeof rows)[number]) => number | null) => {
    const values = rows.map(pick).filter((v): v is number => v != null);
    return values.length ? values.reduce((a, b) => a + b, 0) / values.length : null;
  };

  return {
    batches: rows,
    trend: {
      avgFertilityPct: avg((r) => r.fertilityPct),
      avgHatchOfSetPct: avg((r) => r.hatchOfSetPct),
      avgHatchOfFertilePct: avg((r) => r.hatchOfFertilePct),
    },
  };
}

// US-RPT-002: reuses the same per-batch telemetry window as the
// incubator detail page's history chart (range=batch), plus an
// excursion list derived from Alert — Alerts already represent bound
// violations for that incubator, no separate excursion tracking needed.
export async function getBatchEnvironmentReport(farmId: string, batchId: string) {
  const prisma = getPrisma();
  const batch = await prisma.eggBatch.findFirst({
    where: { id: batchId, farmId },
    include: { hatch: { select: { hatchedAt: true } }, species: { select: { name: true } }, incubator: { select: { name: true } } },
  });
  if (!batch) return null;

  const history = await getIncubatorHistory(farmId, batch.incubatorId, "batch", batchId);
  const from = batch.setAt ?? batch.createdAt;
  const to = batch.hatch?.hatchedAt ?? new Date();
  const excursions = await prisma.alert.findMany({
    where: { farmId, incubatorId: batch.incubatorId, triggeredAt: { gte: from, lte: to } },
    orderBy: { triggeredAt: "asc" },
  });

  return {
    batchId: batch.id,
    speciesName: batch.species.name,
    incubatorName: batch.incubator.name,
    ...history,
    excursions: excursions.map((a) => ({
      severity: a.severity,
      message: a.message,
      triggeredAt: a.triggeredAt,
      resolvedAt: a.resolvedAt,
      durationMinutes: a.resolvedAt ? Math.round((a.resolvedAt.getTime() - a.triggeredAt.getTime()) / 60_000) : null,
    })),
  };
}

// US-RPT-003: farm-wide vaccination compliance — reuses the same
// per-flock computeCompliance() the flock detail page and due-reminder
// sweep already use, aggregated across every flock in the farm.
export async function getVaccinationComplianceReport(farmId: string) {
  const flocks = await getPrisma().flock.findMany({ where: { farmId }, include: { hatchEvent: true } });

  const perFlock = await Promise.all(
    flocks.map(async (flock) => {
      const birth = birthDate({
        hatchedAt: flock.hatchEvent?.hatchedAt ?? null,
        acquisitionDate: flock.acquisitionDate,
        acquisitionAgeDays: flock.acquisitionAgeDays,
      });
      if (!birth) return { flockId: flock.id, flockName: flock.name, items: [] };

      const [templates, records] = await Promise.all([
        getPrisma().vaccinationTemplateItem.findMany({
          where: { farmId, speciesId: flock.speciesId, purpose: flock.purpose },
        }),
        getPrisma().vaccinationRecord.findMany({ where: { flockId: flock.id }, select: { id: true, templateItemId: true, date: true } }),
      ]);
      return { flockId: flock.id, flockName: flock.name, items: computeCompliance(birth, templates, records) };
    }),
  );

  const totals = { administered: 0, overdue: 0, due: 0, upcoming: 0 };
  for (const flock of perFlock) {
    for (const item of flock.items) totals[item.status] += 1;
  }

  return { flocks: perFlock, totals };
}

// US-RPT-004: mortality by flock/stage/cause, benchmarked against
// domain-knowledge §5 norms (early-brooding cumulative %, ongoing
// monthly %) — a genuine early-warning signal per that doc, not decoration.
export async function getMortalityTrends(farmId: string) {
  const flocks = await getPrisma().flock.findMany({
    where: { farmId },
    include: { hatchEvent: true, mortalityRecords: { orderBy: { date: "asc" } } },
  });

  const now = new Date();
  const monthAgo = new Date(now.getTime() - 30 * 86_400_000);

  return flocks.map((flock) => {
    const birth = birthDate({
      hatchedAt: flock.hatchEvent?.hatchedAt ?? null,
      acquisitionDate: flock.acquisitionDate,
      acquisitionAgeDays: flock.acquisitionAgeDays,
    });

    const byCause = { death: 0, cull: 0, sale: 0 };
    const byMonth = new Map<string, { death: number; cull: number; sale: number }>();
    let earlyDeaths = 0;
    let recentDeaths = 0;
    let totalLost = 0;

    for (const r of flock.mortalityRecords) {
      byCause[r.cause] += r.count;
      totalLost += r.count;
      const monthKey = r.date.toISOString().slice(0, 7);
      const bucket = byMonth.get(monthKey) ?? { death: 0, cull: 0, sale: 0 };
      bucket[r.cause] += r.count;
      byMonth.set(monthKey, bucket);

      if (birth && r.cause === "death") {
        const ageAtDeath = ageDaysFrom(birth, r.date);
        if (ageAtDeath <= EARLY_MORTALITY_WINDOW_DAYS) earlyDeaths += r.count;
        if (r.date >= monthAgo) recentDeaths += r.count;
      }
    }

    const currentCount = flock.placedCount - totalLost;
    const stage = birth ? deriveStage(flock.purpose, ageDaysFrom(birth)) : null;

    return {
      flockId: flock.id,
      flockName: flock.name,
      stage,
      placedCount: flock.placedCount,
      currentCount,
      byCause,
      byMonth: [...byMonth.entries()].map(([month, counts]) => ({ month, ...counts })).sort((a, b) => a.month.localeCompare(b.month)),
      earlyMortality: {
        deaths: earlyDeaths,
        status: earlyMortalityStatus(earlyDeaths, flock.placedCount),
      },
      recentMonthlyMortality: {
        deaths: recentDeaths,
        status: monthlyMortalityStatus(recentDeaths, currentCount + recentDeaths),
      },
    };
  });
}
