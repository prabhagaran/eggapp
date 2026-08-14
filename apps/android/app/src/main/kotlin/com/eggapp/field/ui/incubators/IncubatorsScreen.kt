package com.eggapp.field.ui.incubators

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Inventory2
import androidx.compose.material.icons.filled.Thermostat
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
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
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.data.Incubator
import com.eggapp.field.ui.components.AppCard
import com.eggapp.field.ui.components.DropdownField
import com.eggapp.field.ui.components.EmptyNote
import com.eggapp.field.ui.components.MutedText
import com.eggapp.field.ui.components.PillTone
import com.eggapp.field.ui.components.SectionRule
import com.eggapp.field.ui.components.StatCard
import com.eggapp.field.ui.components.StatusPill
import com.eggapp.field.ui.components.TileRow
import com.eggapp.field.ui.theme.LocalStatusColors
import kotlinx.coroutines.delay
import java.time.Duration
import java.time.Instant

private fun ageLabel(ts: String, nowMillis: Long): Pair<String, Boolean> {
    val age = Duration.between(Instant.parse(ts), Instant.ofEpochMilli(nowMillis))
    val fresh = age.seconds < 90 // matches apps/web/lib/useAuthedFarm.ts isFresh()
    val label = if (age.seconds < 60) "${age.seconds}s ago" else "${age.toMinutes()}m ago"
    return label to fresh
}

@Composable
fun IncubatorsScreen(
    viewModel: IncubatorsViewModel = viewModel(),
    onOpenSetpoints: (String) -> Unit,
    onOpenCollections: () -> Unit,
) {
    val state by viewModel.state.collectAsState()
    var showCreate by remember { mutableStateOf(false) }

    // Drives the on-screen "Xs ago" labels forward between poll cycles.
    var now by remember { mutableLongStateOf(System.currentTimeMillis()) }
    LaunchedEffect(Unit) {
        while (true) {
            delay(1_000)
            now = System.currentTimeMillis()
        }
    }

    val onlineCount = state.incubators.count { it.device?.status == "active" }

    LazyColumn(modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp)) {
        item {
            Column(modifier = Modifier.padding(top = 18.dp, bottom = 14.dp)) {
                Text("Incubators", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
                MutedText(state.farmName ?: "your farm")
            }
        }

        item {
            TileRow {
                StatCard(
                    label = "Online",
                    value = "$onlineCount",
                    caption = "of ${state.incubators.size} incubators",
                    icon = Icons.Filled.Thermostat,
                    dark = true,
                    modifier = Modifier.weight(1f),
                )
                StatCard(
                    label = "Collections",
                    value = "Eggs",
                    caption = "record a check →",
                    icon = Icons.Filled.Inventory2,
                    onClick = onOpenCollections,
                    modifier = Modifier.weight(1f),
                )
            }
        }

        item {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                SectionRule("All incubators")
                TextButton(onClick = { showCreate = !showCreate }) { Text(if (showCreate) "Cancel" else "Add") }
            }
        }

        if (showCreate) {
            item {
                CreateIncubatorForm(
                    species = state.species,
                    saving = state.saving,
                    onSave = { name, capacity, speciesId -> viewModel.createIncubator(name, capacity, speciesId) { showCreate = false } },
                )
            }
        }

        if (state.loading) {
            item { CircularProgressIndicator(modifier = Modifier.padding(16.dp)) }
        }
        state.error?.let { err ->
            item { Text(err, color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(bottom = 8.dp)) }
        }
        if (!state.loading && state.incubators.isEmpty() && state.error == null) {
            item { EmptyNote("No incubators yet — add one with the button above.") }
        }

        items(state.incubators) { inc -> IncubatorCard(inc, now, onOpenSetpoints) }
    }
}

@Composable
private fun IncubatorCard(inc: Incubator, nowMillis: Long, onOpenSetpoints: (String) -> Unit) {
    // Always navigable — that screen also holds incubator name/capacity/
    // species editing (Phase 4), which applies whether or not a device
    // is bound yet, not just to devices with live setpoints.
    AppCard(
        modifier = Modifier.fillMaxWidth().padding(bottom = 12.dp),
        onClick = { onOpenSetpoints(inc.id) },
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(inc.name, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                MutedText("capacity ${inc.capacity}")
            }
            val device = inc.device
            StatusPill(
                text = device?.status ?: "no device",
                tone = when {
                    device == null -> PillTone.Neutral
                    device.status == "active" -> PillTone.Ok
                    else -> PillTone.Danger
                },
            )
        }

        val telemetry = inc.latestTelemetry
        if (telemetry != null) {
            val (age, fresh) = ageLabel(telemetry.ts, nowMillis)
            Row(
                modifier = Modifier.padding(top = 14.dp).fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(18.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Reading(telemetry.tempC?.let { "%.1f".format(it) }, "°C")
                Reading(telemetry.humidityPct?.let { "${it.toInt()}" }, "%")
                Text(
                    if (fresh) age else "$age ⚠",
                    style = MaterialTheme.typography.bodySmall,
                    color = if (fresh) MaterialTheme.colorScheme.onSurfaceVariant else LocalStatusColors.current.warnText,
                    modifier = Modifier.weight(1f),
                    textAlign = TextAlign.End,
                )
            }
        }
    }
}

/** Big value + small unit, matching the dashboard's sensor tiles. */
@Composable
private fun Reading(value: String?, unit: String) {
    Row(verticalAlignment = Alignment.Bottom) {
        Text(
            value ?: "—",
            style = MaterialTheme.typography.titleLarge,
            fontWeight = FontWeight.Bold,
            color = if (value == null) MaterialTheme.colorScheme.onSurfaceVariant else MaterialTheme.colorScheme.onSurface,
        )
        if (value != null) {
            Text(
                unit,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = 2.dp, bottom = 2.dp),
            )
        }
    }
}

@Composable
private fun CreateIncubatorForm(
    species: List<com.eggapp.field.data.Species>,
    saving: Boolean,
    onSave: (String, Int, String?) -> Unit,
) {
    var name by remember { mutableStateOf("") }
    var capacity by remember { mutableStateOf("") }
    var speciesId by remember { mutableStateOf("") }

    AppCard(modifier = Modifier.fillMaxWidth().padding(bottom = 12.dp)) {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Add incubator", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            OutlinedTextField(name, { name = it }, label = { Text("Name") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(
                capacity, { capacity = it }, label = { Text("Capacity (eggs)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth(),
            )
            DropdownField(
                label = "Default species (optional)",
                selectedValue = speciesId,
                options = listOf("" to "—") + species.map { it.id to it.name },
                onSelect = { speciesId = it },
                modifier = Modifier.fillMaxWidth(),
            )
            Button(
                enabled = !saving && name.isNotBlank(),
                onClick = {
                    val cap = capacity.toIntOrNull()
                    if (cap != null) onSave(name, cap, speciesId.ifBlank { null })
                },
            ) { Text(if (saving) "Saving…" else "Add incubator") }
        }
    }
}
