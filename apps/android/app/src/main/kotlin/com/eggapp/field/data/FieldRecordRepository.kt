package com.eggapp.field.data

import android.content.Context
import androidx.work.Constraints
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.WorkManager
import com.eggapp.field.data.local.AppDatabase
import com.eggapp.field.data.local.CandlingEntity
import com.eggapp.field.data.local.CollectionEntity
import com.eggapp.field.data.local.FeedLogEntity
import com.eggapp.field.data.local.HatchEntity
import com.eggapp.field.data.local.MortalityEntity
import com.eggapp.field.data.local.VaccinationEntity
import com.eggapp.field.data.local.WaterLogEntity
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

    suspend fun saveCollection(
        farmId: String,
        collectedOn: String,
        count: Int,
        avgWeightGrams: Double?,
        sourceNote: String?,
    ) {
        dao.insertCollection(
            CollectionEntity(
                clientId = UUID.randomUUID().toString(),
                farmId = farmId,
                collectedOn = collectedOn,
                count = count,
                avgWeightGrams = avgWeightGrams,
                sourceNote = sourceNote,
                status = "queued",
                createdAtMillis = System.currentTimeMillis(),
            ),
        )
        enqueueSync()
    }

    suspend fun saveMortality(farmId: String, flockId: String, date: String, count: Int, cause: String, notes: String?) {
        dao.insertMortality(
            MortalityEntity(
                clientId = UUID.randomUUID().toString(),
                farmId = farmId,
                flockId = flockId,
                date = date,
                count = count,
                cause = cause,
                notes = notes,
                status = "queued",
                createdAtMillis = System.currentTimeMillis(),
            ),
        )
        enqueueSync()
    }

    suspend fun saveVaccination(
        farmId: String,
        flockId: String,
        templateItemId: String?,
        date: String,
        vaccine: String,
        disease: String,
        route: String,
        count: Int,
        administeredBy: String,
        manufacturer: String?,
        lotNumber: String?,
        dose: String?,
        reactions: String?,
    ) {
        dao.insertVaccination(
            VaccinationEntity(
                clientId = UUID.randomUUID().toString(),
                farmId = farmId,
                flockId = flockId,
                templateItemId = templateItemId,
                date = date,
                vaccine = vaccine,
                disease = disease,
                route = route,
                count = count,
                administeredBy = administeredBy,
                manufacturer = manufacturer,
                lotNumber = lotNumber,
                dose = dose,
                reactions = reactions,
                status = "queued",
                createdAtMillis = System.currentTimeMillis(),
            ),
        )
        enqueueSync()
    }

    suspend fun saveFeedLog(farmId: String, flockId: String, feedType: String, quantityKg: Double) {
        dao.insertFeedLog(
            FeedLogEntity(
                clientId = UUID.randomUUID().toString(),
                farmId = farmId,
                flockId = flockId,
                loggedAt = Instant.now().toString(),
                feedType = feedType,
                quantityKg = quantityKg,
                status = "queued",
                createdAtMillis = System.currentTimeMillis(),
            ),
        )
        enqueueSync()
    }

    suspend fun saveWaterLog(farmId: String, flockId: String, quantityLiters: Double) {
        dao.insertWaterLog(
            WaterLogEntity(
                clientId = UUID.randomUUID().toString(),
                farmId = farmId,
                flockId = flockId,
                loggedAt = Instant.now().toString(),
                quantityLiters = quantityLiters,
                status = "queued",
                createdAtMillis = System.currentTimeMillis(),
            ),
        )
        enqueueSync()
    }

    fun observeCandlingsForBatch(batchId: String) = dao.observeCandlingsForBatch(batchId)
    fun observeHatchForBatch(batchId: String) = dao.observeHatchForBatch(batchId)
    fun observeCollectionsForFarm(farmId: String) = dao.observeCollectionsForFarm(farmId)
    fun observeMortalityForFlock(flockId: String) = dao.observeMortalityForFlock(flockId)
    fun observeVaccinationForFlock(flockId: String) = dao.observeVaccinationForFlock(flockId)
    fun observeFeedLogsForFlock(flockId: String) = dao.observeFeedLogsForFlock(flockId)
    fun observeWaterLogsForFlock(flockId: String) = dao.observeWaterLogsForFlock(flockId)

    /** Runs immediately if online, otherwise WorkManager waits for connectivity and retries with backoff. */
    private fun enqueueSync() {
        val request = OneTimeWorkRequestBuilder<SyncWorker>()
            .setConstraints(Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build())
            .build()
        workManager.enqueue(request)
    }
}
