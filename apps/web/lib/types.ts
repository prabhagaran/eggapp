// Lightweight response shapes for the web client (per docs/api/openapi.yaml).
import type {
  AlertSeverity,
  AlertState,
  BatchStatus,
  DeviceStatus,
  FlockPurpose,
  FlockStage,
  InventoryItemKind,
  MortalityCause,
} from "@eggapp/shared-types";

export interface Farm {
  id: string;
  name: string;
  timezone: string;
  location: string | null;
  role: "owner" | "manager" | "worker";
}

export interface Me {
  id: string;
  email: string;
  name: string | null;
  farms: Farm[];
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
  flockId: string | null;
  flock?: { id: string; name: string } | null;
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
  hatch?: { id: string; flock: { id: string } | null } | null;
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

export interface BatchDetail extends Omit<Batch, "hatch"> {
  species: Species & { name: string; id: string; incubationDays: number };
  incubator: { id: string; name: string };
  sources: { collectionId: string; count: number; collection: EggCollection }[];
  candlings: CandlingSession[];
  hatch: (HatchEvent & { id: string }) | null;
}

// ── Flock operations (Phase 2) ──────────────────────────────────

export interface Flock {
  id: string;
  name: string;
  speciesId: string;
  purpose: FlockPurpose;
  hatchEventId: string | null;
  acquisitionDate: string | null;
  acquisitionAgeDays: number | null;
  acquisitionNote: string | null;
  placedCount: number;
  createdAt: string;
  species?: { id: string; name: string };
  // Derived server-side, never stored (US-FLK-002).
  ageDays: number | null;
  stage: FlockStage | null;
  currentCount: number;
}

export interface MortalityRecord {
  id: string;
  date: string;
  count: number;
  cause: MortalityCause;
  notes: string | null;
}

export interface VaccinationTemplateItem {
  id: string;
  speciesId: string;
  purpose: FlockPurpose;
  ageDaysFrom: number;
  ageDaysTo: number;
  vaccine: string;
  disease: string;
  route: string;
}

export interface VaccinationRecord {
  id: string;
  flockId: string;
  templateItemId: string | null;
  date: string;
  vaccine: string;
  disease: string;
  manufacturer: string | null;
  lotNumber: string | null;
  expiry: string | null;
  route: string;
  dose: string | null;
  count: number;
  administeredBy: string;
  reactions: string | null;
  nextDueDate: string | null;
  amendsRecordId: string | null;
}

export type ComplianceStatus = "administered" | "overdue" | "due" | "upcoming";

export interface ComplianceItem {
  id: string; // templateItemId
  ageDaysFrom: number;
  ageDaysTo: number;
  vaccine: string;
  disease: string;
  route: string;
  dueDate: string;
  status: ComplianceStatus;
  record: { id: string; date: string } | null;
}

export interface FeedLog {
  id: string;
  loggedAt: string;
  feedType: string;
  quantityKg: number;
  stageMismatch: boolean;
}

export interface WaterLog {
  id: string;
  loggedAt: string;
  quantityLiters: number;
}

export interface FlockDetail extends Flock {
  mortalityRecords: MortalityRecord[];
  vaccinationRecords: VaccinationRecord[];
  recentFeed: FeedLog[];
  recentWater: WaterLog[];
}

// ── Inventory (Phase 3) ──────────────────────────────────────────

export interface InventoryItem {
  id: string;
  farmId: string;
  kind: InventoryItemKind;
  name: string;
  unit: string;
  quantity: number;
  lotNumber: string | null;
  expiry: string | null;
  lowStockThreshold: number | null;
  createdAt: string;
}

export interface StockTransaction {
  id: string;
  itemId: string;
  delta: number;
  cause: "feed_log" | "vaccination" | "manual";
  refId: string | null;
  note: string | null;
  createdAt: string;
}

export interface InventoryItemDetail extends InventoryItem {
  transactions: StockTransaction[];
}

// ── Reports (Phase 2/3) ──────────────────────────────────────────

export interface HatchPerformanceRow {
  batchId: string;
  speciesId: string;
  speciesName: string;
  incubatorId: string;
  incubatorName: string;
  setAt: string | null;
  hatchedAt: string | null;
  viableCount: number;
  fertilityPct: number | null;
  hatchOfSetPct: number | null;
  hatchOfFertilePct: number | null;
}

export interface HatchPerformanceReport {
  batches: HatchPerformanceRow[];
  trend: { avgFertilityPct: number | null; avgHatchOfSetPct: number | null; avgHatchOfFertilePct: number | null };
}

export interface Excursion {
  severity: "warning" | "critical";
  message: string;
  triggeredAt: string;
  resolvedAt: string | null;
  durationMinutes: number | null;
}

export interface BatchEnvironmentReport {
  batchId: string;
  speciesName: string;
  incubatorName: string;
  range: string;
  from: string | null;
  to: string | null;
  summary: {
    temp: { min: number | null; max: number | null; avg: number | null };
    humidity: { min: number | null; max: number | null; avg: number | null };
  };
  points: { ts: string; tempC: number | null; humidityPct: number | null }[];
  excursions: Excursion[];
}

export interface VaccinationComplianceReport {
  flocks: { flockId: string; flockName: string; items: ComplianceItem[] }[];
  totals: { administered: number; overdue: number; due: number; upcoming: number };
}

export type MortalityNormStatus = "within_norm" | "elevated" | "unknown";

export interface MortalityTrendRow {
  flockId: string;
  flockName: string;
  stage: FlockStage | null;
  placedCount: number;
  currentCount: number;
  byCause: { death: number; cull: number; sale: number };
  byMonth: { month: string; death: number; cull: number; sale: number }[];
  earlyMortality: { deaths: number; status: MortalityNormStatus };
  recentMonthlyMortality: { deaths: number; status: MortalityNormStatus };
}

// ── Team / multi-farm (Phase 3) ──────────────────────────────────

export interface Member {
  userId: string;
  email: string;
  name: string | null;
  role: "owner" | "manager" | "worker";
  memberSince: string;
}

export interface InviteResult extends Member {
  temporaryPassword?: string;
}
