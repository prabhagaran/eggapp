package com.eggapp.field.data.local

import androidx.room.Entity
import androidx.room.PrimaryKey

// Sync states match packages/shared-types SYNC_STATES (queued/syncing/synced/conflict).
// clientId doubles as the Room primary key AND the BR-010 idempotency key sent
// to the server — one UUID serves both purposes, no separate row id needed.

@Entity(tableName = "pending_candling")
data class CandlingEntity(
    @PrimaryKey val clientId: String,
    val farmId: String,
    val batchId: String,
    val dayNo: Int,
    val candledAt: String,
    val fertile: Int,
    val clear: Int,
    val bloodRing: Int,
    val unsure: Int,
    val discrepancyNote: String?,
    val status: String, // "queued" | "synced" | "conflict"
    val errorMessage: String? = null,
    val createdAtMillis: Long,
)

@Entity(tableName = "pending_hatch")
data class HatchEntity(
    @PrimaryKey val clientId: String,
    val farmId: String,
    val batchId: String,
    val hatchedAt: String,
    val hatched: Int,
    val pippedDead: Int,
    val deadInShell: Int,
    val unhatched: Int,
    val discrepancyNote: String?,
    val status: String,
    val errorMessage: String? = null,
    val createdAtMillis: Long,
)

@Entity(tableName = "pending_collection")
data class CollectionEntity(
    @PrimaryKey val clientId: String,
    val farmId: String,
    val collectedOn: String,
    val count: Int,
    val avgWeightGrams: Double?,
    val sourceNote: String?,
    val status: String, // "queued" | "synced" | "conflict"
    val errorMessage: String? = null,
    val createdAtMillis: Long,
)

// ── Flock operations (Phase 2) ──────────────────────────────────

@Entity(tableName = "pending_mortality")
data class MortalityEntity(
    @PrimaryKey val clientId: String,
    val farmId: String,
    val flockId: String,
    val date: String,
    val count: Int,
    val cause: String, // "death" | "cull" | "sale"
    val notes: String?,
    val status: String,
    val errorMessage: String? = null,
    val createdAtMillis: Long,
)

@Entity(tableName = "pending_vaccination")
data class VaccinationEntity(
    @PrimaryKey val clientId: String,
    val farmId: String,
    val flockId: String,
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
    val status: String,
    val errorMessage: String? = null,
    val createdAtMillis: Long,
)

@Entity(tableName = "pending_feed_log")
data class FeedLogEntity(
    @PrimaryKey val clientId: String,
    val farmId: String,
    val flockId: String,
    val loggedAt: String,
    val feedType: String,
    val quantityKg: Double,
    val status: String,
    val errorMessage: String? = null,
    val createdAtMillis: Long,
)

@Entity(tableName = "pending_water_log")
data class WaterLogEntity(
    @PrimaryKey val clientId: String,
    val farmId: String,
    val flockId: String,
    val loggedAt: String,
    val quantityLiters: Double,
    val status: String,
    val errorMessage: String? = null,
    val createdAtMillis: Long,
)
