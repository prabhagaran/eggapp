// Canonical lifecycle enums from docs/architecture/domain-model.md.
// Clients never invent states (system-architect owns changes here).
// API request/response types get generated from docs/api/openapi.yaml in
// Phase 1 and exported alongside these.

export const BATCH_STATUSES = [
  "planned",
  "setting",
  "incubating",
  "lockdown",
  "hatching",
  "completed",
  "closed",
  "aborted",
] as const;
export type BatchStatus = (typeof BATCH_STATUSES)[number];

export const DEVICE_STATUSES = ["provisioned", "active", "offline", "decommissioned"] as const;
export type DeviceStatus = (typeof DEVICE_STATUSES)[number];

export const CONFIG_STATES = ["sent", "received", "applied", "unconfirmed"] as const;
export type ConfigState = (typeof CONFIG_STATES)[number];

export const ALERT_STATES = ["open", "acked", "resolved"] as const;
export type AlertState = (typeof ALERT_STATES)[number];

export const ALERT_SEVERITIES = ["warning", "critical"] as const;
export type AlertSeverity = (typeof ALERT_SEVERITIES)[number];

export const FARM_ROLES = ["owner", "manager", "worker"] as const;
export type FarmRole = (typeof FARM_ROLES)[number];

/** Client-side sync state (android-architect) — never sent to the server. */
export const SYNC_STATES = ["queued", "syncing", "synced", "conflict"] as const;
export type SyncState = (typeof SYNC_STATES)[number];

export const FLOCK_PURPOSES = ["layer", "broiler", "breeder"] as const;
export type FlockPurpose = (typeof FLOCK_PURPOSES)[number];

export const MORTALITY_CAUSES = ["death", "cull", "sale"] as const;
export type MortalityCause = (typeof MORTALITY_CAUSES)[number];

// Derived server-side from age + purpose (domain-knowledge §5) — never
// stored, but exported here so clients can render a stable label set.
export const FLOCK_STAGES = [
  "brooding",
  "grower",
  "pre_lay",
  "layer",
  "broiler_starter",
  "broiler_grower",
  "broiler_finisher",
] as const;
export type FlockStage = (typeof FLOCK_STAGES)[number];

export const INVENTORY_ITEM_KINDS = ["feed", "vaccine", "consumable"] as const;
export type InventoryItemKind = (typeof INVENTORY_ITEM_KINDS)[number];

export const STOCK_TRANSACTION_CAUSES = ["feed_log", "vaccination", "manual"] as const;
export type StockTransactionCause = (typeof STOCK_TRANSACTION_CAUSES)[number];
