package com.eggapp.field.ui.profile

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.Farm
import com.eggapp.field.data.Me
import com.eggapp.field.data.TokenStore
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

data class ProfileUiState(val me: Me? = null, val farm: Farm? = null, val loading: Boolean = true)

class ProfileViewModel(application: Application) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)

    private val _state = MutableStateFlow(ProfileUiState())
    val state: StateFlow<ProfileUiState> = _state

    init {
        viewModelScope.launch {
            val me = runCatching { api.me() }.getOrNull()?.takeIf { it.isSuccessful }?.body()
            val farms = runCatching { api.farms() }.getOrNull()?.takeIf { it.isSuccessful }?.body().orEmpty()
            val farm = farms.firstOrNull { it.id == tokenStore.farmId() } ?: farms.firstOrNull()
            _state.value = ProfileUiState(me = me, farm = farm, loading = false)
        }
    }

    fun logout() = tokenStore.clear()
}
