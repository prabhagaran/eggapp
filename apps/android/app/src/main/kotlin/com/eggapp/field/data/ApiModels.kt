package com.eggapp.field.data

// Shapes match docs/api/openapi.yaml exactly — field names are the wire
// format (camelCase JSON), not adapted.

data class LoginRequest(val email: String, val password: String)

data class TokenPair(val accessToken: String, val refreshToken: String)

data class RefreshRequest(val refreshToken: String)

data class PushTokenRequest(val fcmToken: String)

data class Farm(
    val id: String,
    val name: String,
    val timezone: String,
    val location: String?,
    val role: String,
)

data class Me(
    val id: String,
    val email: String,
    val name: String?,
)

data class DeviceSummary(
    val id: String,
    val hardwareId: String,
    val name: String?,
    val status: String,
    val lastSeenAt: String?,
    // Snapshot from the device's latest telemetry (US-INC-003) — what
    // the setpoint form shows before the owner edits it.
    val currentTempSetpoint: Double?,
    val currentTempHysteresis: Double?,
    val currentHumSetpoint: Double?,
    val currentHumHysteresis: Double?,
)

data class LatestTelemetry(
    val ts: String,
    val tempC: Double?,
    val humidityPct: Double?,
    val turnerOn: Boolean?,
    val source: String,
)

data class Incubator(
    val id: String,
    val name: String,
    val capacity: Int,
    val defaultSpeciesId: String?,
    val deviceId: String?,
    val device: DeviceSummary?,
    val latestTelemetry: LatestTelemetry?,
)

data class ApiErrorBody(val error: ApiErrorDetail?)
data class ApiErrorDetail(val code: String, val message: String)

data class SpeciesRef(val id: String, val name: String, val incubationDays: Int)
data class IncubatorRef(val id: String, val name: String)

data class Batch(
    val id: String,
    val incubatorId: String,
    val speciesId: String,
    val status: String, // BatchStatus — see packages/shared-types
    val setAt: String?,
    val candlingDays: List<Int>,
    val lockdownAt: String?,
    val expectedHatchAt: String?,
    val viableCount: Int,
    val fertilityPct: Double?,
    val hatchOfSetPct: Double?,
    val hatchOfFertilePct: Double?,
    val species: SpeciesRef?,
    val incubator: IncubatorRef?,
)

// BR-010: clientId makes this create idempotent — replaying the same
// clientId returns the original record (200) instead of duplicating (201).
data class CandlingRequest(
    val dayNo: Int,
    val candledAt: String,
    val fertile: Int,
    val clear: Int,
    val bloodRing: Int,
    val unsure: Int,
    val discrepancyNote: String?,
    val clientId: String,
)

data class HatchRequest(
    val hatchedAt: String,
    val hatched: Int,
    val pippedDead: Int,
    val deadInShell: Int,
    val unhatched: Int,
    val discrepancyNote: String?,
    val clientId: String,
)

// Server returns { session, batch } / { event, batch } — only `batch` is
// needed client-side; Gson ignores the other field automatically.
data class CandlingResponse(val batch: Batch)
data class HatchResponse(val batch: Batch)

// BR-010: clientId makes this create idempotent, same as candling/hatch.
data class CollectionRequest(
    val collectedOn: String,
    val count: Int,
    val avgWeightGrams: Double?,
    val sourceNote: String?,
    val clientId: String,
)

data class DiscardRequest(val count: Int, val reason: String)

data class Collection(
    val id: String,
    val collectedOn: String,
    val count: Int,
    val discardedCount: Int,
    val discardNote: String?,
    val avgWeightGrams: Double?,
    val sourceNote: String?,
    val assignedCount: Int,
    val availableCount: Int,
)

// US-INC-003: at least one field required server-side; all optional here
// since the form only sends what fits in a partial update.
data class SetpointRequest(
    val tempSetpoint: Double? = null,
    val tempHysteresis: Double? = null,
    val humSetpoint: Double? = null,
    val humHysteresis: Double? = null,
)

data class DeviceConfigPayload(
    val tempSetpoint: Double?,
    val tempHysteresis: Double?,
    val humSetpoint: Double?,
    val humHysteresis: Double?,
)

data class DeviceConfig(
    val id: String,
    val version: Int,
    val payload: DeviceConfigPayload,
    val state: String, // "sent" | "received" | "applied" | "unconfirmed"
    val sentAt: String,
    val receivedAt: String?,
    val appliedAt: String?,
)

// ── Flock operations (Phase 2) ──────────────────────────────────

data class FlockSpeciesRef(val id: String, val name: String)

