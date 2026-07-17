package com.eggapp.field.ui.batch

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.Batch
import com.eggapp.field.data.TokenStore
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

private val ACTIVE_STATUSES = setOf("planned", "setting", "incubating", "lockdown", "hatching")

data class BatchesUiState(val batches: List<Batch> = emptyList(), val loading: Boolean = true, val error: String? = null)

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
                val result = runCatching { api.batches(farmId) }
                val response = result.getOrNull()
                _state.value = if (response != null && response.isSuccessful) {
                    BatchesUiState(
                        batches = response.body().orEmpty().filter { it.status in ACTIVE_STATUSES },
                        loading = false,
                    )
                } else {
                    BatchesUiState(loading = false, error = "Failed to load batches")
                }
            }
        }
    }
}
