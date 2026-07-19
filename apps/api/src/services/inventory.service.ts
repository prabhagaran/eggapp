import type { FastifyBaseLogger } from "fastify";
import type { InventoryItemKind, Prisma, StockTransactionCause } from "@prisma/client";
import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

// US-INV-003: expiry warning lead time — 30 days, per the user story text.
const EXPIRY_WARNING_DAYS = 30;

export interface CreateItemInput {
  kind: InventoryItemKind;
  name: string;
  unit: string;
  quantity: number; // initial stock, recorded as a "manual" StockTransaction
  lotNumber?: string;
  expiry?: Date;
  lowStockThreshold?: number;
}

// US-INV-001: vaccine items must carry lot + expiry — they feed
// vaccination records, where that provenance matters.
export async function createItem(farmId: string, input: CreateItemInput) {
  if (input.kind === "vaccine" && (!input.lotNumber || !input.expiry)) {
    throw new AppError(400, "vaccine_requires_lot_expiry", "Vaccine items require lotNumber and expiry");
  }
  return getPrisma().$transaction(async (tx) => {
    const item = await tx.inventoryItem.create({
      data: {
        farmId,
        kind: input.kind,
        name: input.name,
        unit: input.unit,
        quantity: input.quantity,
        lotNumber: input.lotNumber,
        expiry: input.expiry,
        lowStockThreshold: input.lowStockThreshold,
      },
    });
    if (input.quantity !== 0) {
      await tx.stockTransaction.create({
        data: { itemId: item.id, delta: input.quantity, cause: "manual", note: "initial stock" },
      });
    }
    return item;
  });
}

export async function listItems(farmId: string) {
  return getPrisma().inventoryItem.findMany({ where: { farmId }, orderBy: [{ kind: "asc" }, { name: "asc" }] });
}

export async function getItem(farmId: string, id: string) {
  const item = await getPrisma().inventoryItem.findFirst({
    where: { id, farmId },
    include: { transactions: { orderBy: { createdAt: "desc" }, take: 50 } },
  });
  if (!item) throw new AppError(404, "not_found", "Inventory item not found");
  return item;
}

export interface UpdateItemInput {
  name?: string;
  unit?: string;
  lotNumber?: string;
  expiry?: Date;
  lowStockThreshold?: number;
}

// Cosmetic/tracking fields only — kind and quantity aren't editable here:
// kind shouldn't change mid-life, and quantity only ever changes through
// adjustStock/deductStock so every change stays in the StockTransaction ledger.
export async function updateItem(farmId: string, id: string, input: UpdateItemInput) {
  const prisma = getPrisma();
  const existing = await prisma.inventoryItem.findFirst({ where: { id, farmId } });
  if (!existing) throw new AppError(404, "not_found", "Inventory item not found");
  if (existing.kind === "vaccine" && (input.lotNumber === "" || input.expiry === null)) {
    throw new AppError(400, "vaccine_requires_lot_expiry", "Vaccine items require lotNumber and expiry");
  }
  return prisma.inventoryItem.update({
    where: { id },
    data: {
      name: input.name,
      unit: input.unit,
      lotNumber: input.lotNumber,
      expiry: input.expiry,
      lowStockThreshold: input.lowStockThreshold,
    },
  });
}

// Manual restock/correction (US-INV-001) — auto-deductions (US-INV-002) go
// through deductStock below instead, inside the same transaction as the
// feed log / vaccination record that triggered them.
export async function adjustStock(farmId: string, id: string, delta: number, note?: string) {
  return getPrisma().$transaction(async (tx) => {
    const item = await tx.inventoryItem.findFirst({ where: { id, farmId } });
    if (!item) throw new AppError(404, "not_found", "Inventory item not found");
    const updated = await tx.inventoryItem.update({ where: { id }, data: { quantity: { increment: delta } } });
    await tx.stockTransaction.create({ data: { itemId: id, delta, cause: "manual", note } });
    return updated;
  });
}

export async function deleteItem(farmId: string, id: string) {
  const item = await getPrisma().inventoryItem.findFirst({ where: { id, farmId } });
  if (!item) throw new AppError(404, "not_found", "Inventory item not found");
  await getPrisma().inventoryItem.delete({ where: { id } });
}

// US-INV-002: called from feedwater.service.ts / vaccination.service.ts,
// inside the same transaction as the FeedLog/VaccinationRecord creation,
// so a log never exists without its stock deduction (or vice versa).
// `delta` is negative — this is deduction, not restock.
export async function deductStock(
  tx: Prisma.TransactionClient,
  farmId: string,
  itemId: string,
  delta: number,
  cause: StockTransactionCause,
  refId: string,
) {
  const item = await tx.inventoryItem.findFirst({ where: { id: itemId, farmId } });
  if (!item) throw new AppError(404, "not_found", "Inventory item not found");
  await tx.inventoryItem.update({ where: { id: itemId }, data: { quantity: { increment: delta } } });
  await tx.stockTransaction.create({ data: { itemId, delta, cause, refId } });
}

// US-INV-003 — periodic sweep (mirrors checkConsumptionAnomalies): fires a
// deduped warning Alert once per item per condition, not on every sweep.
export async function checkStockAlerts(_log: FastifyBaseLogger) {
  const prisma = getPrisma();
  const items = await prisma.inventoryItem.findMany();
  const expiryCutoff = new Date(Date.now() + EXPIRY_WARNING_DAYS * 86_400_000);

  for (const item of items) {
    if (item.lowStockThreshold != null && item.quantity <= item.lowStockThreshold) {
      await raiseOnce(item.farmId, item.id, `[low_stock] ${item.name} is low: ${item.quantity}${item.unit} left (threshold ${item.lowStockThreshold}${item.unit})`);
    }
    if (item.expiry && item.expiry <= expiryCutoff) {
      const days = Math.ceil((item.expiry.getTime() - Date.now()) / 86_400_000);
      const label = days < 0 ? `expired ${-days}d ago` : `expires in ${days}d`;
      await raiseOnce(item.farmId, item.id, `[expiry] ${item.name} (lot ${item.lotNumber ?? "—"}) ${label}`);
    }
  }
}

async function raiseOnce(farmId: string, inventoryItemId: string, message: string) {
  const prisma = getPrisma();
  const messageKey = message.slice(0, message.indexOf("]") + 1);
  const openExisting = await prisma.alert.findFirst({
    where: { farmId, inventoryItemId, state: "open", message: { startsWith: messageKey } },
  });
  if (openExisting) return;
  await prisma.alert.create({ data: { farmId, inventoryItemId, severity: "warning", state: "open", message } });
}
