// Lightweight response shapes for the web client (per docs/api/openapi.yaml).
import type { AlertSeverity, AlertState, BatchStatus, DeviceStatus } from "@eggapp/shared-types";

export interface Farm {
  id: string;
  name: string;
  timezone: string;
  location: string | null;
  role: "owner" | "manager" | "worker";
}

export interface Species {
  id: string;
  name: string;
  incubationDays: number;
  lockdownOffsetDays: number;
  candlingDays: number[];
}

export interface DeviceSummary {
  id: string;
  hardwareId: string;
  name: string | null;
  status: DeviceStatus;
  lastSeenAt: string | null;
  currentTempSetpoint: number | null;
  currentTempHysteresis: number | null;
  currentHumSetpoint: number | null;
  currentHumHysteresis: number | null;
}

export type ConfigState = "sent" | "received" | "applied" | "unconfirmed";

export interface DeviceConfig {
  id: string;
  version: number;
  payload: {
    tempSetpoint?: number;
    tempHysteresis?: number;
    humSetpoint?: number;
    humHysteresis?: number;
  };
  state: ConfigState;
  sentAt: string;
  receivedAt: string | null;
  appliedAt: string | null;
}

export interface LatestTelemetry {
  ts: string;
  tempC: number | null;
  humidityPct: number | null;
  turnerOn: boolean | null;
  source: "mqtt" | "ble";
}

export interface Incubator {
  id: string;
  name: string;
  capacity: number;
  defaultSpeciesId: string | null;
  deviceId: string | null;
  device: DeviceSummary | null;
  latestTelemetry: LatestTelemetry | null;
}

export interface Device extends DeviceSummary {
  firmwareVersion: string | null;
  incubator: { id: string; name: string } | null;
}

export interface EggCollection {
  id: string;
  collectedOn: string;
  count: number;
  discardedCount: number;
  discardNote: string | null;
  avgWeightGrams: number | null;
  sourceNote: string | null;
  assignedCount: number;
  availableCount: number;
}

export interface CandlingSession {
  id: string;
  dayNo: number;
  candledAt: string;
  fertile: number;
  clear: number;
  bloodRing: number;
  unsure: number;
  discrepancyNote: string | null;
}

export interface HatchEvent {
  id: string;
  hatchedAt: string;
  hatched: number;
  pippedDead: number;
  deadInShell: number;
  unhatched: number;
  discrepancyNote: string | null;
}

export interface Batch {
  id: string;
  incubatorId: string;
  speciesId: string;
  status: BatchStatus;
  setAt: string | null;
  candlingDays: number[];
  lockdownAt: string | null;
  expectedHatchAt: string | null;
  viableCount: number;
  fertilityPct: number | null;
  hatchOfSetPct: number | null;
  hatchOfFertilePct: number | null;
  abortReason: string | null;
  species?: { id: string; name: string; incubationDays: number };
  incubator?: { id: string; name: string };
}

export interface Alert {
  id: string;
  incubatorId: string | null;
  severity: AlertSeverity;
  state: AlertState;
  message: string;
  triggeredAt: string;
  ackedAt: string | null;
  resolvedAt: string | null;
}

export interface BatchDetail extends Batch {
  species: Species & { name: string; id: string; incubationDays: number };
  incubator: { id: string; name: string };
  sources: { collectionId: string; count: number; collection: EggCollection }[];
  candlings: CandlingSession[];
  hatch: HatchEvent | null;
}
