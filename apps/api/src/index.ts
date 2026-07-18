import "dotenv/config";
import { buildApp } from "./app.js";
import { config } from "./config.js";
import { startMqttIngest } from "./infra/mqtt/ingest.js";
import { checkDeviceSilence } from "./services/alert.service.js";
import { checkUnconfirmedConfigs } from "./services/deviceConfig.service.js";
import { checkVaccinationDue } from "./services/vaccination.service.js";
import { checkConsumptionAnomalies } from "./services/feedwater.service.js";

const app = buildApp();

app.listen({ port: config.port, host: "0.0.0.0" }).catch((err) => {
  app.log.error(err);
  process.exit(1);
});

const mqttClient = startMqttIngest(app.log);
app.addHook("onClose", (_instance, done) => {
  mqttClient?.end(false, {}, () => done());
  if (!mqttClient) done();
});

// US-ENV-004: periodic check, independent of any single telemetry message
// (a device that's gone silent is exactly the case with no message to
// react to). 60s cadence is plenty against a 5-minute threshold.
const deviceSilenceInterval = setInterval(() => {
  checkDeviceSilence(app.log).catch((err) => app.log.error({ err }, "[alerts] device silence check failed"));
}, 60_000);
app.addHook("onClose", (_instance, done) => {
  clearInterval(deviceSilenceInterval);
  done();
});

// US-INC-003: same rationale as the device-silence check above — a
// config that never gets acked needs a periodic sweep, not a reaction
// to any single message.
const unconfirmedConfigInterval = setInterval(() => {
  checkUnconfirmedConfigs(app.log).catch((err) => app.log.error({ err }, "[config] unconfirmed check failed"));
}, 60_000);
app.addHook("onClose", (_instance, done) => {
  clearInterval(unconfirmedConfigInterval);
  done();
});

// US-VAC-002 / US-FED-003 / US-WTR-002: lower-frequency concerns than
// device telemetry — due dates and consumption trends don't change
// minute to minute, so a 15-minute sweep is plenty and avoids needless
// DB load at every farm's flock/log tables.
const vaccinationDueInterval = setInterval(() => {
  checkVaccinationDue(app.log).catch((err) => app.log.error({ err }, "[vaccination] due check failed"));
}, 15 * 60_000);
app.addHook("onClose", (_instance, done) => {
  clearInterval(vaccinationDueInterval);
  done();
});

const consumptionAnomalyInterval = setInterval(() => {
  checkConsumptionAnomalies(app.log).catch((err) => app.log.error({ err }, "[feedwater] anomaly check failed"));
}, 15 * 60_000);
app.addHook("onClose", (_instance, done) => {
  clearInterval(consumptionAnomalyInterval);
  done();
});
