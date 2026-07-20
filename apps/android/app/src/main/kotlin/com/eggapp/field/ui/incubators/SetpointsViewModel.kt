package com.eggapp.field.ui.incubators

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.CLEAR
import com.eggapp.field.data.DeviceConfig
import com.eggapp.field.data.Incubator
import com.eggapp.field.data.SetpointRequest
import com.eggapp.field.data.Species
import com.eggapp.field.data.TokenStore
import com.eggapp.field.data.jsonPatchBody
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

// Actuator on/off can change from outside the app entirely — the device's
// own auto-control loop, or the incubator's physical button UI — so this
// screen can't rely solely on its own commands' ack-polling to stay fresh.
// Same background cadence as IncubatorsViewModel's list poll.
private const val INCUBATOR_POLL_INTERVAL_MS = 15_000L

// How long a just-toggled actuator keeps overriding whatever reloads report,
// before we give up waiting for the device to confirm and trust the server.
// Must comfortably exceed the firmware settle (~2.2s, task_mqtt.cpp) + MQTT
// publish + ingest + one reload cycle; if the relay genuinely didn't change
// (stuck/faulted) the device keeps reporting the old value, and after this
// window the UI reverts to that truth instead of lying indefinitely.
private const val ACTUATOR_PENDING_TTL_MS = 12_000L

data class SetpointsUiState(
    val incubator: Incubator? = null,
    val config: DeviceConfig? = null,
    val species: List<Species> = emptyList(),
    val loading: Boolean = true,
    val saving: Boolean = false,
    val savingIncubator: Boolean = false,
    val error: String? = null,
)

// US-INC-003, Android side: same push+poll pattern as apps/web's SetpointsCard.
// Fetches by incubatorId (not passed the full object) — same idiom as
// BatchDetailViewModel, so this screen is reachable from anywhere with
// just an id, not tied to nav-arg object passing.
class SetpointsViewModel(application: Application, private val incubatorId: String) : AndroidViewModel(application) {
    private val tokenStore = TokenStore(application)
    private val api = ApiClient.authenticated(tokenStore)
    private val farmId = tokenStore.farmId()

    private val _state = MutableStateFlow(SetpointsUiState())
    val state: StateFlow<SetpointsUiState> = _state

    private var pollJob: kotlinx.coroutines.Job? = null

    init {
        val farm = farmId
        if (farm == null) {
            _state.value = SetpointsUiState(loading = false, error = "No farm selected")
        } else {
            viewModelScope.launch {
                while (isActive) {
                    reloadIncubatorNow(farm)
                    if (_state.value.loading) _state.value = _state.value.copy(loading = false)
                    delay(INCUBATOR_POLL_INTERVAL_MS)
                }
            }
            viewModelScope.launch {
                val speciesList = runCatching { api.species() }.getOrNull()?.body().orEmpty()
                _state.value = _state.value.copy(species = speciesList)
            }
            reloadConfig()
        }
    }

