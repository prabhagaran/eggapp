import type { Prisma } from "@prisma/client";
import { getPrisma } from "../infra/db.js";
import { AppError } from "../lib/errors.js";

export async function registerDevice(farmId: string, input: { hardwareId: string; name?: string }) {
  // hardwareId is globally unique — a duplicate surfaces as P2002 → 409.
  return getPrisma().$transaction(async (tx) => {
    const device = await tx.device.create({ data: { farmId, ...input } });
    await tx.deviceEvent.create({ data: { deviceId: device.id, type: "provisioned" } });
    return device;
  });
}

export async function listDevices(farmId: string) {
  return getPrisma().device.findMany({
    where: { farmId },
    include: { incubator: { select: { id: true, name: true } } },
    orderBy: { createdAt: "asc" },
  });
}

async function getFarmDevice(tx: Prisma.TransactionClient, farmId: string, deviceId: string) {
  const device = await tx.device.findFirst({ where: { id: deviceId, farmId } });
  if (!device) throw new AppError(404, "not_found", "Device not found");
  return device;
}

// BR-007: at most one incubator per device, one device per incubator.
export async function bindDevice(farmId: string, deviceId: string, incubatorId: string) {
  return getPrisma().$transaction(async (tx) => {
    const device = await getFarmDevice(tx, farmId, deviceId);
    if (device.status === "decommissioned") {
      throw new AppError(409, "device_decommissioned", "Device is decommissioned");
    }
    const incubator = await tx.incubator.findFirst({ where: { id: incubatorId, farmId } });
    if (!incubator) throw new AppError(404, "not_found", "Incubator not found");
    if (incubator.deviceId && incubator.deviceId !== deviceId) {
      throw new AppError(409, "incubator_occupied", "Incubator already has a bound device");
    }
    const bound = await tx.incubator.findUnique({ where: { deviceId } });
    if (bound && bound.id !== incubatorId) {
      throw new AppError(409, "device_already_bound", `Device is bound to incubator '${bound.name}'`);
    }
    return tx.incubator.update({ where: { id: incubatorId }, data: { deviceId } });
  });
}

export async function unbindDevice(farmId: string, deviceId: string) {
  return getPrisma().$transaction(async (tx) => {
    await getFarmDevice(tx, farmId, deviceId);
    const incubator = await tx.incubator.findUnique({ where: { deviceId } });
    if (!incubator) throw new AppError(409, "not_bound", "Device is not bound to an incubator");
    return tx.incubator.update({ where: { id: incubator.id }, data: { deviceId: null } });
  });
}

export async function decommissionDevice(farmId: string, deviceId: string) {
  return getPrisma().$transaction(async (tx) => {
    const device = await getFarmDevice(tx, farmId, deviceId);
    if (device.status === "decommissioned") return device;
    const incubator = await tx.incubator.findUnique({ where: { deviceId } });
    if (incubator) {
      await tx.incubator.update({ where: { id: incubator.id }, data: { deviceId: null } });
    }
    const updated = await tx.device.update({
      where: { id: deviceId },
      data: { status: "decommissioned" },
    });
    await tx.deviceEvent.create({ data: { deviceId, type: "decommissioned" } });
    return updated;
  });
}
