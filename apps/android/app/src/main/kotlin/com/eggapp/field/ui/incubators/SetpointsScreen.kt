package com.eggapp.field.ui.incubators

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
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

    Scaffold(topBar = { TopAppBar(title = { Text("${state.incubator?.name ?: "Incubator"} — setpoints") }) }) { padding ->
        Column(modifier = Modifier.padding(padding).padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            if (state.loading) CircularProgressIndicator()
            state.error?.let { Text(it, color = MaterialTheme.colorScheme.error) }

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
                Text(
                    "v${cfg.version}: ${CONFIG_STATE_LABEL[cfg.state] ?: cfg.state}",
                    color = if (cfg.state == "unconfirmed") MaterialTheme.colorScheme.error
                    else MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}
