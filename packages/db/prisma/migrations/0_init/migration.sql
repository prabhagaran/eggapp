-- CreateSchema
CREATE SCHEMA IF NOT EXISTS "public";

-- CreateEnum
CREATE TYPE "FarmRole" AS ENUM ('owner', 'manager', 'worker');

-- CreateEnum
CREATE TYPE "DeviceStatus" AS ENUM ('provisioned', 'active', 'offline', 'decommissioned');

-- CreateEnum
CREATE TYPE "ConfigState" AS ENUM ('sent', 'received', 'applied', 'unconfirmed');

-- CreateEnum
CREATE TYPE "TelemetrySource" AS ENUM ('mqtt', 'ble');

-- CreateEnum
CREATE TYPE "DeviceEventType" AS ENUM ('provisioned', 'online', 'offline', 'decommissioned', 'config_sent', 'config_received', 'config_applied');

-- CreateEnum
CREATE TYPE "BatchStatus" AS ENUM ('planned', 'setting', 'incubating', 'lockdown', 'hatching', 'completed', 'closed', 'aborted');

-- CreateEnum
CREATE TYPE "AlertSeverity" AS ENUM ('warning', 'critical');

-- CreateEnum
CREATE TYPE "AlertState" AS ENUM ('open', 'acked', 'resolved');

-- CreateTable
CREATE TABLE "User" (
    "id" TEXT NOT NULL,
    "email" TEXT NOT NULL,
    "passwordHash" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "tokenVersion" INTEGER NOT NULL DEFAULT 0,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "User_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "Farm" (
    "id" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "location" TEXT,
    "timezone" TEXT NOT NULL,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "Farm_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "FarmMembership" (
    "userId" TEXT NOT NULL,
    "farmId" TEXT NOT NULL,
    "role" "FarmRole" NOT NULL,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "FarmMembership_pkey" PRIMARY KEY ("userId","farmId")
);

-- CreateTable
CREATE TABLE "Species" (
    "id" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "incubationDays" INTEGER NOT NULL,
    "lockdownOffsetDays" INTEGER NOT NULL,
    "tempSetC" DOUBLE PRECISION NOT NULL,
    "tempMinC" DOUBLE PRECISION NOT NULL,
    "tempMaxC" DOUBLE PRECISION NOT NULL,
    "humidityIncubMinPct" INTEGER NOT NULL,
    "humidityIncubMaxPct" INTEGER NOT NULL,
    "humidityLockMinPct" INTEGER NOT NULL,
    "humidityLockMaxPct" INTEGER NOT NULL,
    "candlingDays" INTEGER[],
    "turnUntilDay" INTEGER NOT NULL,

    CONSTRAINT "Species_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "Device" (
    "id" TEXT NOT NULL,
    "farmId" TEXT NOT NULL,
    "hardwareId" TEXT NOT NULL,
    "name" TEXT,
    "status" "DeviceStatus" NOT NULL DEFAULT 'provisioned',
    "lastSeenAt" TIMESTAMP(3),
    "firmwareVersion" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "Device_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "Incubator" (
    "id" TEXT NOT NULL,
    "farmId" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "capacity" INTEGER NOT NULL,
    "defaultSpeciesId" TEXT,
    "deviceId" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "Incubator_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "TelemetryReading" (
    "id" BIGSERIAL NOT NULL,
    "deviceId" TEXT NOT NULL,
    "ts" TIMESTAMP(3) NOT NULL,
    "tempC" DOUBLE PRECISION,
    "humidityPct" DOUBLE PRECISION,
    "turnerOn" BOOLEAN,
    "source" "TelemetrySource" NOT NULL DEFAULT 'mqtt',

    CONSTRAINT "TelemetryReading_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "DeviceEvent" (
    "id" TEXT NOT NULL,
    "deviceId" TEXT NOT NULL,
    "ts" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "type" "DeviceEventType" NOT NULL,
    "detail" JSONB,

    CONSTRAINT "DeviceEvent_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "DeviceConfig" (
    "id" TEXT NOT NULL,
    "deviceId" TEXT NOT NULL,
    "version" INTEGER NOT NULL,
    "payload" JSONB NOT NULL,
    "state" "ConfigState" NOT NULL DEFAULT 'sent',
    "sentAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "receivedAt" TIMESTAMP(3),
    "appliedAt" TIMESTAMP(3),

    CONSTRAINT "DeviceConfig_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "EggCollection" (
    "id" TEXT NOT NULL,
    "farmId" TEXT NOT NULL,
    "collectedOn" TIMESTAMP(3) NOT NULL,
    "count" INTEGER NOT NULL,
    "discardedCount" INTEGER NOT NULL DEFAULT 0,
    "discardNote" TEXT,
    "avgWeightGrams" DOUBLE PRECISION,
    "sourceNote" TEXT,
    "clientId" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "EggCollection_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "EggBatch" (
    "id" TEXT NOT NULL,
    "farmId" TEXT NOT NULL,
    "incubatorId" TEXT NOT NULL,
    "speciesId" TEXT NOT NULL,
    "status" "BatchStatus" NOT NULL DEFAULT 'planned',
    "setAt" TIMESTAMP(3),
    "candlingDays" INTEGER[],
    "lockdownAt" TIMESTAMP(3),
    "expectedHatchAt" TIMESTAMP(3),
    "viableCount" INTEGER NOT NULL DEFAULT 0,
    "fertilityPct" DOUBLE PRECISION,
    "hatchOfSetPct" DOUBLE PRECISION,
    "hatchOfFertilePct" DOUBLE PRECISION,
    "abortReason" TEXT,
    "storageOverrideNote" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "EggBatch_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "BatchEggSource" (
    "batchId" TEXT NOT NULL,
    "collectionId" TEXT NOT NULL,
    "count" INTEGER NOT NULL,

    CONSTRAINT "BatchEggSource_pkey" PRIMARY KEY ("batchId","collectionId")
);

