-- CreateEnum
CREATE TYPE "FlockPurpose" AS ENUM ('layer', 'broiler', 'breeder');

-- CreateEnum
CREATE TYPE "MortalityCause" AS ENUM ('death', 'cull', 'sale');

-- AlterTable
ALTER TABLE "EggCollection" ADD COLUMN     "flockId" TEXT;

-- CreateTable
CREATE TABLE "Flock" (
    "id" TEXT NOT NULL,
    "farmId" TEXT NOT NULL,
    "name" TEXT NOT NULL,
    "speciesId" TEXT NOT NULL,
    "purpose" "FlockPurpose" NOT NULL,
    "hatchEventId" TEXT,
    "acquisitionDate" TIMESTAMP(3),
    "acquisitionAgeDays" INTEGER,
    "acquisitionNote" TEXT,
    "placedCount" INTEGER NOT NULL,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "Flock_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "MortalityRecord" (
    "id" TEXT NOT NULL,
    "flockId" TEXT NOT NULL,
    "date" TIMESTAMP(3) NOT NULL,
    "count" INTEGER NOT NULL,
    "cause" "MortalityCause" NOT NULL,
    "notes" TEXT,
    "recordedById" TEXT,
    "clientId" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "MortalityRecord_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "VaccinationTemplateItem" (
    "id" TEXT NOT NULL,
    "farmId" TEXT NOT NULL,
    "speciesId" TEXT NOT NULL,
    "purpose" "FlockPurpose" NOT NULL,
    "ageDaysFrom" INTEGER NOT NULL,
    "ageDaysTo" INTEGER NOT NULL,
    "vaccine" TEXT NOT NULL,
    "disease" TEXT NOT NULL,
    "route" TEXT NOT NULL,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "VaccinationTemplateItem_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "VaccinationRecord" (
    "id" TEXT NOT NULL,
    "flockId" TEXT NOT NULL,
    "templateItemId" TEXT,
    "date" TIMESTAMP(3) NOT NULL,
    "vaccine" TEXT NOT NULL,
    "disease" TEXT NOT NULL,
    "manufacturer" TEXT,
    "lotNumber" TEXT,
    "expiry" TIMESTAMP(3),
    "route" TEXT NOT NULL,
    "dose" TEXT,
    "count" INTEGER NOT NULL,
    "administeredBy" TEXT NOT NULL,
    "reactions" TEXT,
    "nextDueDate" TIMESTAMP(3),
    "amendsRecordId" TEXT,
    "recordedById" TEXT,
    "clientId" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "VaccinationRecord_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "FeedLog" (
    "id" TEXT NOT NULL,
    "flockId" TEXT NOT NULL,
    "loggedAt" TIMESTAMP(3) NOT NULL,
    "feedType" TEXT NOT NULL,
    "quantityKg" DOUBLE PRECISION NOT NULL,
    "stageMismatch" BOOLEAN NOT NULL DEFAULT false,
    "recordedById" TEXT,
    "clientId" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "FeedLog_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "WaterLog" (
    "id" TEXT NOT NULL,
    "flockId" TEXT NOT NULL,
    "loggedAt" TIMESTAMP(3) NOT NULL,
    "quantityLiters" DOUBLE PRECISION NOT NULL,
    "recordedById" TEXT,
    "clientId" TEXT,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "WaterLog_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE UNIQUE INDEX "Flock_hatchEventId_key" ON "Flock"("hatchEventId");

-- CreateIndex
CREATE INDEX "Flock_farmId_idx" ON "Flock"("farmId");

-- CreateIndex
CREATE UNIQUE INDEX "MortalityRecord_clientId_key" ON "MortalityRecord"("clientId");

-- CreateIndex
CREATE INDEX "MortalityRecord_flockId_date_idx" ON "MortalityRecord"("flockId", "date" DESC);

-- CreateIndex
CREATE INDEX "VaccinationTemplateItem_farmId_speciesId_purpose_idx" ON "VaccinationTemplateItem"("farmId", "speciesId", "purpose");

-- CreateIndex
CREATE UNIQUE INDEX "VaccinationRecord_clientId_key" ON "VaccinationRecord"("clientId");

-- CreateIndex
CREATE INDEX "VaccinationRecord_flockId_date_idx" ON "VaccinationRecord"("flockId", "date" DESC);

-- CreateIndex
CREATE UNIQUE INDEX "FeedLog_clientId_key" ON "FeedLog"("clientId");

-- CreateIndex
CREATE INDEX "FeedLog_flockId_loggedAt_idx" ON "FeedLog"("flockId", "loggedAt" DESC);

-- CreateIndex
CREATE UNIQUE INDEX "WaterLog_clientId_key" ON "WaterLog"("clientId");

-- CreateIndex
CREATE INDEX "WaterLog_flockId_loggedAt_idx" ON "WaterLog"("flockId", "loggedAt" DESC);

-- AddForeignKey
ALTER TABLE "EggCollection" ADD CONSTRAINT "EggCollection_flockId_fkey" FOREIGN KEY ("flockId") REFERENCES "Flock"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "Flock" ADD CONSTRAINT "Flock_farmId_fkey" FOREIGN KEY ("farmId") REFERENCES "Farm"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "Flock" ADD CONSTRAINT "Flock_speciesId_fkey" FOREIGN KEY ("speciesId") REFERENCES "Species"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "Flock" ADD CONSTRAINT "Flock_hatchEventId_fkey" FOREIGN KEY ("hatchEventId") REFERENCES "HatchEvent"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "MortalityRecord" ADD CONSTRAINT "MortalityRecord_flockId_fkey" FOREIGN KEY ("flockId") REFERENCES "Flock"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "VaccinationTemplateItem" ADD CONSTRAINT "VaccinationTemplateItem_farmId_fkey" FOREIGN KEY ("farmId") REFERENCES "Farm"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "VaccinationTemplateItem" ADD CONSTRAINT "VaccinationTemplateItem_speciesId_fkey" FOREIGN KEY ("speciesId") REFERENCES "Species"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "VaccinationRecord" ADD CONSTRAINT "VaccinationRecord_flockId_fkey" FOREIGN KEY ("flockId") REFERENCES "Flock"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "VaccinationRecord" ADD CONSTRAINT "VaccinationRecord_templateItemId_fkey" FOREIGN KEY ("templateItemId") REFERENCES "VaccinationTemplateItem"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "VaccinationRecord" ADD CONSTRAINT "VaccinationRecord_amendsRecordId_fkey" FOREIGN KEY ("amendsRecordId") REFERENCES "VaccinationRecord"("id") ON DELETE SET NULL ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "FeedLog" ADD CONSTRAINT "FeedLog_flockId_fkey" FOREIGN KEY ("flockId") REFERENCES "Flock"("id") ON DELETE CASCADE ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "WaterLog" ADD CONSTRAINT "WaterLog_flockId_fkey" FOREIGN KEY ("flockId") REFERENCES "Flock"("id") ON DELETE CASCADE ON UPDATE CASCADE;

