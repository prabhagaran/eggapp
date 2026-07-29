-- ADR 0009: Coop becomes a first-class entity and coop monitoring devices
-- bind to it. Additive and fully nullable — existing flocks keep working
-- with no coop, and no existing row is rewritten.

CREATE TABLE "Coop" (
    "id" TEXT NOT NULL,
    "farmId" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "capacity" INTEGER,
    "deviceId" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT "Coop_pkey" PRIMARY KEY ("id")
);

-- One device per coop, mirroring Incubator.deviceId's unique binding.
CREATE UNIQUE INDEX "Coop_deviceId_key" ON "Coop"("deviceId");
CREATE INDEX "Coop_farmId_idx" ON "Coop"("farmId");

ALTER TABLE "Coop" ADD CONSTRAINT "Coop_farmId_fkey"
    FOREIGN KEY ("farmId") REFERENCES "Farm"("id") ON DELETE CASCADE ON UPDATE CASCADE;
ALTER TABLE "Coop" ADD CONSTRAINT "Coop_deviceId_fkey"
    FOREIGN KEY ("deviceId") REFERENCES "Device"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- Where a flock is housed. SET NULL on coop delete: losing the building
-- record must never cascade into deleting the birds' ledger.
ALTER TABLE "Flock" ADD COLUMN "coopId" TEXT;
ALTER TABLE "Flock" ADD CONSTRAINT "Flock_coopId_fkey"
    FOREIGN KEY ("coopId") REFERENCES "Coop"("id") ON DELETE SET NULL ON UPDATE CASCADE;
