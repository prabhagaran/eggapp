package com.eggapp.field.ui.login

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.LoginRequest
import com.eggapp.field.data.PushTokenRequest
import com.eggapp.field.data.TokenStore
import com.google.firebase.messaging.FirebaseMessaging
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.tasks.await

data class LoginUiState(
    val busy: Boolean = false,
    val error: String? = null,
    val loggedIn: Boolean = false,
)

class LoginViewModel(application: Application) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authFree()

    private val _state = MutableStateFlow(LoginUiState())
    val state: StateFlow<LoginUiState> = _state

    fun alreadyLoggedIn(): Boolean = tokenStore.accessToken() != null

    fun login(email: String, password: String) {
        _state.value = LoginUiState(busy = true)
        viewModelScope.launch {
            val result = runCatching { api.login(LoginRequest(email, password)) }
            val response = result.getOrNull()
            if (response == null || !response.isSuccessful || response.body() == null) {
                _state.value = LoginUiState(
                    error = result.exceptionOrNull()?.message
                        ?: "Login failed (${response?.code() ?: "no response"})",
                )
                return@launch
            }
            val tokens = response.body()!!
            tokenStore.saveTokens(tokens.accessToken, tokens.refreshToken)

            // Resolve the farm to operate against — first membership,
            // same default as the web client (apps/web/lib/useAuthedFarm.ts).
            val authed = ApiClient.authenticated(tokenStore)
            val farms = runCatching { authed.farms() }.getOrNull()?.body()
            if (farms.isNullOrEmpty()) {
                _state.value = LoginUiState(error = "Logged in, but no farm found for this account")
                return@launch
            }
            tokenStore.saveFarmId(farms[0].id)

            // Register this device for push (US-NOT-002). Best-effort —
            // a failure here shouldn't block login; onNewToken will
            // retry registration on the next token rotation regardless.
            runCatching {
                val fcmToken = FirebaseMessaging.getInstance().token.await()
                authed.registerPushToken(PushTokenRequest(fcmToken))
            }

            _state.value = LoginUiState(loggedIn = true)
        }
    }
}
