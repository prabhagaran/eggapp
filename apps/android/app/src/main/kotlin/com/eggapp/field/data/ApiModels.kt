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
