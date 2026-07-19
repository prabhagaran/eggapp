package com.eggapp.field.ui.incubators

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.ui.components.DropdownField
import com.eggapp.field.ui.components.MutedText
import com.eggapp.field.ui.components.PillTone
import com.eggapp.field.ui.components.StatusPill

private val CONFIG_STATE_LABEL = mapOf(
    "sent" to "sent — waiting for device",
    "received" to "received by device — applying",
    "applied" to "applied ✓",
    "unconfirmed" to "unconfirmed — device may be offline",
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SetpointsScreen(incubatorId: String, onBack: () -> Unit) {
    val application = androidx.compose.ui.platform.LocalContext.current.applicationContext as android.app.Application
    val viewModel: SetpointsViewModel = viewModel(factory = SetpointsViewModel.Factory(application, incubatorId))
    val state by viewModel.state.collectAsState()

    var tempSetpoint by remember { mutableStateOf("") }
    var tempHysteresis by remember { mutableStateOf("") }
    var humSetpoint by remember { mutableStateOf("") }
    var humHysteresis by remember { mutableStateOf("") }

    // Prefill once the incubator's current setpoints load — remember{}
    // alone only captures the initial (empty) state, since the fetch
    // resolves asynchronously after first composition.
    LaunchedEffect(state.incubator) {
        val device = state.incubator?.device ?: return@LaunchedEffect
        tempSetpoint = device.currentTempSetpoint?.toString() ?: tempSetpoint
        tempHysteresis = device.currentTempHysteresis?.toString() ?: tempHysteresis
        humSetpoint = device.currentHumSetpoint?.toString() ?: humSetpoint
        humHysteresis = device.currentHumHysteresis?.toString() ?: humHysteresis
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("${state.incubator?.name ?: "Incubator"} — setpoints") },
                navigationIcon = { IconButton(onClick = onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back") } },
            )
        },
    ) { padding ->
        Column(modifier = Modifier.padding(padding).padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            if (state.loading) CircularProgressIndicator()
            state.error?.let { Text(it, color = MaterialTheme.colorScheme.error) }

            state.incubator?.let { incubator ->
                IncubatorEditCard(
                    incubator = incubator,
                    species = state.species,
                    saving = state.savingIncubator,
                    onSave = viewModel::updateIncubator,
                )
            }

            OutlinedTextField(
                value = tempSetpoint,
                onValueChange = { tempSetpoint = it },
                label = { Text("Temp setpoint (°C)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = tempHysteresis,
                onValueChange = { tempHysteresis = it },
                label = { Text("Temp hysteresis (°C)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = humSetpoint,
                onValueChange = { humSetpoint = it },
                label = { Text("Humidity setpoint (%)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = humHysteresis,
                onValueChange = { humHysteresis = it },
                label = { Text("Humidity hysteresis (%)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                modifier = Modifier.fillMaxWidth(),
            )

            Button(
                enabled = !state.saving,
                onClick = {
                    val t = tempSetpoint.toDoubleOrNull()
                    val th = tempHysteresis.toDoubleOrNull()
                    val h = humSetpoint.toDoubleOrNull()
                    val hh = humHysteresis.toDoubleOrNull()
                    if (t != null && th != null && h != null && hh != null) {
                        viewModel.submit(t, th, h, hh)
                    }
                },
            ) {
                Text(if (state.saving) "Sending…" else "Send to device")
            }

            state.config?.let { cfg ->
                StatusPill(
                    "v${cfg.version}: ${CONFIG_STATE_LABEL[cfg.state] ?: cfg.state}",
                    tone = when (cfg.state) {
                        "applied" -> PillTone.Ok
                        "unconfirmed" -> PillTone.Danger
                        else -> PillTone.Accent
                    },
                )
            }
        }
    }
}

@Composable
private fun IncubatorEditCard(
    incubator: com.eggapp.field.data.Incubator,
    species: List<com.eggapp.field.data.Species>,
    saving: Boolean,
    onSave: (String, Int, String?) -> Unit,
) {
    // Keyed on incubator.id so a fresh load re-seeds the form, but the
    // user's in-progress edits survive unrelated recompositions (same
    // idiom as apps/web's key={editing?.id} remount trick).
    var name by remember(incubator.id) { mutableStateOf(incubator.name) }
    var capacity by remember(incubator.id) { mutableStateOf(incubator.capacity.toString()) }
    var speciesId by remember(incubator.id) { mutableStateOf(incubator.defaultSpeciesId ?: "") }

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Incubator settings", style = MaterialTheme.typography.titleMedium)
            OutlinedTextField(name, { name = it }, label = { Text("Name") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(
                capacity, { capacity = it }, label = { Text("Capacity (eggs)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth(),
            )
            DropdownField(
                label = "Default species",
                selectedValue = speciesId,
                options = listOf("" to "—") + species.map { it.id to it.name },
                onSelect = { speciesId = it },
                modifier = Modifier.fillMaxWidth(),
            )
            Button(
                enabled = !saving,
                onClick = {
                    val cap = capacity.toIntOrNull()
                    if (name.isNotBlank() && cap != null) onSave(name, cap, speciesId.ifBlank { null })
                },
            ) { Text(if (saving) "Saving…" else "Save incubator") }
        }
    }
}
