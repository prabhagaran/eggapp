package com.eggapp.field.ui.collections

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.Collection
import com.eggapp.field.data.DiscardRequest
import com.eggapp.field.data.FieldRecordRepository
import com.eggapp.field.data.TokenStore
import com.eggapp.field.data.local.CollectionEntity
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.launch

data class CollectionsUiState(
    val serverCollections: List<Collection> = emptyList(),
    val localQueued: List<CollectionEntity> = emptyList(),
    val loading: Boolean = true,
    val error: String? = null,
)

class CollectionsViewModel(application: Application) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)
    private val repository = FieldRecordRepository(application)
    private val farmId = tokenStore.farmId()

    private val _state = MutableStateFlow(CollectionsUiState())
    val state: StateFlow<CollectionsUiState> = _state

    init {
        val farm = farmId
        if (farm == null) {
            _state.value = CollectionsUiState(loading = false, error = "No farm selected")
        } else {
            reload()
            // Offline-first: queued/unsynced collections show up immediately
            // regardless of connectivity, same pattern as candling/hatch.
            repository.observeCollectionsForFarm(farm)
                .onEach { rows ->
                    _state.value = _state.value.copy(localQueued = rows.filter { it.status != "synced" })
                }
                .launchIn(viewModelScope)
        }
    }

    fun reload() {
        val farm = farmId ?: return
        viewModelScope.launch {
            val result = runCatching { api.collections(farm) }
            val response = result.getOrNull()
            _state.value = if (response != null && response.isSuccessful) {
                _state.value.copy(serverCollections = response.body().orEmpty(), loading = false, error = null)
            } else {
                // Fetch failed (offline, etc.) — keep whatever was already loaded, just stop the spinner.
                _state.value.copy(loading = false)
            }
        }
    }

    fun recordCollection(collectedOn: String, count: Int, avgWeightGrams: Double?, sourceNote: String?) {
        val farm = farmId ?: return
        viewModelScope.launch {
            runCatching {
                repository.saveCollection(farm, collectedOn, count, avgWeightGrams, sourceNote)
            }.onFailure { _state.value = _state.value.copy(error = it.message) }
        }
    }

    // Online-only: the discard endpoint has no clientId idempotency
    // (unlike create), so it isn't safe to queue-and-retry — a retried
    // discard would double-deduct. Discarding normally happens while
    // reviewing a freshly-fetched list anyway, so this is not a real gap.
    fun discard(collectionId: String, count: Int, reason: String) {
        val farm = farmId ?: return
        viewModelScope.launch {
            val result = runCatching { api.discardEggs(farm, collectionId, DiscardRequest(count, reason)) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                reload()
            } else {
                _state.value = _state.value.copy(error = "Discard failed — check connectivity and try again")
            }
        }
    }
}
