-- Marks readings fabricated by firmware built with SIMULATE_SENSORS.
-- Defaults false, so every existing row is correctly recorded as real —
-- no simulated data existed before this column.
ALTER TABLE "TelemetryReading" ADD COLUMN "simulated" BOOLEAN NOT NULL DEFAULT false;

-- Partial index: simulated rows are the rare case, and the question worth
-- answering fast is "show me the fabricated ones" (to review or purge
-- them), not "show me the real ones".
CREATE INDEX "TelemetryReading_simulated_idx" ON "TelemetryReading"("deviceId", "ts" DESC)
    WHERE "simulated" = true;
