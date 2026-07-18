package com.eggapp.field.ui.incubators

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.Incubator
import com.eggapp.field.data.TokenStore
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

// Telemetry lands every ~60s (see docs/iot/telemetry-contract.md); poll a
// bit faster so a fresh reading shows up without a manual pull-to-refresh —
// same reasoning as apps/web's 15s polling.
private const val POLL_INTERVAL_MS = 15_000L

data class IncubatorsUiState(
    val incubators: List<Incubator> = emptyList(),
    val farmName: String? = null,
    val loading: Boolean = true,
    val error: String? = null,
)

class IncubatorsViewModel(application: Application) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)

    private val _state = MutableStateFlow(IncubatorsUiState())
    val state: StateFlow<IncubatorsUiState> = _state

    init {
        viewModelScope.launch {
            val farmId = tokenStore.farmId()
            if (farmId == null) {
                _state.value = IncubatorsUiState(loading = false, error = "No farm selected")
                return@launch
            }
            val farmName = runCatching { api.farms() }.getOrNull()
                ?.takeIf { it.isSuccessful }?.body()
                ?.firstOrNull { it.id == farmId }?.name
            _state.value = _state.value.copy(farmName = farmName)
            while (isActive) {
                val result = runCatching { api.incubators(farmId) }
                val response = result.getOrNull()
                _state.value = if (response != null && response.isSuccessful) {
                    _state.value.copy(incubators = response.body().orEmpty(), loading = false, error = null)
                } else {
                    _state.value.copy(
                        loading = false,
                        error = result.exceptionOrNull()?.message ?: "Failed to load incubators",
                    )
                }
                delay(POLL_INTERVAL_MS)
            }
        }
    }

    fun logout() = tokenStore.clear()
}
