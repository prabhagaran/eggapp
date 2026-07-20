package com.eggapp.field.ui.flocks

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.CreateFlockRequest
import com.eggapp.field.data.Flock
import com.eggapp.field.data.Species
import com.eggapp.field.data.TokenStore
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

// Counts/stages here can change from the web dashboard (or another
// device) while this screen is open — poll like IncubatorsViewModel
// does, rather than fetching once and going stale.
private const val POLL_INTERVAL_MS = 15_000L

// A hatch that could become a flock via the "from hatch" origin —
// matches apps/web's flocks/page.tsx completedHatches derivation.
data class CompletedHatchOption(val hatchEventId: String, val label: String)

data class FlocksUiState(
    val flocks: List<Flock> = emptyList(),
    val species: List<Species> = emptyList(),
    val completedHatches: List<CompletedHatchOption> = emptyList(),
    val loading: Boolean = true,
    val saving: Boolean = false,
    val error: String? = null,
)

class FlocksViewModel(application: Application) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)

    private val _state = MutableStateFlow(FlocksUiState())
    val state: StateFlow<FlocksUiState> = _state

    init {
        val farmId = tokenStore.farmId()
        if (farmId == null) {
            _state.value = FlocksUiState(loading = false, error = "No farm selected")
        } else {
            viewModelScope.launch {
                val speciesList = runCatching { api.species() }.getOrNull()?.body().orEmpty()
                _state.value = _state.value.copy(species = speciesList)
            }
            viewModelScope.launch {
                val completed = runCatching { api.batches(farmId, status = "completed") }.getOrNull()?.body().orEmpty()
                _state.value = _state.value.copy(
                    completedHatches = completed
                        .mapNotNull { b -> b.hatch?.takeIf { it.flock == null }?.let { it.id to b } }
                        .map { (hatchId, b) -> CompletedHatchOption(hatchId, "${b.species?.name ?: b.speciesId} batch") },
                )
            }
            viewModelScope.launch {
                while (isActive) {
                    val result = runCatching { api.flocks(farmId) }
                    val response = result.getOrNull()
                    _state.value = if (response != null && response.isSuccessful) {
                        _state.value.copy(flocks = response.body().orEmpty(), loading = false, error = null)
                    } else {
                        _state.value.copy(loading = false, error = "Failed to load flocks")
                    }
                    delay(POLL_INTERVAL_MS)
                }
            }
        }
    }

    fun createFlock(body: CreateFlockRequest, onDone: () -> Unit) {
        val farmId = tokenStore.farmId() ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(saving = true, error = null)
            val result = runCatching { api.createFlock(farmId, body) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                val created = response.body()
                _state.value = _state.value.copy(
                    saving = false,
                    flocks = if (created != null) _state.value.flocks + created else _state.value.flocks,
                )
                onDone()
            } else {
                _state.value = _state.value.copy(saving = false, error = result.exceptionOrNull()?.message ?: "Failed to create flock")
            }
        }
    }
}
