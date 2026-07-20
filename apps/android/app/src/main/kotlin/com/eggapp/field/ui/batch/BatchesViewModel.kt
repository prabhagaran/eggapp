package com.eggapp.field.ui.batch

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.Batch
import com.eggapp.field.data.BatchEggSourceInput
import com.eggapp.field.data.Collection
import com.eggapp.field.data.CreateBatchRequest
import com.eggapp.field.data.Incubator
import com.eggapp.field.data.Species
import com.eggapp.field.data.TokenStore
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

private val ACTIVE_STATUSES = setOf("planned", "setting", "incubating", "lockdown", "hatching")

// Status/viable-count here can change from the web dashboard while this
// screen is open — poll like IncubatorsViewModel does, rather than
// fetching once and going stale.
private const val POLL_INTERVAL_MS = 15_000L

data class BatchesUiState(
    val batches: List<Batch> = emptyList(),
    val species: List<Species> = emptyList(),
    val incubators: List<Incubator> = emptyList(),
    val collections: List<Collection> = emptyList(),
    val loading: Boolean = true,
    val saving: Boolean = false,
    val error: String? = null,
)

class BatchesViewModel(application: Application) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)

    private val _state = MutableStateFlow(BatchesUiState())
    val state: StateFlow<BatchesUiState> = _state

    init {
        val farmId = tokenStore.farmId()
        if (farmId == null) {
            _state.value = BatchesUiState(loading = false, error = "No farm selected")
        } else {
            viewModelScope.launch {
                val speciesList = runCatching { api.species() }.getOrNull()?.body().orEmpty()
                _state.value = _state.value.copy(species = speciesList)
            }
            viewModelScope.launch {
                val incubators = runCatching { api.incubators(farmId) }.getOrNull()?.body().orEmpty()
                _state.value = _state.value.copy(incubators = incubators)
            }
            reloadCollections(farmId)
            viewModelScope.launch {
                while (isActive) {
                    val result = runCatching { api.batches(farmId) }
                    val response = result.getOrNull()
                    _state.value = if (response != null && response.isSuccessful) {
                        _state.value.copy(
                            batches = response.body().orEmpty().filter { it.status in ACTIVE_STATUSES },
                            loading = false,
                            error = null,
                        )
                    } else {
                        _state.value.copy(loading = false, error = "Failed to load batches")
                    }
                    delay(POLL_INTERVAL_MS)
                }
            }
        }
    }

    private fun reloadCollections(farmId: String) {
        viewModelScope.launch {
            val collections = runCatching { api.collections(farmId) }.getOrNull()?.body().orEmpty()
            _state.value = _state.value.copy(collections = collections.filter { it.availableCount > 0 })
        }
    }

    fun createBatch(incubatorId: String, speciesId: String, sources: List<BatchEggSourceInput>, overrideNote: String?, onDone: () -> Unit) {
        val farmId = tokenStore.farmId() ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(saving = true, error = null)
            val result = runCatching { api.createBatch(farmId, CreateBatchRequest(incubatorId, speciesId, sources, overrideNote)) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                val body = response.body()
                if (body != null && body.warnings.isNotEmpty()) {
                    _state.value = _state.value.copy(error = "Created with warnings: ${body.warnings.joinToString("; ")}")
                }
                _state.value = _state.value.copy(saving = false)
                reloadCollections(farmId)
                onDone()
            } else {
                _state.value = _state.value.copy(saving = false, error = result.exceptionOrNull()?.message ?: "Failed to create batch")
            }
        }
    }
}
