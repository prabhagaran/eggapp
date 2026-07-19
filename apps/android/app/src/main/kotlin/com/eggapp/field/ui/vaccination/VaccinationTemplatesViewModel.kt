package com.eggapp.field.ui.vaccination

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.Species
import com.eggapp.field.data.TokenStore
import com.eggapp.field.data.VaccinationTemplateItem
import com.eggapp.field.data.VaccinationTemplateItemRequest
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

// New on Android (Phase 4) — previously web-only per CLAUDE.md's surface
// split; the product decision now is full parity, so this mirrors
// apps/web/app/vaccination-templates/page.tsx: list + create/edit + delete.
data class VaccinationTemplatesUiState(
    val templates: List<VaccinationTemplateItem> = emptyList(),
    val species: List<Species> = emptyList(),
    val loading: Boolean = true,
    val saving: Boolean = false,
    val error: String? = null,
)

class VaccinationTemplatesViewModel(application: Application) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)
    private val farmId = tokenStore.farmId()

    private val _state = MutableStateFlow(VaccinationTemplatesUiState())
    val state: StateFlow<VaccinationTemplatesUiState> = _state

    init {
        val farm = farmId
        if (farm == null) {
            _state.value = VaccinationTemplatesUiState(loading = false, error = "No farm selected")
        } else {
            reload()
            viewModelScope.launch {
                val speciesList = runCatching { api.species() }.getOrNull()?.body().orEmpty()
                _state.value = _state.value.copy(species = speciesList)
            }
        }
    }

    fun reload() {
        val farm = farmId ?: return
        viewModelScope.launch {
            val result = runCatching { api.vaccinationTemplates(farm) }
            val response = result.getOrNull()
            _state.value = if (response != null && response.isSuccessful) {
                _state.value.copy(templates = response.body().orEmpty(), loading = false, error = null)
            } else {
                _state.value.copy(loading = false, error = result.exceptionOrNull()?.message ?: "Failed to load templates")
            }
        }
    }

    fun createTemplate(body: VaccinationTemplateItemRequest, onDone: () -> Unit) {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(saving = true, error = null)
            val result = runCatching { api.createVaccinationTemplate(farm, body) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(saving = false)
                reload()
                onDone()
            } else {
                _state.value = _state.value.copy(saving = false, error = result.exceptionOrNull()?.message ?: "Failed to create")
            }
        }
    }

    fun updateTemplate(id: String, body: VaccinationTemplateItemRequest, onDone: () -> Unit) {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(saving = true, error = null)
            val result = runCatching { api.updateVaccinationTemplate(farm, id, body) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(saving = false)
                reload()
                onDone()
            } else {
                _state.value = _state.value.copy(saving = false, error = result.exceptionOrNull()?.message ?: "Failed to save")
            }
        }
    }

    fun deleteTemplate(id: String) {
        val farm = farmId ?: return
        viewModelScope.launch {
            val result = runCatching { api.deleteVaccinationTemplate(farm, id) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                reload()
            } else {
                _state.value = _state.value.copy(error = result.exceptionOrNull()?.message ?: "Failed to delete")
            }
        }
    }
}