// Server derives ageDays/stage/currentCount — never sent by the client (US-FLK-002/BR-009).
data class Flock(
    val id: String,
    val name: String,
    val speciesId: String,
    val purpose: String, // "layer" | "broiler" | "breeder"
    val hatchEventId: String?,
    val acquisitionDate: String?,
    val acquisitionAgeDays: Int?,
    val acquisitionNote: String?,
    val placedCount: Int,
    val species: FlockSpeciesRef?,
    val ageDays: Int?,
    val stage: String?,
    val currentCount: Int,
)

data class MortalityRecordDto(
    val id: String,
    val date: String,
    val count: Int,
    val cause: String,
    val notes: String?,
)

// BR-010: clientId makes this create idempotent, same as candling/hatch.
data class MortalityRequest(
    val date: String,
    val count: Int,
    val cause: String,
    val notes: String?,
    val clientId: String,
)

data class ComplianceItem(
    val id: String, // templateItemId
    val ageDaysFrom: Int,
    val ageDaysTo: Int,
    val vaccine: String,
    val disease: String,
    val route: String,
    val dueDate: String,
    val status: String, // "administered" | "overdue" | "due" | "upcoming"
    val record: ComplianceRecordRef?,
)

data class ComplianceRecordRef(val id: String, val date: String)

data class VaccinationRecordDto(
    val id: String,
    val templateItemId: String?,
    val date: String,
    val vaccine: String,
    val disease: String,
    val route: String,
    val count: Int,
    val administeredBy: String,
)

// BR-005: immutable; BR-010: clientId idempotent.
data class VaccinationRequest(
    val templateItemId: String?,
    val date: String,
    val vaccine: String,
    val disease: String,
    val route: String,
    val count: Int,
    val administeredBy: String,
    val manufacturer: String?,
    val lotNumber: String?,
    val dose: String?,
    val reactions: String?,
    val clientId: String,
)

data class FeedLogDto(
    val id: String,
    val loggedAt: String,
    val feedType: String,
    val quantityKg: Double,
    val stageMismatch: Boolean,
)

data class FeedLogRequest(
    val loggedAt: String,
    val feedType: String,
    val quantityKg: Double,
    val clientId: String,
)

data class WaterLogDto(
    val id: String,
    val loggedAt: String,
    val quantityLiters: Double,
)

data class WaterLogRequest(
    val loggedAt: String,
    val quantityLiters: Double,
    val clientId: String,
)

data class FlockDetail(
    val id: String,
    val name: String,
    val speciesId: String,
    val purpose: String,
    val acquisitionNote: String?,
    val placedCount: Int,
    val stageOverride: String?, // manual correction; null = auto-derive (see stage below)
    val ageDays: Int?,
    val stage: String?,
    val currentCount: Int,
    val mortalityRecords: List<MortalityRecordDto>,
    val vaccinationRecords: List<VaccinationRecordDto>,
    val recentFeed: List<FeedLogDto>,
    val recentWater: List<WaterLogDto>,
)

// ── Species (Phase 4 — needed for edit-form dropdowns) ──────────────────

data class Species(val id: String, val name: String)

// ── Batch lifecycle (Phase 4) ────────────────────────────────────────────

// Empty body serializes to "{}" (Gson omits null fields by default) —
// matches apps/web's post("/set", {}); server defaults setAt to now().
data class SetBatchRequest(val setAt: String? = null)

// ── Vaccination templates (Phase 4 — new on Android) ─────────────────────

data class VaccinationTemplateItem(
    val id: String,
    val speciesId: String,
    val purpose: String,
    val ageDaysFrom: Int,
    val ageDaysTo: Int,
    val vaccine: String,
    val disease: String,
    val route: String,
)

data class VaccinationTemplateItemRequest(
    val speciesId: String,
    val purpose: String,
    val ageDaysFrom: Int,
    val ageDaysTo: Int,
    val vaccine: String,
    val disease: String,
    val route: String,
)

// ── Inventory (Phase 4 — new on Android) ──────────────────────────────────

data class InventoryItem(
    val id: String,
    val kind: String, // "feed" | "vaccine" | "consumable"
    val name: String,
    val unit: String,
    val quantity: Double,
    val lotNumber: String?,
    val expiry: String?,
    val lowStockThreshold: Double?,
)

data class CreateInventoryItemRequest(
    val kind: String,
    val name: String,
    val unit: String,
    val quantity: Double,
    val lotNumber: String? = null,
    val expiry: String? = null,
    val lowStockThreshold: Double? = null,
)

// kind/quantity aren't editable here — quantity only ever changes through
// adjustStock/deductStock so every change stays in the audit ledger
// (same rule as apps/web's inventory edit form).
data class UpdateInventoryItemRequest(
    val name: String,
    val unit: String,
    val lotNumber: String? = null,
    val expiry: String? = null,
    val lowStockThreshold: Double? = null,
)
