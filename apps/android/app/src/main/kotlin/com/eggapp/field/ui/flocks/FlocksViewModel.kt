package com.eggapp.field.ui.flocks

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.Flock
import com.eggapp.field.data.TokenStore
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

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
                val result = runCatching { api.flocks(farmId) }
                val response = result.getOrNull()
                _state.value = if (response != null && response.isSuccessful) {
                    FlocksUiState(flocks = response.body().orEmpty(), loading = false)
                } else {
                    FlocksUiState(loading = false, error = "Failed to load flocks")
                }
            }
        }
    }
}
