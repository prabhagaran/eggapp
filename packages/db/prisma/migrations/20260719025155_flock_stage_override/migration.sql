-- CreateEnum
CREATE TYPE "FlockStage" AS ENUM ('brooding', 'grower', 'pre_lay', 'layer', 'broiler_starter', 'broiler_grower', 'broiler_finisher');

-- AlterTable
ALTER TABLE "Flock" ADD COLUMN     "stageOverride" "FlockStage";
