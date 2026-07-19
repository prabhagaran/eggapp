package com.eggapp.field.ui.incubators

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.CLEAR
import com.eggapp.field.data.DeviceConfig
import com.eggapp.field.data.Incubator
import com.eggapp.field.data.SetpointRequest
import com.eggapp.field.data.Species
import com.eggapp.field.data.TokenStore
import com.eggapp.field.data.jsonPatchBody
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

data class SetpointsUiState(
    val incubator: Incubator? = null,
    val config: DeviceConfig? = null,
    val species: List<Species> = emptyList(),
    val loading: Boolean = true,
    val saving: Boolean = false,
    val savingIncubator: Boolean = false,
    val error: String? = null,
)

// US-INC-003, Android side: same push+poll pattern as apps/web's SetpointsCard.
// Fetches by incubatorId (not passed the full object) — same idiom as
// BatchDetailViewModel, so this screen is reachable from anywhere with
// just an id, not tied to nav-arg object passing.
class SetpointsViewModel(application: Application, private val incubatorId: String) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)
    private val farmId = tokenStore.farmId()

    private val _state = MutableStateFlow(SetpointsUiState())
    val state: StateFlow<SetpointsUiState> = _state

    private var pollJob: kotlinx.coroutines.Job? = null

    init {
        val farm = farmId
        if (farm == null) {
            _state.value = SetpointsUiState(loading = false, error = "No farm selected")
        } else {
            viewModelScope.launch {
                val incResponse = runCatching { api.incubator(farm, incubatorId) }.getOrNull()
                _state.value = _state.value.copy(incubator = incResponse?.body(), loading = false)
            }
            viewModelScope.launch {
                val speciesList = runCatching { api.species() }.getOrNull()?.body().orEmpty()
                _state.value = _state.value.copy(species = speciesList)
            }
            reloadConfig()
        }
    }

    fun updateIncubator(name: String, capacity: Int, defaultSpeciesId: String?) {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(savingIncubator = true, error = null)
            val body = jsonPatchBody(
                "name" to name,
                "capacity" to capacity,
                "defaultSpeciesId" to (defaultSpeciesId ?: CLEAR),
            )
            val result = runCatching { api.updateIncubator(farm, incubatorId, body) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(savingIncubator = false, incubator = response.body())
            } else {
                _state.value = _state.value.copy(
                    savingIncubator = false,
                    error = result.exceptionOrNull()?.message ?: "Failed to save incubator",
                )
            }
        }
    }

    fun reloadConfig() {
        val farm = farmId ?: return
        viewModelScope.launch {
            val response = runCatching { api.latestConfig(farm, incubatorId) }.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(config = response.body())
                maybePollForAck()
            }
        }
    }

    // While a command hasn't reached a final state, poll to show sent -> received -> applied live.
    private fun maybePollForAck() {
        val cfg = _state.value.config
        if (cfg == null || cfg.state == "applied" || cfg.state == "unconfirmed") return
        pollJob?.cancel()
        pollJob = viewModelScope.launch {
            while (isActive) {
                delay(3_000)
                val farm = farmId ?: return@launch
                val response = runCatching { api.latestConfig(farm, incubatorId) }.getOrNull()
                if (response != null && response.isSuccessful) {
                    val fresh = response.body()
                    _state.value = _state.value.copy(config = fresh)
                    if (fresh == null || fresh.state == "applied" || fresh.state == "unconfirmed") return@launch
                }
            }
        }
    }

    fun submit(tempSetpoint: Double, tempHysteresis: Double, humSetpoint: Double, humHysteresis: Double) {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(saving = true, error = null)
            val result = runCatching {
                api.pushSetpoints(
                    farm,
                    incubatorId,
                    SetpointRequest(tempSetpoint, tempHysteresis, humSetpoint, humHysteresis),
                )
            }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(saving = false, config = response.body())
                maybePollForAck()
            } else {
                _state.value = _state.value.copy(
                    saving = false,
                    error = result.exceptionOrNull()?.message ?: "Failed to send setpoints",
                )
            }
        }
    }

    class Factory(private val application: Application, private val incubatorId: String) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T =
            SetpointsViewModel(application, incubatorId) as T
    }
}