-- CreateTable
CREATE TABLE "CandlingSession" (
    "id" TEXT NOT NULL,
    "batchId" TEXT NOT NULL,
    "dayNo" INTEGER NOT NULL,
    "candledAt" TIMESTAMP(3) NOT NULL,
    "fertile" INTEGER NOT NULL,
    "clear" INTEGER NOT NULL,
    "bloodRing" INTEGER NOT NULL,
    "unsure" INTEGER NOT NULL,
    "discrepancyNote" TEXT,
    "recordedById" TEXT,
    "clientId" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "CandlingSession_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "HatchEvent" (
    "id" TEXT NOT NULL,
    "batchId" TEXT NOT NULL,
    "hatchedAt" TIMESTAMP(3) NOT NULL,
    "hatched" INTEGER NOT NULL,
    "pippedDead" INTEGER NOT NULL,
    "deadInShell" INTEGER NOT NULL,
    "unhatched" INTEGER NOT NULL,
    "discrepancyNote" TEXT,
    "recordedById" TEXT,
    "clientId" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "HatchEvent_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "AlertRule" (
    "id" TEXT NOT NULL,
    "farmId" TEXT NOT NULL,
    "incubatorId" TEXT,
    "metric" TEXT NOT NULL,
    "warnMin" DOUBLE PRECISION,
    "warnMax" DOUBLE PRECISION,
    "critMin" DOUBLE PRECISION,
    "critMax" DOUBLE PRECISION,
    "sustainMinutes" INTEGER NOT NULL DEFAULT 10,
    "quietHours" JSONB,
    "enabled" BOOLEAN NOT NULL DEFAULT true,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "AlertRule_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "Alert" (
    "id" TEXT NOT NULL,
    "farmId" TEXT NOT NULL,
    "ruleId" TEXT,
    "incubatorId" TEXT,
    "deviceId" TEXT,
    "severity" "AlertSeverity" NOT NULL,
    "state" "AlertState" NOT NULL DEFAULT 'open',
    "message" TEXT NOT NULL,
    "triggeredAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "ackedAt" TIMESTAMP(3),
    "ackedById" TEXT,
    "resolvedAt" TIMESTAMP(3),

    CONSTRAINT "Alert_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "Notification" (
    "id" TEXT NOT NULL,
    "userId" TEXT NOT NULL,
    "alertId" TEXT,
    "kind" TEXT NOT NULL,
    "title" TEXT NOT NULL,
    "body" TEXT NOT NULL,
    "deepLink" TEXT,
    "channel" TEXT NOT NULL,
    "sentAt" TIMESTAMP(3),
    "readAt" TIMESTAMP(3),

    CONSTRAINT "Notification_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE UNIQUE INDEX "User_email_key" ON "User"("email");

-- CreateIndex
CREATE INDEX "FarmMembership_farmId_idx" ON "FarmMembership"("farmId");

-- CreateIndex
CREATE UNIQUE INDEX "Device_hardwareId_key" ON "Device"("hardwareId");

-- CreateIndex
CREATE INDEX "Device_farmId_idx" ON "Device"("farmId");

-- CreateIndex
CREATE UNIQUE INDEX "Incubator_deviceId_key" ON "Incubator"("deviceId");

-- CreateIndex
CREATE INDEX "Incubator_farmId_idx" ON "Incubator"("farmId");

-- CreateIndex
CREATE INDEX "TelemetryReading_deviceId_ts_idx" ON "TelemetryReading"("deviceId", "ts" DESC);

-- CreateIndex
CREATE INDEX "DeviceEvent_deviceId_ts_idx" ON "DeviceEvent"("deviceId", "ts" DESC);

-- CreateIndex
CREATE UNIQUE INDEX "DeviceConfig_deviceId_version_key" ON "DeviceConfig"("deviceId", "version");

-- CreateIndex
CREATE UNIQUE INDEX "EggCollection_clientId_key" ON "EggCollection"("clientId");

-- CreateIndex
CREATE INDEX "EggCollection_farmId_collectedOn_idx" ON "EggCollection"("farmId", "collectedOn" DESC);

-- CreateIndex
CREATE INDEX "EggBatch_farmId_status_idx" ON "EggBatch"("farmId", "status");

-- CreateIndex
CREATE UNIQUE INDEX "CandlingSession_clientId_key" ON "CandlingSession"("clientId");

-- CreateIndex
CREATE UNIQUE INDEX "CandlingSession_batchId_dayNo_key" ON "CandlingSession"("batchId", "dayNo");

-- CreateIndex
CREATE UNIQUE INDEX "HatchEvent_batchId_key" ON "HatchEvent"("batchId");

-- CreateIndex
CREATE UNIQUE INDEX "HatchEvent_clientId_key" ON "HatchEvent"("clientId");

-- CreateIndex
CREATE INDEX "AlertRule_farmId_idx" ON "AlertRule"("farmId");

-- CreateIndex
CREATE INDEX "Alert_farmId_state_triggeredAt_idx" ON "Alert"("farmId", "state", "triggeredAt" DESC);

-- CreateIndex
CREATE INDEX "Notification_userId_sentAt_idx" ON "Notification"("userId", "sentAt" DESC);

-- AddForeignKey
ALTER TABLE "FarmMembership" ADD CONSTRAINT "FarmMembership_userId_fkey" FOREIGN KEY ("userId") REFERENCES "User"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "FarmMembership" ADD CONSTRAINT "FarmMembership_farmId_fkey" FOREIGN KEY ("farmId") REFERENCES "Farm"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "Device" ADD CONSTRAINT "Device_farmId_fkey" FOREIGN KEY ("farmId") REFERENCES "Farm"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "Incubator" ADD CONSTRAINT "Incubator_farmId_fkey" FOREIGN KEY ("farmId") REFERENCES "Farm"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "Incubator" ADD CONSTRAINT "Incubator_defaultSpeciesId_fkey" FOREIGN KEY ("defaultSpeciesId") REFERENCES "Species"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "Incubator" ADD CONSTRAINT "Incubator_deviceId_fkey" FOREIGN KEY ("deviceId") REFERENCES "Device"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "TelemetryReading" ADD CONSTRAINT "TelemetryReading_deviceId_fkey" FOREIGN KEY ("deviceId") REFERENCES "Device"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "DeviceEvent" ADD CONSTRAINT "DeviceEvent_deviceId_fkey" FOREIGN KEY ("deviceId") REFERENCES "Device"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "DeviceConfig" ADD CONSTRAINT "DeviceConfig_deviceId_fkey" FOREIGN KEY ("deviceId") REFERENCES "Device"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "EggCollection" ADD CONSTRAINT "EggCollection_farmId_fkey" FOREIGN KEY ("farmId") REFERENCES "Farm"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "EggBatch" ADD CONSTRAINT "EggBatch_farmId_fkey" FOREIGN KEY ("farmId") REFERENCES "Farm"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "EggBatch" ADD CONSTRAINT "EggBatch_incubatorId_fkey" FOREIGN KEY ("incubatorId") REFERENCES "Incubator"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "EggBatch" ADD CONSTRAINT "EggBatch_speciesId_fkey" FOREIGN KEY ("speciesId") REFERENCES "Species"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "BatchEggSource" ADD CONSTRAINT "BatchEggSource_batchId_fkey" FOREIGN KEY ("batchId") REFERENCES "EggBatch"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "BatchEggSource" ADD CONSTRAINT "BatchEggSource_collectionId_fkey" FOREIGN KEY ("collectionId") REFERENCES "EggCollection"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "CandlingSession" ADD CONSTRAINT "CandlingSession_batchId_fkey" FOREIGN KEY ("batchId") REFERENCES "EggBatch"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "HatchEvent" ADD CONSTRAINT "HatchEvent_batchId_fkey" FOREIGN KEY ("batchId") REFERENCES "EggBatch"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "AlertRule" ADD CONSTRAINT "AlertRule_farmId_fkey" FOREIGN KEY ("farmId") REFERENCES "Farm"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "Alert" ADD CONSTRAINT "Alert_farmId_fkey" FOREIGN KEY ("farmId") REFERENCES "Farm"("id") ON DELETE CASCADE ON UPDATE CASCADE;

