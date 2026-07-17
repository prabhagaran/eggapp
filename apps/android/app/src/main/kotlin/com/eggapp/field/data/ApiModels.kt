package com.eggapp.field.data

// Shapes match docs/api/openapi.yaml exactly — field names are the wire
// format (camelCase JSON), not adapted.

data class LoginRequest(val email: String, val password: String)

data class TokenPair(val accessToken: String, val refreshToken: String)

data class RefreshRequest(val refreshToken: String)

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
