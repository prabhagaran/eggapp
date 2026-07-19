package com.eggapp.field.ui.flocks

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.CLEAR
import com.eggapp.field.data.ComplianceItem
import com.eggapp.field.data.FieldRecordRepository
import com.eggapp.field.data.FlockDetail
import com.eggapp.field.data.Species
import com.eggapp.field.data.TokenStore
import com.eggapp.field.data.jsonPatchBody
import com.eggapp.field.data.local.FeedLogEntity
import com.eggapp.field.data.local.MortalityEntity
import com.eggapp.field.data.local.VaccinationEntity
import com.eggapp.field.data.local.WaterLogEntity
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

// Kotlin has no built-in 4-tuple (only Pair/Triple) — just enough to
// destructure combine()'s output below.
private data class Quad<A, B, C, D>(val a: A, val b: B, val c: C, val d: D)

// The server-computed currentCount/compliance shown here only reflect a
// record after SyncWorker has pushed it (and after anyone else's edits,
// e.g. from the web dashboard) — poll like IncubatorsViewModel does
// rather than fetching once at screen-open and going stale.
private const val POLL_INTERVAL_MS = 15_000L

data class FlockDetailUiState(
    val flock: FlockDetail? = null,
    val compliance: List<ComplianceItem> = emptyList(),
    val localMortality: List<MortalityEntity> = emptyList(),
    val localVaccination: List<VaccinationEntity> = emptyList(),
    val localFeed: List<FeedLogEntity> = emptyList(),
    val localWater: List<WaterLogEntity> = emptyList(),
    val species: List<Species> = emptyList(),
    val loading: Boolean = true,
    val saving: Boolean = false,
    val deleting: Boolean = false,
    val deleted: Boolean = false,
    val saveError: String? = null,
)

class FlockDetailViewModel(application: Application, private val flockId: String) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)
    private val repository = FieldRecordRepository(application)
    private val farmId = tokenStore.farmId()

    private val _state = MutableStateFlow(FlockDetailUiState())
    val state: StateFlow<FlockDetailUiState> = _state

    init {
        loadFlock()
        viewModelScope.launch {
            val speciesList = runCatching { api.species() }.getOrNull()?.body().orEmpty()
            _state.value = _state.value.copy(species = speciesList)
        }
        // Offline-first: locally-queued records show up immediately
        // regardless of sync state — same pattern as BatchDetailViewModel.
        combine(
            repository.observeMortalityForFlock(flockId),
            repository.observeVaccinationForFlock(flockId),
            repository.observeFeedLogsForFlock(flockId),
            repository.observeWaterLogsForFlock(flockId),
        ) { mortality, vaccination, feed, water -> Quad(mortality, vaccination, feed, water) }
            .onEach { (mortality, vaccination, feed, water) ->
                _state.value = _state.value.copy(localMortality = mortality, localVaccination = vaccination, localFeed = feed, localWater = water)
            }
            .launchIn(viewModelScope)
    }

    private fun loadFlock() {
        val farm = farmId ?: return
        viewModelScope.launch {
            while (isActive) {
                val flockResult = runCatching { api.flock(farm, flockId) }.getOrNull()?.body()
                val complianceResult = runCatching { api.vaccinationCompliance(farm, flockId) }.getOrNull()?.body()
                _state.value = _state.value.copy(
                    flock = flockResult ?: _state.value.flock,
                    compliance = complianceResult ?: _state.value.compliance,
                    loading = false,
                )
                delay(POLL_INTERVAL_MS)
            }
        }
    }

    fun updateFlock(name: String, speciesId: String, purpose: String, acquisitionNote: String?, stageOverride: String?) {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(saving = true, saveError = null)
            // acquisitionNote is optional-but-not-nullable server-side (unlike
            // stageOverride) — an empty note is omitted, not cleared, same
            // limitation apps/web's edit form has.
            val body = jsonPatchBody(
                "name" to name,
                "speciesId" to speciesId,
                "purpose" to purpose,
                "acquisitionNote" to acquisitionNote,
                "stageOverride" to (stageOverride ?: CLEAR),
            )
            val result = runCatching { api.updateFlock(farm, flockId, body) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(saving = false, flock = response.body())
            } else {
                _state.value = _state.value.copy(
                    saving = false,
                    saveError = result.exceptionOrNull()?.message ?: "Failed to save flock",
                )
            }
        }
    }

    fun deleteFlock() {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(deleting = true, saveError = null)
            val result = runCatching { api.deleteFlock(farm, flockId) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(deleting = false, deleted = true)
            } else {
                _state.value = _state.value.copy(
                    deleting = false,
                    saveError = result.exceptionOrNull()?.message ?: "Failed to delete flock",
                )
            }
        }
    }

    fun recordMortality(date: String, count: Int, cause: String, notes: String?) {
        val farm = farmId ?: return
        viewModelScope.launch {
            runCatching { repository.saveMortality(farm, flockId, date, count, cause, notes) }
                .onFailure { _state.value = _state.value.copy(saveError = it.message) }
        }
    }

    fun recordVaccination(
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
        val farm = farmId ?: return
        viewModelScope.launch {
            runCatching {
                repository.saveVaccination(
                    farm, flockId, templateItemId, date, vaccine, disease, route, count,
                    administeredBy, manufacturer, lotNumber, dose, reactions,
                )
            }.onFailure { _state.value = _state.value.copy(saveError = it.message) }
        }
    }

    fun recordFeed(feedType: String, quantityKg: Double) {
        val farm = farmId ?: return
        viewModelScope.launch {
            runCatching { repository.saveFeedLog(farm, flockId, feedType, quantityKg) }
                .onFailure { _state.value = _state.value.copy(saveError = it.message) }
        }
    }

    fun recordWater(quantityLiters: Double) {
        val farm = farmId ?: return
        viewModelScope.launch {
            runCatching { repository.saveWaterLog(farm, flockId, quantityLiters) }
                .onFailure { _state.value = _state.value.copy(saveError = it.message) }
        }
    }

    class Factory(private val application: Application, private val flockId: String) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T =
            FlockDetailViewModel(application, flockId) as T
    }
}
