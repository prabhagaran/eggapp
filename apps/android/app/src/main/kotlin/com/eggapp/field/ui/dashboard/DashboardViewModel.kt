package com.eggapp.field.ui.dashboard

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.Alert
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.Batch
import com.eggapp.field.data.Coop
import com.eggapp.field.data.Farm
import com.eggapp.field.data.Flock
import com.eggapp.field.data.Incubator
import com.eggapp.field.data.Me
import com.eggapp.field.data.TokenStore
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

// Matches apps/web/app/page.tsx — telemetry lands every ~60s, so poll a bit
// faster and the readings update without a manual refresh.
private const val POLL_INTERVAL_MS = 15_000L

val ACTIVE_BATCH_STATUSES = setOf("planned", "setting", "incubating", "lockdown", "hatching")

data class DashboardUiState(
    val me: Me? = null,
    val farms: List<Farm> = emptyList(),
    val farmId: String? = null,
    val incubators: List<Incubator> = emptyList(),
    val batches: List<Batch> = emptyList(),
    val flocks: List<Flock> = emptyList(),
    val alerts: List<Alert> = emptyList(),
    val coops: List<Coop> = emptyList(),
    val loading: Boolean = true,
    val error: String? = null,
) {
    val farmName: String? get() = farms.firstOrNull { it.id == farmId }?.name
    val totalBirds: Int get() = flocks.sumOf { it.currentCount }
    val activeBatches: List<Batch> get() = batches.filter { it.status in ACTIVE_BATCH_STATUSES }
    val incubatorsOnline: Int get() = incubators.count { it.device?.status == "active" }
    val criticalAlerts: Int get() = alerts.count { it.severity == "critical" }
    val warningAlerts: Int get() = alerts.count { it.severity == "warning" }
}

class DashboardViewModel(application: Application) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)

    private val _state = MutableStateFlow(DashboardUiState())
    val state: StateFlow<DashboardUiState> = _state

    init {
        viewModelScope.launch {
            val me = runCatching { api.me() }.getOrNull()?.takeIf { it.isSuccessful }?.body()
            val farms = runCatching { api.farms() }.getOrNull()?.takeIf { it.isSuccessful }?.body().orEmpty()
            // A farm was picked at login, but fall back to the first one the
            // user actually belongs to rather than showing an empty shell.
            val farmId = tokenStore.farmId()?.takeIf { id -> farms.any { it.id == id } } ?: farms.firstOrNull()?.id
            _state.value = _state.value.copy(me = me, farms = farms, farmId = farmId)

            if (farmId == null) {
                _state.value = _state.value.copy(loading = false, error = "No farm selected")
                return@launch
            }

            while (isActive) {
                // Each call is independently fault-tolerant: one endpoint
                // failing (say alerts) must not blank out the whole dashboard.
                val incubators = runCatching { api.incubators(farmId) }.getOrNull()?.body()
                val batches = runCatching { api.batches(farmId) }.getOrNull()?.body()
                val flocks = runCatching { api.flocks(farmId) }.getOrNull()?.body()
                val alerts = runCatching { api.alerts(farmId, "open") }.getOrNull()?.body()
                val coops = runCatching { api.coops(farmId) }.getOrNull()?.body()

                val allFailed = incubators == null && batches == null && flocks == null &&
                    alerts == null && coops == null
                _state.value = _state.value.copy(
                    incubators = incubators ?: _state.value.incubators,
                    batches = batches ?: _state.value.batches,
                    flocks = flocks ?: _state.value.flocks,
                    alerts = alerts ?: _state.value.alerts,
                    coops = coops ?: _state.value.coops,
                    loading = false,
                    error = if (allFailed) "Couldn't reach the farm — showing the last known state" else null,
                )
                delay(POLL_INTERVAL_MS)
            }
        }
    }

    /** Switching farms re-points every subsequent request; the caller restarts the UI. */
    fun selectFarm(id: String) = tokenStore.saveFarmId(id)

    fun logout() = tokenStore.clear()
}
