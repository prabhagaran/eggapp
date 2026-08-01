package com.eggapp.field.ui.dashboard

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Air
import androidx.compose.material.icons.filled.NotificationsActive
import androidx.compose.material.icons.filled.Egg
import androidx.compose.material.icons.filled.Fireplace
import androidx.compose.material.icons.filled.Grass
import androidx.compose.material.icons.filled.LightMode
import androidx.compose.material.icons.filled.Pets
import androidx.compose.material.icons.filled.RotateRight
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material.icons.filled.Thermostat
import androidx.compose.material.icons.filled.Toys
import androidx.compose.material.icons.filled.WaterDrop
import androidx.compose.material.icons.filled.Waves
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.data.Batch
import com.eggapp.field.ui.components.AppCard
import com.eggapp.field.ui.components.EmptyNote
import com.eggapp.field.ui.components.LiveBand
import com.eggapp.field.ui.components.Range
import com.eggapp.field.ui.components.SectionRule
import com.eggapp.field.ui.components.SensorTile
import com.eggapp.field.ui.components.StatCard
import com.eggapp.field.ui.components.StateTile
import com.eggapp.field.ui.components.StripSelector
import com.eggapp.field.ui.components.TileRow
import com.eggapp.field.ui.theme.LocalStatusColors
import kotlinx.coroutines.delay
import java.time.Duration
import java.time.Instant

// Optimal bands for the coop's live tiles (ADR 0009), copied from
// apps/web/app/page.tsx. Unlike an incubator — whose correct temperature
// depends on the species profile it is running — a bird house has genuine
// species-independent comfort and welfare ranges, so fixed bands are right.
private val COOP_TEMP_RANGE = Range(min = 18.0, max = 30.0, warnBand = 4.0)
private val COOP_HUM_RANGE = Range(min = 50.0, max = 70.0, warnBand = 10.0)
private val CO2_RANGE = Range(max = 450.0, warnBand = 550.0)
private val NH3_RANGE = Range(max = 10.0, warnBand = 10.0)
private val FEED_RANGE = Range(min = 20.0, warnBand = 10.0)
private val WATER_RANGE = Range(min = 25.0, warnBand = 15.0)

private const val FRESH_SECONDS = 90L // matches apps/web/lib/useAuthedFarm.ts

private fun isFresh(ts: String, nowMillis: Long): Boolean =
    runCatching {
        Duration.between(Instant.parse(ts), Instant.ofEpochMilli(nowMillis)).seconds < FRESH_SECONDS
    }.getOrDefault(false)

private fun fmtAge(ts: String, nowMillis: Long): String = runCatching {
    val secs = Duration.between(Instant.parse(ts), Instant.ofEpochMilli(nowMillis)).seconds
    when {
        secs < 60 -> "${secs}s ago"
        secs < 3600 -> "${secs / 60}m ago"
        else -> "${secs / 3600}h ago"
    }
}.getOrDefault("—")

