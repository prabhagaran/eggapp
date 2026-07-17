const DEV_SECRET = "dev-insecure-secret";

export const config = {
  port: Number(process.env.PORT ?? 3001),
  jwtSecret: process.env.JWT_SECRET ?? DEV_SECRET,
  accessTokenTtl: "15m",
  refreshTokenTtl: "30d",
  isProd: process.env.NODE_ENV === "production",
  // MQTT ingest is opt-in: absent MQTT_URL disables it, same pattern as
  // the firmware's own opt-in-via-secrets.h for its MQTT publish side.
  mqttUrl: process.env.MQTT_URL ?? "",
  mqttUsername: process.env.MQTT_API_USERNAME ?? "",
  mqttPassword: process.env.MQTT_API_PASSWORD ?? "",
};

if (config.isProd && config.jwtSecret === DEV_SECRET) {
  throw new Error("JWT_SECRET must be set in production");
}
