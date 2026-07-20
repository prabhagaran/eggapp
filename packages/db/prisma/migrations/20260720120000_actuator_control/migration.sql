-- AlterTable
ALTER TABLE "Device" ADD COLUMN     "currentFanOn" BOOLEAN,
ADD COLUMN     "currentFanOverride" BOOLEAN,
ADD COLUMN     "currentTurnerOn" BOOLEAN,
ADD COLUMN     "currentTurnerOverride" BOOLEAN,
ADD COLUMN     "currentHumidifierOn" BOOLEAN,
ADD COLUMN     "currentHumidifierOverride" BOOLEAN,
ADD COLUMN     "currentPumpOn" BOOLEAN,
ADD COLUMN     "currentPumpOverride" BOOLEAN;

-- AlterTable
ALTER TABLE "TelemetryReading" ADD COLUMN     "heaterOn" BOOLEAN,
ADD COLUMN     "coolerOn" BOOLEAN,
ADD COLUMN     "humidifierOn" BOOLEAN,
ADD COLUMN     "fanOn" BOOLEAN,
ADD COLUMN     "pumpOn" BOOLEAN;
