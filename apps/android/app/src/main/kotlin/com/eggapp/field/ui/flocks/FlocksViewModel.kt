package com.eggapp.field.ui.flocks

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.Flock
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

data class FlocksUiState(val flocks: List<Flock> = emptyList(), val loading: Boolean = true, val error: String? = null)

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
                while (isActive) {
                    val result = runCatching { api.flocks(farmId) }
                    val response = result.getOrNull()
                    _state.value = if (response != null && response.isSuccessful) {
                        FlocksUiState(flocks = response.body().orEmpty(), loading = false)
                    } else {
                        _state.value.copy(loading = false, error = "Failed to load flocks")
                    }
                    delay(POLL_INTERVAL_MS)
                }
            }
        }
    }
}
