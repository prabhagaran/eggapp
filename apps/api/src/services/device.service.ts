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
    include: {
      incubator: { select: { id: true, name: true } },
      coop: { select: { id: true, name: true } },
    },
    orderBy: { createdAt: "asc" },
  });
}

async function getFarmDevice(tx: Prisma.TransactionClient, farmId: string, deviceId: string) {
  const device = await tx.device.findFirst({ where: { id: deviceId, farmId } });
  if (!device) throw new AppError(404, "not_found", "Device not found");
  return device;
}

// ADR 0009: a device binds to an Incubator OR a Coop, never both. This is
// the single point where any binding is created, which is why the mutual
// exclusion is enforced here rather than as a DB constraint — expressing
// "exactly one of two optional inverse relations" in Postgres needs a
// trigger or denormalised check columns, both worse than this guard.
async function currentBinding(tx: Prisma.TransactionClient, deviceId: string) {
  const [incubator, coop] = await Promise.all([
    tx.incubator.findUnique({ where: { deviceId }, select: { id: true, name: true } }),
    tx.coop.findUnique({ where: { deviceId }, select: { id: true, name: true } }),
  ]);
  return { incubator, coop };
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
    const bound = await currentBinding(tx, deviceId);
    if (bound.incubator && bound.incubator.id !== incubatorId) {
      throw new AppError(409, "device_already_bound", `Device is bound to incubator '${bound.incubator.name}'`);
    }
    if (bound.coop) {
      throw new AppError(409, "device_already_bound", `Device is bound to coop '${bound.coop.name}'`);
    }
    return tx.incubator.update({ where: { id: incubatorId }, data: { deviceId } });
  });
}

/** Binds a device to a coop (ADR 0009). Mirrors bindDevice's guards. */
export async function bindDeviceToCoop(farmId: string, deviceId: string, coopId: string) {
  return getPrisma().$transaction(async (tx) => {
    const device = await getFarmDevice(tx, farmId, deviceId);
    if (device.status === "decommissioned") {
      throw new AppError(409, "device_decommissioned", "Device is decommissioned");
    }
    const coop = await tx.coop.findFirst({ where: { id: coopId, farmId } });
    if (!coop) throw new AppError(404, "not_found", "Coop not found");
    if (coop.deviceId && coop.deviceId !== deviceId) {
      throw new AppError(409, "coop_occupied", "Coop already has a bound device");
    }
    const bound = await currentBinding(tx, deviceId);
    if (bound.incubator) {
      throw new AppError(409, "device_already_bound", `Device is bound to incubator '${bound.incubator.name}'`);
    }
    if (bound.coop && bound.coop.id !== coopId) {
      throw new AppError(409, "device_already_bound", `Device is bound to coop '${bound.coop.name}'`);
    }
    return tx.coop.update({ where: { id: coopId }, data: { deviceId } });
  });
}

export async function unbindDevice(farmId: string, deviceId: string) {
  return getPrisma().$transaction(async (tx) => {
    await getFarmDevice(tx, farmId, deviceId);
    const bound = await currentBinding(tx, deviceId);
    if (bound.incubator) {
      return tx.incubator.update({ where: { id: bound.incubator.id }, data: { deviceId: null } });
    }
    if (bound.coop) {
      return tx.coop.update({ where: { id: bound.coop.id }, data: { deviceId: null } });
    }
    throw new AppError(409, "not_bound", "Device is not bound to an incubator or coop");
  });
}

export async function decommissionDevice(farmId: string, deviceId: string) {
  return getPrisma().$transaction(async (tx) => {
    const device = await getFarmDevice(tx, farmId, deviceId);
    if (device.status === "decommissioned") return device;
    const bound = await currentBinding(tx, deviceId);
    if (bound.incubator) {
      await tx.incubator.update({ where: { id: bound.incubator.id }, data: { deviceId: null } });
    }
    if (bound.coop) {
      await tx.coop.update({ where: { id: bound.coop.id }, data: { deviceId: null } });
    }
    const updated = await tx.device.update({
      where: { id: deviceId },
      data: { status: "decommissioned" },
    });
    await tx.deviceEvent.create({ data: { deviceId, type: "decommissioned" } });
    return updated;
  });
}