    fun updateIncubator(name: String, capacity: Int, defaultSpeciesId: String?) {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(savingIncubator = true, error = null)
            val body = jsonPatchBody(
                "name" to name,
                "capacity" to capacity,
                "defaultSpeciesId" to (defaultSpeciesId ?: CLEAR),
            )
            val result = runCatching { api.updateIncubator(farm, incubatorId, body) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(savingIncubator = false, incubator = response.body())
            } else {
                _state.value = _state.value.copy(
                    savingIncubator = false,
                    error = result.exceptionOrNull()?.message ?: "Failed to save incubator",
                )
            }
        }
    }

    fun reloadConfig() {
        val farm = farmId ?: return
        viewModelScope.launch {
            val response = runCatching { api.latestConfig(farm, incubatorId) }.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(config = response.body())
                maybePollForAck()
            }
        }
    }

    // Actuator on/off + auto/manual is device state, not command-ack state —
    // it only shows up once fresh telemetry lands, not when the command is
    // acked. Re-fetch the incubator (same as apps/web's polling fix) so the
    // Switch reflects reality instead of the stale snapshot from page load.
    //
    // Several reloads can be in flight at once (submitActuator's immediate
    // call, the ack-poll loop, the post-ack grace reloads) and network
    // responses can resolve out of the order they were issued in. Without
    // reloadSeq, an older reload's response landing after a newer one would
    // overwrite fresh state with stale state — indistinguishable from "my
    // last toggle didn't take" in the UI. Only the most-recently-issued
    // reload is allowed to write to _state.
    private var reloadSeq = 0

    // Actuators the user just toggled, awaiting device confirmation. A manual
    // toggle only appears in device telemetry after the command applies AND
    // the firmware's settle delay elapses and fresh telemetry is published
    // (task_mqtt.cpp) — several seconds. Any reload landing in that window
    // returns the PRE-toggle state; writing it straight to _state is what
    // made the Switch visibly flip back before flipping forward again. So a
    // pending toggle overrides the matching device field on EVERY reload
    // until the server agrees (confirmed → cleared) or the TTL passes (give
    // up → trust the server). All access is on the Main dispatcher
    // (viewModelScope default), so the plain map needs no synchronization.
    private data class PendingToggle(val on: Boolean, val overrideOn: Boolean, val sinceMs: Long)
    private val pendingActuators = mutableMapOf<String, PendingToggle>()

    // Overlays still-pending toggles onto a freshly fetched incubator, and
    // clears any pending the server now confirms or that has expired. Returns
    // what should actually be shown. Given null (e.g. empty body) it keeps
    // the current state rather than blanking the screen.
    private fun reconcileAndApply(fresh: Incubator?): Incubator? {
        if (fresh == null) return _state.value.incubator
        val device = fresh.device ?: return fresh
        if (pendingActuators.isEmpty()) return fresh
        val now = System.currentTimeMillis()

        fun resolve(key: String, serverOn: Boolean?, serverOvr: Boolean?): Pair<Boolean?, Boolean?> {
            val p = pendingActuators[key] ?: return serverOn to serverOvr
            return if (serverOn == p.on || now - p.sinceMs > ACTUATOR_PENDING_TTL_MS) {
                pendingActuators.remove(key)
                serverOn to serverOvr
            } else {
                p.on to p.overrideOn
            }
        }

        val (fanOn, fanOvr) = resolve("fan", device.currentFanOn, device.currentFanOverride)
        val (turnerOn, turnerOvr) = resolve("turner", device.currentTurnerOn, device.currentTurnerOverride)
        val (humOn, humOvr) = resolve("humidifier", device.currentHumidifierOn, device.currentHumidifierOverride)
        val (pumpOn, pumpOvr) = resolve("pump", device.currentPumpOn, device.currentPumpOverride)

        return fresh.copy(
            device = device.copy(
                currentFanOn = fanOn, currentFanOverride = fanOvr,
                currentTurnerOn = turnerOn, currentTurnerOverride = turnerOvr,
                currentHumidifierOn = humOn, currentHumidifierOverride = humOvr,
                currentPumpOn = pumpOn, currentPumpOverride = pumpOvr,
            ),
        )
    }

    private suspend fun reloadIncubatorNow(farm: String) {
        val mySeq = ++reloadSeq
        val response = runCatching { api.incubator(farm, incubatorId) }.getOrNull()
        if (response != null && response.isSuccessful && mySeq == reloadSeq) {
            _state.value = _state.value.copy(incubator = reconcileAndApply(response.body()))
        }
    }

    private fun reloadIncubator() {
        val farm = farmId ?: return
        viewModelScope.launch { reloadIncubatorNow(farm) }
    }

    // While a command hasn't reached a final state, poll to show sent -> received -> applied live.
    private fun maybePollForAck() {
        val cfg = _state.value.config
        if (cfg == null || cfg.state == "applied" || cfg.state == "unconfirmed") {
            scheduleGraceReloads()
            return
        }
        pollJob?.cancel()
        pollJob = viewModelScope.launch {
            while (isActive) {
                delay(3_000)
                val farm = farmId ?: return@launch
                val response = runCatching { api.latestConfig(farm, incubatorId) }.getOrNull()
                if (response != null && response.isSuccessful) {
                    val fresh = response.body()
                    _state.value = _state.value.copy(config = fresh)
                    reloadIncubator()
                    if (fresh == null || fresh.state == "applied" || fresh.state == "unconfirmed") {
                        scheduleGraceReloads()
                        return@launch
                    }
                }
            }
        }
    }

    // The "applied" ack and the telemetry message that actually carries the
    // new currentFanOn/currentTurnerOn/etc. values are two independent MQTT
    // messages (ingest.ts handles them in separate handlers) — "applied"
    // routinely wins the race. Without this, the single reloadIncubator()
    // call right at "applied" can grab the device row before telemetry has
    // landed, and since that was the poll loop's last iteration, the UI is
    // then stuck on stale actuator state until some unrelated action
    // happens to trigger another reload. A short grace window of extra
    // reloads absorbs that race instead of requiring a second toggle to
    // "notice" the first one's result.
    private fun scheduleGraceReloads() {
        viewModelScope.launch {
            // Reloads are now safe at any time (reconcileAndApply keeps a
            // pending toggle showing until the device confirms it), so these
            // exist only to fetch that confirmation promptly and clear the
            // pending state. First fetch lands just after the firmware's
            // settle window (~2.2s, task_mqtt.cpp) + round trip, then a
            // couple more in case the first publish hadn't propagated yet.
            delay(2_800)
            reloadIncubator()
            repeat(2) {
                delay(1_500)
                reloadIncubator()
            }
        }
    }

    fun submit(tempSetpoint: Double, tempHysteresis: Double, humSetpoint: Double, humHysteresis: Double) {
        val farm = farmId ?: return
        viewModelScope.launch {
            _state.value = _state.value.copy(saving = true, error = null)
            val result = runCatching {
                api.pushSetpoints(
                    farm,
                    incubatorId,
                    SetpointRequest(tempSetpoint, tempHysteresis, humSetpoint, humHysteresis),
                )
            }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(saving = false, config = response.body())
                maybePollForAck()
            } else {
                _state.value = _state.value.copy(
                    saving = false,
                    error = result.exceptionOrNull()?.message ?: "Failed to send setpoints",
                )
            }
        }
    }

    // Actuator-control increment: same push+poll pipeline as submit(), but
    // only the two fields for one actuator are set per call so an
    // independent toggle doesn't touch the other three or the setpoints.
    fun submitActuator(request: SetpointRequest) {
        val farm = farmId ?: return

        // Optimistic UI: the real round trip (ack + firmware's ~2.2s settle
        // delay before it trusts the relay tasks have caught up, see
        // task_mqtt.cpp) is deliberately not instant, but the Switch tap
        // should feel like it is. Record the toggle as pending so it (a)
        // shows immediately and (b) survives every stale reload that lands
        // during the propagation window, until the device confirms it — see
        // reconcileAndApply. This is what stops the flip-back-then-forward.
        val now = System.currentTimeMillis()
        request.fanOn?.let { pendingActuators["fan"] = PendingToggle(it, request.fanOverride ?: true, now) }
        request.turnerOn?.let { pendingActuators["turner"] = PendingToggle(it, request.turnerOverride ?: true, now) }
        request.humidifierOn?.let {
            pendingActuators["humidifier"] = PendingToggle(it, request.humidifierOverride ?: true, now)
        }
        request.pumpOn?.let { pendingActuators["pump"] = PendingToggle(it, request.pumpOverride ?: true, now) }

        // Drop any reload already in flight, then paint the optimistic value
        // now by reconciling the current snapshot against the new pending set.
        reloadSeq++
        _state.value = _state.value.copy(incubator = reconcileAndApply(_state.value.incubator))

        viewModelScope.launch {
            _state.value = _state.value.copy(saving = true, error = null)
            val result = runCatching { api.pushSetpoints(farm, incubatorId, request) }
            val response = result.getOrNull()
            if (response != null && response.isSuccessful) {
                _state.value = _state.value.copy(saving = false, config = response.body())
                // No immediate reloadIncubator() here — the backend can't
                // possibly have real telemetry yet (firmware deliberately
                // waits ~2.2s before publishing, see task_mqtt.cpp), so an
                // immediate fetch would just re-read the pre-toggle state
                // and, being the most-recently-issued reload, stomp the
                // optimistic UI update above with it. maybePollForAck's
                // poll loop and grace-reload window already fetch fresh
                // state on a schedule that gives the device time to settle.
                maybePollForAck()
            } else {
                _state.value = _state.value.copy(
                    saving = false,
                    error = result.exceptionOrNull()?.message ?: "Failed to send actuator command",
                )
            }
        }
    }

    class Factory(private val application: Application, private val incubatorId: String) : ViewModelProvider.Factory {
        @Suppress("UNCHECKED_CAST")
        override fun <T : ViewModel> create(modelClass: Class<T>): T =
            SetpointsViewModel(application, incubatorId) as T
    }
}
