import type { FastifyInstance, FastifyReply } from "fastify";
import { z } from "zod";
import { toCsv } from "../../domain/reports.js";
import { requireMembership } from "../../services/access.js";
import * as reports from "../../services/reports.service.js";
import { AppError } from "../../lib/errors.js";

const farmParams = z.object({ farmId: z.uuid() });
const formatQuery = z.object({ format: z.enum(["csv"]).optional() });

function sendCsv(reply: FastifyReply, filename: string, rows: Record<string, unknown>[]) {
  reply.header("content-type", "text/csv").header("content-disposition", `attachment; filename="${filename}"`);
  return reply.send(toCsv(rows));
}

export async function reportRoutes(app: FastifyInstance) {
  app.get("/farms/:farmId/reports/hatch-performance", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    const query = z
      .object({
        speciesId: z.string().optional(),
        incubatorId: z.uuid().optional(),
        from: z.coerce.date().optional(),
        to: z.coerce.date().optional(),
      })
      .merge(formatQuery)
      .parse(req.query);
    const result = await reports.getHatchPerformance(farmId, query);
    if (query.format === "csv") return sendCsv(reply, "hatch-performance.csv", result.batches);
    return result;
  });

  app.get(
    "/farms/:farmId/reports/environmental/:batchId",
    { preHandler: [app.authenticate] },
    async (req, reply) => {
      const { farmId, batchId } = farmParams.extend({ batchId: z.uuid() }).parse(req.params);
      await requireMembership(req.user.sub, farmId);
      const { format } = formatQuery.parse(req.query);
      const result = await reports.getBatchEnvironmentReport(farmId, batchId);
      if (!result) throw new AppError(404, "not_found", "Batch not found");
      if (format === "csv") return sendCsv(reply, `environmental-${batchId}.csv`, result.excursions);
      return result;
    },
  );

  app.get(
    "/farms/:farmId/reports/vaccination-compliance",
    { preHandler: [app.authenticate] },
    async (req, reply) => {
      const { farmId } = farmParams.parse(req.params);
      await requireMembership(req.user.sub, farmId);
      const { format } = formatQuery.parse(req.query);
      const result = await reports.getVaccinationComplianceReport(farmId);
      if (format === "csv") {
        const rows = result.flocks.flatMap((f) => f.items.map((i) => ({ flock: f.flockName, ...i })));
        return sendCsv(reply, "vaccination-compliance.csv", rows);
      }
      return result;
    },
  );

  app.get("/farms/:farmId/reports/mortality-trends", { preHandler: [app.authenticate] }, async (req, reply) => {
    const { farmId } = farmParams.parse(req.params);
    await requireMembership(req.user.sub, farmId);
    const { format } = formatQuery.parse(req.query);
    const result = await reports.getMortalityTrends(farmId);
    if (format === "csv") {
      const rows = result.map((f) => ({
        flock: f.flockName,
        stage: f.stage,
        placedCount: f.placedCount,
        currentCount: f.currentCount,
        deaths: f.byCause.death,
        culls: f.byCause.cull,
        sales: f.byCause.sale,
        earlyMortalityStatus: f.earlyMortality.status,
        recentMonthlyMortalityStatus: f.recentMonthlyMortality.status,
      }));
      return sendCsv(reply, "mortality-trends.csv", rows);
    }
    return result;
  });
}
