// Species reference data from docs/product/domain-knowledge.md §1.
// Idempotent: upserts by slug. Ranged values use the typical midpoint;
// tolerance semantics (warning ±0.3°C etc.) live in alerting logic, while
// tempMin/Max here are the species' critical bounds.
import "dotenv/config";
import { PrismaClient } from "@prisma/client";

const prisma = new PrismaClient();

const species = [
  { id: "chicken", name: "Chicken", incubationDays: 21, tempSetC: 37.6, candlingDays: [7, 14, 18], turnUntilDay: 18 },
  { id: "quail-coturnix", name: "Quail (Coturnix)", incubationDays: 18, tempSetC: 37.6, candlingDays: [6, 12, 14], turnUntilDay: 15, humidityIncubMinPct: 45 },
  { id: "duck", name: "Duck (common)", incubationDays: 28, tempSetC: 37.5, candlingDays: [7, 14, 25], turnUntilDay: 25, humidityIncubMinPct: 55, humidityIncubMaxPct: 60, humidityLockMinPct: 70, humidityLockMaxPct: 75 },
  { id: "duck-muscovy", name: "Muscovy duck", incubationDays: 35, tempSetC: 37.5, candlingDays: [10, 21, 31], turnUntilDay: 31, humidityIncubMinPct: 55, humidityIncubMaxPct: 60, humidityLockMinPct: 70, humidityLockMaxPct: 75 },
  { id: "turkey", name: "Turkey", incubationDays: 28, tempSetC: 37.5, candlingDays: [8, 14, 25], turnUntilDay: 25, humidityIncubMinPct: 55, humidityIncubMaxPct: 60, humidityLockMinPct: 70, humidityLockMaxPct: 75 },
  { id: "goose", name: "Goose", incubationDays: 30, tempSetC: 37.5, candlingDays: [7, 14, 25], turnUntilDay: 27, humidityIncubMinPct: 55, humidityIncubMaxPct: 65, humidityLockMinPct: 73, humidityLockMaxPct: 78 },
] as const;

const defaults = {
  lockdownOffsetDays: 3,
  tempMinC: 36.1,
  tempMaxC: 38.9,
  humidityIncubMinPct: 50,
  humidityIncubMaxPct: 55,
  humidityLockMinPct: 65,
  humidityLockMaxPct: 70,
};

async function main() {
  for (const s of species) {
    const row = { ...defaults, ...s, candlingDays: [...s.candlingDays] };
    await prisma.species.upsert({ where: { id: row.id }, create: row, update: row });
    console.log(`seeded species: ${row.id}`);
  }
}

main()
  .catch((e) => {
    console.error(e);
    process.exit(1);
  })
  .finally(() => prisma.$disconnect());
