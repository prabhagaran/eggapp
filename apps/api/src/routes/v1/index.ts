import type { FastifyInstance } from "fastify";
import { alertRoutes } from "./alerts.js";
import { authRoutes } from "./auth.js";
import { batchRoutes } from "./batches.js";
import { collectionRoutes } from "./collections.js";
import { deviceRoutes } from "./devices.js";
import { farmRoutes } from "./farms.js";
import { feedWaterRoutes } from "./feedwater.js";
import { flockRoutes } from "./flocks.js";
import { incubatorRoutes } from "./incubators.js";
import { setupRoutes } from "./setup.js";
import { speciesRoutes } from "./species.js";
import { vaccinationRoutes } from "./vaccination.js";

export async function v1Routes(app: FastifyInstance) {
  app.register(setupRoutes);
  app.register(authRoutes);
  app.register(speciesRoutes);
  app.register(farmRoutes);
  app.register(incubatorRoutes);
  app.register(deviceRoutes);
  app.register(collectionRoutes);
  app.register(batchRoutes);
  app.register(alertRoutes);
  app.register(flockRoutes);
  app.register(vaccinationRoutes);
  app.register(feedWaterRoutes);
}
