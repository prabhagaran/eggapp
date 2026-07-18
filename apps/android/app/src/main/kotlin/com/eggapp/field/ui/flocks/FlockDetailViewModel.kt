package com.eggapp.field.ui.flocks

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.ComplianceItem
import com.eggapp.field.data.FieldRecordRepository
import com.eggapp.field.data.FlockDetail
import com.eggapp.field.data.TokenStore
import com.eggapp.field.data.local.FeedLogEntity
import com.eggapp.field.data.local.MortalityEntity
import com.eggapp.field.data.local.VaccinationEntity
import com.eggapp.field.data.local.WaterLogEntity
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.launch

// Kotlin has no built-in 4-tuple (only Pair/Triple) — just enough to
// destructure combine()'s output below.
private data class Quad<A, B, C, D>(val a: A, val b: B, val c: C, val d: D)

data class FlockDetailUiState(
    val flock: FlockDetail? = null,
    val compliance: List<ComplianceItem> = emptyList(),
    val localMortality: List<MortalityEntity> = emptyList(),
    val localVaccination: List<VaccinationEntity> = emptyList(),
    val localFeed: List<FeedLogEntity> = emptyList(),
    val localWater: List<WaterLogEntity> = emptyList(),
    val loading: Boolean = true,
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
            val flockResult = runCatching { api.flock(farm, flockId) }.getOrNull()?.body()
            val complianceResult = runCatching { api.vaccinationCompliance(farm, flockId) }.getOrNull()?.body()
            _state.value = _state.value.copy(
                flock = flockResult,
                compliance = complianceResult.orEmpty(),
                loading = false,
            )
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
