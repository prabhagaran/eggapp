-- Air-quality and resource-level telemetry channels.
-- All nullable: existing rows keep NULL ("not measured"), and devices
-- without the sensor fitted simply omit the field on the wire. Additive
-- only — no backfill, no default, so this is safe to apply while the
-- current firmware (which sends none of these) keeps publishing.
ALTER TABLE "TelemetryReading" ADD COLUMN "co2Ppm" DOUBLE PRECISION;
ALTER TABLE "TelemetryReading" ADD COLUMN "ammoniaPpm" DOUBLE PRECISION;
ALTER TABLE "TelemetryReading" ADD COLUMN "lightLux" DOUBLE PRECISION;
ALTER TABLE "TelemetryReading" ADD COLUMN "feedLevelPct" DOUBLE PRECISION;
ALTER TABLE "TelemetryReading" ADD COLUMN "waterLevelPct" DOUBLE PRECISION;
