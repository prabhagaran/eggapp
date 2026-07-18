package com.eggapp.field.sync

import android.content.Context
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.CandlingRequest
import com.eggapp.field.data.CollectionRequest
import com.eggapp.field.data.FeedLogRequest
import com.eggapp.field.data.HatchRequest
import com.eggapp.field.data.MortalityRequest
import com.eggapp.field.data.TokenStore
import com.eggapp.field.data.VaccinationRequest
import com.eggapp.field.data.WaterLogRequest
import com.eggapp.field.data.local.AppDatabase
import retrofit2.Response

/**
 * Pushes Room-queued candling/hatch records to the server (BR-010).
 * Runs whenever WorkManager decides connectivity is available (see the
 * NetworkType.CONNECTED constraint in FieldRecordRepository) and retries
 * automatically on network-level failure. A well-formed rejection (e.g.
 * missing discrepancyNote, invalid batch transition) is NOT retried
 * forever — that needs a human to fix, so it's marked "conflict" instead
 * of endlessly re-attempting the same doomed request.
 */
class SyncWorker(context: Context, params: WorkerParameters) : CoroutineWorker(context, params) {

    override suspend fun doWork(): Result {
        val dao = AppDatabase.get(applicationContext).fieldRecordDao()
        val api = ApiClient.authenticated(TokenStore(applicationContext))

        var hadNetworkFailure = false

        for (record in dao.pendingCandlings()) {
            val outcome = runCatching {
                api.recordCandling(
                    record.farmId,
                    record.batchId,
                    CandlingRequest(
                        dayNo = record.dayNo,
                        candledAt = record.candledAt,
                        fertile = record.fertile,
                        clear = record.clear,
                        bloodRing = record.bloodRing,
                        unsure = record.unsure,
                        discrepancyNote = record.discrepancyNote,
                        clientId = record.clientId,
                    ),
                )
            }
            when (val classification = classify(outcome)) {
                is SyncOutcome.Success -> dao.updateCandlingStatus(record.clientId, "synced", null)
                is SyncOutcome.Rejected -> dao.updateCandlingStatus(record.clientId, "conflict", classification.message)
                is SyncOutcome.NetworkFailure -> hadNetworkFailure = true
            }
        }

        for (record in dao.pendingHatches()) {
            val outcome = runCatching {
                api.recordHatch(
                    record.farmId,
                    record.batchId,
                    HatchRequest(
                        hatchedAt = record.hatchedAt,
                        hatched = record.hatched,
                        pippedDead = record.pippedDead,
                        deadInShell = record.deadInShell,
                        unhatched = record.unhatched,
                        discrepancyNote = record.discrepancyNote,
                        clientId = record.clientId,
                    ),
                )
            }
            when (val classification = classify(outcome)) {
                is SyncOutcome.Success -> dao.updateHatchStatus(record.clientId, "synced", null)
                is SyncOutcome.Rejected -> dao.updateHatchStatus(record.clientId, "conflict", classification.message)
                is SyncOutcome.NetworkFailure -> hadNetworkFailure = true
            }
        }

        for (record in dao.pendingCollections()) {
            val outcome = runCatching {
                api.recordCollection(
                    record.farmId,
                    CollectionRequest(
                        collectedOn = record.collectedOn,
                        count = record.count,
                        avgWeightGrams = record.avgWeightGrams,
                        sourceNote = record.sourceNote,
                        clientId = record.clientId,
                    ),
                )
            }
            when (val classification = classify(outcome)) {
                is SyncOutcome.Success -> dao.updateCollectionStatus(record.clientId, "synced", null)
                is SyncOutcome.Rejected -> dao.updateCollectionStatus(record.clientId, "conflict", classification.message)
                is SyncOutcome.NetworkFailure -> hadNetworkFailure = true
            }
        }

        for (record in dao.pendingMortality()) {
            val outcome = runCatching {
                api.recordMortality(
                    record.farmId,
                    record.flockId,
                    MortalityRequest(
                        date = record.date,
                        count = record.count,
                        cause = record.cause,
                        notes = record.notes,
                        clientId = record.clientId,
                    ),
                )
            }
            when (val classification = classify(outcome)) {
                is SyncOutcome.Success -> dao.updateMortalityStatus(record.clientId, "synced", null)
                is SyncOutcome.Rejected -> dao.updateMortalityStatus(record.clientId, "conflict", classification.message)
                is SyncOutcome.NetworkFailure -> hadNetworkFailure = true
            }
        }

        for (record in dao.pendingVaccination()) {
            val outcome = runCatching {
                api.recordVaccination(
                    record.farmId,
                    record.flockId,
                    VaccinationRequest(
                        templateItemId = record.templateItemId,
                        date = record.date,
                        vaccine = record.vaccine,
                        disease = record.disease,
                        route = record.route,
                        count = record.count,
                        administeredBy = record.administeredBy,
                        manufacturer = record.manufacturer,
                        lotNumber = record.lotNumber,
                        dose = record.dose,
                        reactions = record.reactions,
                        clientId = record.clientId,
                    ),
                )
            }
            when (val classification = classify(outcome)) {
                is SyncOutcome.Success -> dao.updateVaccinationStatus(record.clientId, "synced", null)
                is SyncOutcome.Rejected -> dao.updateVaccinationStatus(record.clientId, "conflict", classification.message)
                is SyncOutcome.NetworkFailure -> hadNetworkFailure = true
            }
        }

        for (record in dao.pendingFeedLogs()) {
            val outcome = runCatching {
                api.recordFeedLog(
                    record.farmId,
                    record.flockId,
                    FeedLogRequest(
                        loggedAt = record.loggedAt,
                        feedType = record.feedType,
                        quantityKg = record.quantityKg,
                        clientId = record.clientId,
                    ),
                )
            }
            when (val classification = classify(outcome)) {
                is SyncOutcome.Success -> dao.updateFeedLogStatus(record.clientId, "synced", null)
                is SyncOutcome.Rejected -> dao.updateFeedLogStatus(record.clientId, "conflict", classification.message)
                is SyncOutcome.NetworkFailure -> hadNetworkFailure = true
            }
        }

        for (record in dao.pendingWaterLogs()) {
            val outcome = runCatching {
                api.recordWaterLog(
                    record.farmId,
                    record.flockId,
                    WaterLogRequest(
                        loggedAt = record.loggedAt,
                        quantityLiters = record.quantityLiters,
                        clientId = record.clientId,
                    ),
                )
            }
            when (val classification = classify(outcome)) {
                is SyncOutcome.Success -> dao.updateWaterLogStatus(record.clientId, "synced", null)
                is SyncOutcome.Rejected -> dao.updateWaterLogStatus(record.clientId, "conflict", classification.message)
                is SyncOutcome.NetworkFailure -> hadNetworkFailure = true
            }
        }

        // Success (200 replay) and 201 (created) both count as synced —
        // BR-010 idempotency means either is a correct outcome.
        return if (hadNetworkFailure) Result.retry() else Result.success()
    }

    private fun classify(result: kotlin.Result<Response<*>>): SyncOutcome {
        val response = result.getOrNull()
        if (response == null) return SyncOutcome.NetworkFailure // exception: no connectivity, timeout, etc.
        return when {
            response.isSuccessful -> SyncOutcome.Success
            response.code() in 500..599 -> SyncOutcome.NetworkFailure // server-side, worth retrying
            else -> SyncOutcome.Rejected(response.errorBody()?.string() ?: "HTTP ${response.code()}")
        }
    }
}

private sealed interface SyncOutcome {
    data object Success : SyncOutcome
    data class Rejected(val message: String) : SyncOutcome
    data object NetworkFailure : SyncOutcome
}
