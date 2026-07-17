package com.eggapp.field.data

import android.content.Context
import androidx.work.Constraints
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.WorkManager
import com.eggapp.field.data.local.AppDatabase
import com.eggapp.field.data.local.CandlingEntity
import com.eggapp.field.data.local.HatchEntity
import com.eggapp.field.sync.SyncWorker
import java.time.Instant
import java.util.UUID

/**
 * Offline-first writes (BR-010): save locally first — this always
 * succeeds regardless of connectivity — then enqueue a WorkManager
 * request that pushes it whenever the network is actually available.
 * The UI never blocks on network for these saves.
 */
class FieldRecordRepository(context: Context) {
    private val dao = AppDatabase.get(context).fieldRecordDao()
    private val workManager = WorkManager.getInstance(context)

    suspend fun saveCandling(
        farmId: String,
        batchId: String,
        dayNo: Int,
        fertile: Int,
        clear: Int,
        bloodRing: Int,
        unsure: Int,
        discrepancyNote: String?,
    ) {
        dao.insertCandling(
            CandlingEntity(
                clientId = UUID.randomUUID().toString(),
                farmId = farmId,
                batchId = batchId,
                dayNo = dayNo,
                candledAt = Instant.now().toString(),
                fertile = fertile,
                clear = clear,
                bloodRing = bloodRing,
                unsure = unsure,
                discrepancyNote = discrepancyNote,
                status = "queued",
                createdAtMillis = System.currentTimeMillis(),
            ),
        )
        enqueueSync()
    }

    suspend fun saveHatch(
        farmId: String,
        batchId: String,
        hatched: Int,
        pippedDead: Int,
        deadInShell: Int,
        unhatched: Int,
        discrepancyNote: String?,
    ) {
        dao.insertHatch(
            HatchEntity(
                clientId = UUID.randomUUID().toString(),
                farmId = farmId,
                batchId = batchId,
                hatchedAt = Instant.now().toString(),
                hatched = hatched,
                pippedDead = pippedDead,
                deadInShell = deadInShell,
                unhatched = unhatched,
                discrepancyNote = discrepancyNote,
                status = "queued",
                createdAtMillis = System.currentTimeMillis(),
            ),
        )
        enqueueSync()
    }

    fun observeCandlingsForBatch(batchId: String) = dao.observeCandlingsForBatch(batchId)
    fun observeHatchForBatch(batchId: String) = dao.observeHatchForBatch(batchId)

    /** Runs immediately if online, otherwise WorkManager waits for connectivity and retries with backoff. */
    private fun enqueueSync() {
        val request = OneTimeWorkRequestBuilder<SyncWorker>()
            .setConstraints(Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build())
            .build()
        workManager.enqueue(request)
    }
}
