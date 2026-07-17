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
