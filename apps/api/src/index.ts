import "dotenv/config";
import { buildApp } from "./app.js";
import { config } from "./config.js";
import { startMqttIngest } from "./infra/mqtt/ingest.js";

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
