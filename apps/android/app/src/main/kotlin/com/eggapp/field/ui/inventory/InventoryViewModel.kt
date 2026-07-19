package com.eggapp.field.ui.inventory

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.CreateInventoryItemRequest
import com.eggapp.field.data.InventoryItem
import com.eggapp.field.data.TokenStore
import com.eggapp.field.data.UpdateInventoryItemRequest
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

// New on Android (Phase 4) — mirrors apps/web/app/inventory/page.tsx:
// list + create + edit + delete. Restock/deduct stay web-only for now
// (not requested; quantity changes need the audit-ledger endpoint).
data class InventoryUiState(
    val items: List<InventoryItem> = emptyList(),
    val loading: Boolean = true,
    val saving: Boolean = false,
    val error: String? = null,
)

class InventoryViewModel(application: Application) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)
    private val farmId = tokenStore.farmId()

    private val _state = MutableStateFlow(InventoryUiState())
    val state: StateFlow<InventoryUiState> = _state

    init {
        val farm = farmId
        if (farm == null) {
            _state.value = InventoryUiState(loading = false, error = "No farm selected")
        } else {
            reload()
        }
    }

    fun reload() {
        val farm = farmId ?: return
        viewModelScope.launch {
            val result = runCatching { api.inventory(farm) }
            val response = result.getOrNull()
            _state.value = if (response != null && response.isSuccessful) {
                _state.value.copy(items = response.body().orEmpty(), loading = false, error = null)
            } else {
                _state.value.copy(loading = false, error = result.exceptionOrNull()?.message ?: "Failed to load inventory")
            }
        }
    }

    fun createItem(body: CreateInventoryItemRequest, onDone: () -> Unit) {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(saving = true, error = null)
            val result = runCatching { api.createInventoryItem(farm, body) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(saving = false)
                reload()
                onDone()
            } else {
                _state.value = _state.value.copy(saving = false, error = result.exceptionOrNull()?.message ?: "Failed to create item")
            }
        }
    }

    fun updateItem(id: String, body: UpdateInventoryItemRequest, onDone: () -> Unit) {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(saving = true, error = null)
            val result = runCatching { api.updateInventoryItem(farm, id, body) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(saving = false)
                reload()
                onDone()
            } else {
                _state.value = _state.value.copy(saving = false, error = result.exceptionOrNull()?.message ?: "Failed to save item")
            }
        }
    }

    fun deleteItem(id: String) {
        val farm = farmId ?: return
        viewModelScope.launch {
            val result = runCatching { api.deleteInventoryItem(farm, id) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                reload()
            } else {
                _state.value = _state.value.copy(error = result.exceptionOrNull()?.message ?: "Failed to delete item")
            }
        }
    }
}
