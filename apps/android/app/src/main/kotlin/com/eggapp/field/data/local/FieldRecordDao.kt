package com.eggapp.field.data.local

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

@Dao
interface FieldRecordDao {
    // OnConflictStrategy.IGNORE: if a record with this clientId already
    // exists locally (e.g. a duplicate save attempt), keep the original
    // rather than overwrite — same idempotency spirit as the server side.
    @Insert(onConflict = OnConflictStrategy.IGNORE)
    suspend fun insertCandling(entity: CandlingEntity)

    @Insert(onConflict = OnConflictStrategy.IGNORE)
    suspend fun insertHatch(entity: HatchEntity)

    @Insert(onConflict = OnConflictStrategy.IGNORE)
    suspend fun insertCollection(entity: CollectionEntity)

    @Query("SELECT * FROM pending_candling WHERE status = 'queued' ORDER BY createdAtMillis")
    suspend fun pendingCandlings(): List<CandlingEntity>

    @Query("SELECT * FROM pending_hatch WHERE status = 'queued' ORDER BY createdAtMillis")
    suspend fun pendingHatches(): List<HatchEntity>

    @Query("SELECT * FROM pending_collection WHERE status = 'queued' ORDER BY createdAtMillis")
    suspend fun pendingCollections(): List<CollectionEntity>

    @Query("UPDATE pending_candling SET status = :status, errorMessage = :error WHERE clientId = :clientId")
    suspend fun updateCandlingStatus(clientId: String, status: String, error: String?)

    @Query("UPDATE pending_hatch SET status = :status, errorMessage = :error WHERE clientId = :clientId")
    suspend fun updateHatchStatus(clientId: String, status: String, error: String?)

    @Query("UPDATE pending_collection SET status = :status, errorMessage = :error WHERE clientId = :clientId")
    suspend fun updateCollectionStatus(clientId: String, status: String, error: String?)

    // Offline-first reads: the batch screen shows locally-saved records
    // immediately, synced or not — the user shouldn't need connectivity
    // to see what they just recorded.
    @Query("SELECT * FROM pending_candling WHERE batchId = :batchId ORDER BY dayNo")
    fun observeCandlingsForBatch(batchId: String): Flow<List<CandlingEntity>>

    @Query("SELECT * FROM pending_hatch WHERE batchId = :batchId LIMIT 1")
    fun observeHatchForBatch(batchId: String): Flow<HatchEntity?>

    @Query("SELECT * FROM pending_collection WHERE farmId = :farmId ORDER BY createdAtMillis DESC")
    fun observeCollectionsForFarm(farmId: String): Flow<List<CollectionEntity>>
}
