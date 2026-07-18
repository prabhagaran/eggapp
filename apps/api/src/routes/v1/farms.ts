import type { FastifyInstance } from "fastify";
import { z } from "zod";
import { getPrisma } from "../../infra/db.js";
import { seedDefaultVaccinationTemplates } from "../../services/vaccination.service.js";

const createFarmSchema = z.object({
  name: z.string().min(1).max(120),
  timezone: z.string().min(1),
  location: z.string().max(200).optional(),
});

export async function farmRoutes(app: FastifyInstance) {
  app.get("/farms", { preHandler: [app.authenticate] }, async (req) => {
    const memberships = await getPrisma().farmMembership.findMany({
      where: { userId: req.user.sub },
      include: { farm: true },
    });
    return memberships.map((m) => ({
      id: m.farm.id,
      name: m.farm.name,
      timezone: m.farm.timezone,
      location: m.farm.location,
      role: m.role,
    }));
  });

  // US-FRM-002: additional farms — the requesting user becomes Owner of
  // the new farm; existing farms/memberships are untouched.
  app.post("/farms", { preHandler: [app.authenticate] }, async (req, reply) => {
    const body = createFarmSchema.parse(req.body);
    const prisma = getPrisma();
    const farm = await prisma.$transaction(async (tx) => {
      const farm = await tx.farm.create({ data: { name: body.name, timezone: body.timezone, location: body.location } });
      await tx.farmMembership.create({ data: { userId: req.user.sub, farmId: farm.id, role: "owner" } });
      return farm;
    });
    await seedDefaultVaccinationTemplates(farm.id).catch((err) => app.log.error({ err }, "[farms] template seed failed"));
    return reply.code(201).send({ id: farm.id, name: farm.name, timezone: farm.timezone, location: farm.location, role: "owner" as const });
  });
}