@Composable
fun DashboardScreen(
    viewModel: DashboardViewModel,
    onOpenAlerts: () -> Unit,
    onOpenBatch: (Batch) -> Unit,
    onOpenBatches: () -> Unit,
    onOpenFlocks: () -> Unit,
    onOpenIncubators: () -> Unit,
) {
    val state by viewModel.state.collectAsState()

    // Drives the "Xs ago" labels forward between poll cycles.
    var now by remember { mutableLongStateOf(System.currentTimeMillis()) }
    LaunchedEffect(Unit) {
        while (true) {
            delay(1_000)
            now = System.currentTimeMillis()
        }
    }

    // null = follow the freshest machine, which is also the behaviour before
    // the user has picked anything. Storing the id rather than an index keeps
    // the selection stable across the 15s poll even if the list reorders.
    var selectedCoopId by remember { mutableStateOf<String?>(null) }
    var selectedIncId by remember { mutableStateOf<String?>(null) }

    val reportingCoops = state.coops
        .filter { it.latestTelemetry != null }
        .sortedByDescending { Instant.parse(it.latestTelemetry!!.ts) }
    val monitoredCoop = reportingCoops.firstOrNull { it.id == selectedCoopId } ?: reportingCoops.firstOrNull()

    val reportingIncs = state.incubators
        .filter { it.latestTelemetry != null }
        .sortedByDescending { Instant.parse(it.latestTelemetry!!.ts) }
    val liveInc = reportingIncs.firstOrNull { it.id == selectedIncId } ?: reportingIncs.firstOrNull()

    LazyColumn(modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp)) {
        item {
            Column(modifier = Modifier.padding(top = 18.dp)) {
                Text("Dashboard", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
                Text(
                    "Plan, monitor, and act on your farm with ease.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(top = 2.dp),
                )
            }
        }

        if (state.loading) {
            item { CircularProgressIndicator(modifier = Modifier.padding(24.dp)) }
        }
        state.error?.let { err ->
            item {
                Text(
                    err,
                    color = MaterialTheme.colorScheme.error,
                    style = MaterialTheme.typography.bodySmall,
                    modifier = Modifier.padding(top = 12.dp),
                )
            }
        }

        item { SectionRule("Farm Overview") }
        item {
            TileRow {
                StatCard(
                    label = "Total Birds",
                    value = if (state.flocks.isEmpty() && state.loading) "—" else "${state.totalBirds}",
                    caption = "across ${state.flocks.size} flocks",
                    icon = Icons.Filled.Pets,
                    dark = true,
                    onClick = onOpenFlocks,
                    modifier = Modifier.weight(1f),
                )
                StatCard(
                    label = "Active Batches",
                    value = "${state.activeBatches.size}",
                    caption = "of ${state.batches.size} total",
                    icon = Icons.Filled.Egg,
                    onClick = onOpenBatches,
                    modifier = Modifier.weight(1f),
                )
            }
        }
        item {
            TileRow {
                StatCard(
                    label = "Incubators",
                    value = "${state.incubatorsOnline}",
                    caption = "of ${state.incubators.size} online",
                    icon = Icons.Filled.Thermostat,
                    onClick = onOpenIncubators,
                    modifier = Modifier.weight(1f),
                )
                StatCard(
                    label = "Open Alerts",
                    value = "${state.alerts.size}",
                    caption = "review →",
                    icon = Icons.Filled.NotificationsActive,
                    onClick = onOpenAlerts,
                    modifier = Modifier.weight(1f),
                )
            }
        }

        if (state.alerts.isNotEmpty()) {
            item {
                AppCard(modifier = Modifier.fillMaxWidth().padding(top = 4.dp)) {
                    Text("Open alerts by severity", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                    Text(
                        "${state.alerts.size} open right now",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(top = 2.dp, bottom = 12.dp),
                    )
                    SeverityBar("Critical", state.criticalAlerts, state.alerts.size, critical = true)
                    SeverityBar("Warning", state.warningAlerts, state.alerts.size, critical = false)
                }
            }
        }

        // A farm with no coop monitor gets no tiles at all — the correct
        // empty state. Rendering the grid with every value dashed would
        // imply broken sensors rather than "you haven't set this up".
        val coopT = monitoredCoop?.latestTelemetry
        if (monitoredCoop != null && coopT != null) {
            val live = isFresh(coopT.ts, now)
            item {
                Column(modifier = Modifier.padding(top = 18.dp)) {
                    LiveBand(
                        title = "Real-Time Coop Monitoring",
                        subtitle = buildString {
                            append(if (coopT.simulated) "Fabricated by " else "Live data from ")
                            append(monitoredCoop.device?.name ?: monitoredCoop.device?.hardwareId ?: "device")
                            append(" — ").append(monitoredCoop.name)
                            if (coopT.simulated) append(" · no sensors connected")
                        },
                        simulated = coopT.simulated,
                        connected = live,
                        ageLabel = fmtAge(coopT.ts, now),
                    )
                    StripSelector(
                        items = reportingCoops.map { Triple(it.id, it.name, isFresh(it.latestTelemetry!!.ts, now)) },
                        selectedId = monitoredCoop.id,
                        onSelect = { selectedCoopId = it },
                    )
                    if (!live) StaleNote(fmtAge(coopT.ts, now))
                }
            }
            item { SectionRule("Environmental Conditions") }
            item {
                TileRow {
                    SensorTile(Icons.Filled.Thermostat, "Temperature", coopT.tempC, "°C", COOP_TEMP_RANGE, modifier = Modifier.weight(1f))
                    SensorTile(Icons.Filled.WaterDrop, "Humidity", coopT.humidityPct, "%", COOP_HUM_RANGE, decimals = 0, modifier = Modifier.weight(1f))
                }
            }
            item {
                TileRow {
                    SensorTile(Icons.Filled.Air, "CO₂ Level", coopT.co2Ppm, "ppm", CO2_RANGE, decimals = 0, modifier = Modifier.weight(1f))
                    SensorTile(Icons.Filled.Speed, "Ammonia", coopT.ammoniaPpm, "ppm", NH3_RANGE, modifier = Modifier.weight(1f))
                }
            }
            item { SectionRule("Resource Management") }
            item {
                TileRow {
                    SensorTile(Icons.Filled.LightMode, "Light", coopT.lightLux, "lux", decimals = 0, hint = "Ambient light", modifier = Modifier.weight(1f))
                    SensorTile(Icons.Filled.Grass, "Feed Level", coopT.feedLevelPct, "%", FEED_RANGE, hint = "Refill at <20%", modifier = Modifier.weight(1f))
                }
            }
            item {
                TileRow {
                    SensorTile(Icons.Filled.WaterDrop, "Water Level", coopT.waterLevelPct, "%", WATER_RANGE, hint = "Refill at <25%", modifier = Modifier.weight(1f))
                    Box(modifier = Modifier.weight(1f))
                }
            }
        } else if (!state.loading) {
            item {
                Column(modifier = Modifier.padding(top = 18.dp)) {
                    LiveBand(
                        title = "Real-Time Coop Monitoring",
                        subtitle = if (state.coops.isEmpty()) {
                            "No coops yet — add one from the web dashboard to monitor house conditions"
                        } else {
                            "No coop is reporting telemetry yet — bind a monitor device"
                        },
                    )
                }
            }
        }

        // Incubator strip — same tile system, different machine: bands come
        // from its own setpoints, and it has relays worth showing.
        val incT = liveInc?.latestTelemetry
        if (liveInc != null && incT != null) {
            val incFresh = isFresh(incT.ts, now)
            // Bands from the machine's own setpoints, not a fixed comfort
            // range: the correct temperature depends on the species profile
            // it is running, so a hardcoded band would flag a correctly-set
            // duck incubator as abnormal.
            val tempSp = liveInc.device?.currentTempSetpoint
            val tempHyst = liveInc.device?.currentTempHysteresis ?: 0.5
            val humSp = liveInc.device?.currentHumSetpoint
            val humHyst = liveInc.device?.currentHumHysteresis ?: 5.0
            val tempRange = tempSp?.let { Range(it - tempHyst, it + tempHyst, tempHyst * 2) }
            val humRange = humSp?.let { Range(it - humHyst, it + humHyst, humHyst * 2) }

            item {
                Column(modifier = Modifier.padding(top = 18.dp)) {
                    LiveBand(
                        title = "Incubator Conditions",
                        subtitle = buildString {
                            append(liveInc.device?.name ?: liveInc.device?.hardwareId ?: "device")
                            append(" — ").append(liveInc.name)
                            if (tempSp != null) append(" · target ${tempSp}°C / ${humSp}%")
                        },
                        connected = incFresh,
                        ageLabel = fmtAge(incT.ts, now),
                    )
                    StripSelector(
                        items = reportingIncs.map { Triple(it.id, it.name, isFresh(it.latestTelemetry!!.ts, now)) },
                        selectedId = liveInc.id,
                        onSelect = { selectedIncId = it },
                    )
                    if (!incFresh) StaleNote(fmtAge(incT.ts, now))
                }
            }
            item {
                TileRow(modifier = Modifier.padding(top = 12.dp)) {
                    SensorTile(
                        Icons.Filled.Thermostat, "Temperature", incT.tempC, "°C", tempRange,
                        hint = tempSp?.let { "Setpoint $it°C ±$tempHyst" } ?: "No setpoint reported yet",
                        modifier = Modifier.weight(1f),
                    )
                    SensorTile(
                        Icons.Filled.WaterDrop, "Humidity", incT.humidityPct, "%", humRange, decimals = 0,
                        hint = humSp?.let { "Setpoint $it% ±$humHyst" } ?: "No setpoint reported yet",
                        modifier = Modifier.weight(1f),
                    )
                }
            }
            item {
                TileRow {
                    StateTile(Icons.Filled.Fireplace, "Heater", incT.heaterOn, "Warms to setpoint", Modifier.weight(1f))
                    StateTile(Icons.Filled.Air, "Cooler", incT.coolerOn, "Vents above setpoint", Modifier.weight(1f))
                }
            }
            item { SectionRule("Incubator Actuators") }
            item {
                TileRow {
                    StateTile(Icons.Filled.WaterDrop, "Humidifier", incT.humidifierOn, "Raises humidity", Modifier.weight(1f))
                    StateTile(Icons.Filled.Toys, "Fan", incT.fanOn, "Circulates air", Modifier.weight(1f))
                }
            }
            item {
                TileRow {
                    StateTile(Icons.Filled.RotateRight, "Egg Turner", incT.turnerOn, "Rotates trays on schedule", Modifier.weight(1f))
                    StateTile(Icons.Filled.Waves, "Pump", incT.pumpOn, "Refills the water tray", Modifier.weight(1f))
                }
            }
        }

        item { SectionRule("Active batches") }
        if (state.activeBatches.isEmpty() && !state.loading) {
            item { EmptyNote("No active batches — start one from the Batches tab.") }
        }
        items(state.activeBatches.size) { index ->
            val b = state.activeBatches[index]
            AppCard(
                modifier = Modifier.fillMaxWidth().padding(bottom = 12.dp),
                onClick = { onOpenBatch(b) },
            ) {
                Text(
                    "${b.species?.name ?: b.speciesId} — ${b.incubator?.name ?: "unassigned"}",
                    style = MaterialTheme.typography.titleSmall,
                    fontWeight = FontWeight.Bold,
                )
                Text(
                    "${b.status} · ${b.viableCount} viable eggs",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(top = 4.dp),
                )
                b.fertilityPct?.let {
                    Text(
                        "Fertility ${it.toInt()}%",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }

        item { Box(modifier = Modifier.height(24.dp)) }
    }
}

@Composable
private fun SeverityBar(label: String, count: Int, total: Int, critical: Boolean) {
    val status = LocalStatusColors.current
    val fraction = if (total > 0) count.toFloat() / total else 0f
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Text(
            label,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.weight(0.3f),
        )
        Box(
            modifier = Modifier
                .weight(1f)
                .height(9.dp)
                .background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(999.dp)),
        ) {
            if (fraction > 0f) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth(fraction)
                        .height(9.dp)
                        .background(
                            if (critical) status.dangerText else status.warnText,
                            RoundedCornerShape(999.dp),
                        ),
                )
            }
        }
        Text("$count", style = MaterialTheme.typography.labelLarge, fontWeight = FontWeight.Bold)
    }
}

/** Stale readings stay on screen, but never unlabelled — same as the web. */
@Composable
private fun StaleNote(age: String) {
    val status = LocalStatusColors.current
    Text(
        "Last reading was $age. These values were true then, not now — the device has stopped reporting, so treat every tile below as history.",
        style = MaterialTheme.typography.bodySmall,
        color = status.warnText,
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 10.dp)
            .background(status.warnBg, RoundedCornerShape(12.dp))
            .padding(12.dp),
    )
}
