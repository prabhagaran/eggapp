-- DropForeignKey
ALTER TABLE "BatchEggSource" DROP CONSTRAINT "BatchEggSource_collectionId_fkey";

-- AddForeignKey
ALTER TABLE "BatchEggSource" ADD CONSTRAINT "BatchEggSource_collectionId_fkey" FOREIGN KEY ("collectionId") REFERENCES "EggCollection"("id") ON DELETE CASCADE ON UPDATE CASCADE;

