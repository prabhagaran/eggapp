package com.eggapp.field.ui.batch

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.Batch
import com.eggapp.field.data.BatchCache
import com.eggapp.field.data.BatchDetail
import com.eggapp.field.data.BatchEggSourceInput
import com.eggapp.field.data.Collection
import com.eggapp.field.data.FieldRecordRepository
import com.eggapp.field.data.Incubator
import com.eggapp.field.data.SetBatchRequest
import com.eggapp.field.data.Species
import com.eggapp.field.data.TokenStore
import com.eggapp.field.data.UpdateBatchRequest
import com.eggapp.field.data.local.CandlingEntity
import com.eggapp.field.data.local.HatchEntity
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

// Status/counts here only reflect a candling/hatch record after
// SyncWorker has pushed it — poll like IncubatorsViewModel does rather
// than fetching once at screen-open and going stale.
private const val POLL_INTERVAL_MS = 15_000L

data class BatchDetailUiState(
    val batch: Batch? = null,
    val localCandlings: List<CandlingEntity> = emptyList(),
    val localHatch: HatchEntity? = null,
    val loading: Boolean = true,
    val settingBatch: Boolean = false,
    val saveError: String? = null,
    // Edit/delete — only ever populated for a "planned" or "incubating"
    // batch (loadEditData is only called from that state's UI).
    val editDetail: BatchDetail? = null,
    val editSpecies: List<Species> = emptyList(),
    val editIncubators: List<Incubator> = emptyList(),
    val editCollections: List<Collection> = emptyList(),
    val savingEdit: Boolean = false,
    val deleting: Boolean = false,
    val deleted: Boolean = false,
)

class BatchDetailViewModel(application: Application, private val batchId: String) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)
    private val repository = FieldRecordRepository(application)
    private val batchCache = BatchCache(application)
    private val farmId = tokenStore.farmId()

    private val _state = MutableStateFlow(BatchDetailUiState())
    val state: StateFlow<BatchDetailUiState> = _state

    init {
        // Show cached state immediately (works with zero connectivity),
        // then refresh from the server if reachable.
        _state.value = _state.value.copy(batch = batchCache.get(batchId))
        loadBatch()
        // Offline-first: local records show up immediately regardless of
        // whether they've synced yet — the user shouldn't need
        // connectivity to see what they just recorded in the barn.
        combine(
            repository.observeCandlingsForBatch(batchId),
            repository.observeHatchForBatch(batchId),
        ) { candlings, hatch -> candlings to hatch }
            .onEach { (candlings, hatch) ->
                _state.value = _state.value.copy(localCandlings = candlings, localHatch = hatch)
            }
            .launchIn(viewModelScope)
    }

    private fun loadBatch() {
        val farm = farmId ?: return
        viewModelScope.launch {
            while (isActive) {
                val batches = runCatching { api.batches(farm) }.getOrNull()?.body()
                val fetched = batches?.find { it.id == batchId }
                if (fetched != null) {
                    batchCache.save(fetched)
                    _state.value = _state.value.copy(batch = fetched, loading = false)
                } else {
                    // Fetch failed (offline, etc.) — keep showing the last
                    // known value (cache or a prior poll), just stop the spinner.
                    _state.value = _state.value.copy(loading = false)
                }
                delay(POLL_INTERVAL_MS)
            }
        }
    }

    fun setBatch(setAt: String? = null) {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(settingBatch = true, saveError = null)
            val result = runCatching { api.setBatch(farm, batchId, SetBatchRequest(setAt = setAt)) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                val updated = response.body()
                if (updated != null) batchCache.save(updated)
                _state.value = _state.value.copy(settingBatch = false, batch = updated ?: _state.value.batch)
            } else {
                _state.value = _state.value.copy(
                    settingBatch = false,
                    saveError = result.exceptionOrNull()?.message ?: "Failed to start incubation",
                )
            }
        }
    }

    // Lazily fetches everything the edit form needs — only called when the
    // user actually opens edit, not on every screen load (the list poll in
    // loadBatch() already covers the common read-only view).
    fun loadEditData() {
        val farm = farmId ?: return
        viewModelScope.launch {
            val detail = runCatching { api.batchDetail(farm, batchId) }.getOrNull()?.body()
            val species = runCatching { api.species() }.getOrNull()?.body().orEmpty()
            val incubators = runCatching { api.incubators(farm) }.getOrNull()?.body().orEmpty()
            val collections = runCatching { api.collections(farm) }.getOrNull()?.body().orEmpty()
            _state.value = _state.value.copy(
                editDetail = detail,
                editSpecies = species,
                editIncubators = incubators,
                editCollections = collections,
            )
        }
    }

    fun updateBatch(incubatorId: String, speciesId: String, sources: List<BatchEggSourceInput>, overrideNote: String?, onDone: () -> Unit) {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(savingEdit = true, saveError = null)
            val result = runCatching {
                api.updateBatch(farm, batchId, UpdateBatchRequest(incubatorId, speciesId, sources, overrideNote))
            }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                val updated = response.body()?.batch
                if (updated != null) batchCache.save(updated)
                _state.value = _state.value.copy(savingEdit = false, batch = updated ?: _state.value.batch, editDetail = null)
                onDone()
            } else {
                _state.value = _state.value.copy(savingEdit = false, saveError = result.exceptionOrNull()?.message ?: "Failed to save changes")
            }
        }
    }

    fun deleteBatch() {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(deleting = true, saveError = null)
            val result = runCatching { api.deleteBatch(farm, batchId) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(deleting = false, deleted = true)
            } else {
                _state.value = _state.value.copy(deleting = false, saveError = result.exceptionOrNull()?.message ?: "Failed to delete batch")
            }
        }
    }

    fun cancelEdit() {
        _state.value = _state.value.copy(editDetail = null)
    }

    fun recordCandling(dayNo: Int, fertile: Int, clear: Int, bloodRing: Int, unsure: Int, note: String?) {
        val farm = farmId ?: return
        viewModelScope.launch {
            runCatching {
                repository.saveCandling(farm, batchId, dayNo, fertile, clear, bloodRing, unsure, note)
            }.onFailure { _state.value = _state.value.copy(saveError = it.message) }
        }
    }

    fun recordHatch(hatched: Int, pippedDead: Int, deadInShell: Int, unhatched: Int, note: String?) {
        val farm = farmId ?: return
        viewModelScope.launch {
            runCatching {
                repository.saveHatch(farm, batchId, hatched, pippedDead, deadInShell, unhatched, note)
            }.onFailure { _state.value = _state.value.copy(saveError = it.message) }
        }
    }

    class Factory(private val application: Application, private val batchId: String) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T =
            BatchDetailViewModel(application, batchId) as T
    }
}
