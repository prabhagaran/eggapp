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
