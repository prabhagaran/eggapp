import { PrismaClient } from "@prisma/client";
import { randomUUID } from "crypto";
import bcrypt from "bcryptjs";
const prisma = new PrismaClient();

const passwordHash = await bcrypt.hash("android-create-pw", 10);
const user = await prisma.user.create({
  data: { id: randomUUID(), email: "android-create@test.local", passwordHash, name: "Android Create QA" },
});
const farm = await prisma.farm.create({
  data: { id: randomUUID(), name: "Android Create QA (test — safe to delete)", timezone: "Asia/Kolkata" },
});
await prisma.farmMembership.create({ data: { userId: user.id, farmId: farm.id, role: "owner" } });

const species = await prisma.species.findFirst({ where: { name: "Chicken" } });
if (!species) throw new Error("no Chicken species seeded");

// Seed an egg collection so batch-create has something to source from.
const collection = await prisma.eggCollection.create({
  data: { farmId: farm.id, collectedOn: new Date(), count: 20, sourceNote: "test collection" },
});

console.log(JSON.stringify({ farmId: farm.id, userId: user.id, collectionId: collection.id }, null, 2));
await prisma.$disconnect();
