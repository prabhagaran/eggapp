package com.eggapp.field.ui.incubators

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.data.Incubator
import kotlinx.coroutines.delay
import java.time.Duration
import java.time.Instant

private fun ageLabel(ts: String, nowMillis: Long): Pair<String, Boolean> {
    val age = Duration.between(Instant.parse(ts), Instant.ofEpochMilli(nowMillis))
    val fresh = age.seconds < 90 // matches apps/web/lib/useAuthedFarm.ts isFresh()
    val label = if (age.seconds < 60) "${age.seconds}s ago" else "${age.toMinutes()}m ago"
    return label to fresh
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun IncubatorsScreen(
    viewModel: IncubatorsViewModel = viewModel(),
    onLogout: () -> Unit,
    onOpenBatches: () -> Unit,
    onOpenCollections: () -> Unit,
) {
    val state by viewModel.state.collectAsState()

    // Drives the on-screen "Xs ago" labels forward between poll cycles.
    var now by remember { mutableLongStateOf(System.currentTimeMillis()) }
    LaunchedEffect(Unit) {
        while (true) {
            delay(1_000)
            now = System.currentTimeMillis()
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Incubators") },
                actions = {
                    TextButton(onClick = onOpenBatches) { Text("Batches") }
                    TextButton(onClick = onOpenCollections) { Text("Collections") }
                    TextButton(onClick = { viewModel.logout(); onLogout() }) { Text("Log out") }
                },
            )
        },
    ) { padding ->
        Column(modifier = Modifier.padding(padding)) {
            if (state.loading) {
                CircularProgressIndicator(modifier = Modifier.padding(16.dp))
            }
            if (state.error != null) {
                Text(state.error!!, color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(16.dp))
            }
            if (!state.loading && state.incubators.isEmpty() && state.error == null) {
                Text(
                    "No incubators yet — add one from the web dashboard.",
                    modifier = Modifier.padding(16.dp),
                )
            }
            LazyColumn {
                items(state.incubators) { inc -> IncubatorCard(inc, now) }
            }
        }
    }
}

@Composable
private fun IncubatorCard(inc: Incubator, nowMillis: Long) {
    Card(modifier = Modifier.fillMaxWidth().padding(12.dp)) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(inc.name, style = MaterialTheme.typography.titleMedium)
            Text("capacity ${inc.capacity}", style = MaterialTheme.typography.bodySmall)

            val device = inc.device
            if (device != null) {
                Text("Device: ${device.name ?: device.hardwareId} — ${device.status}")
            } else {
                Text("No device bound", color = MaterialTheme.colorScheme.error)
            }

            val telemetry = inc.latestTelemetry
            if (telemetry != null) {
                val (age, fresh) = ageLabel(telemetry.ts, nowMillis)
                Row(
                    modifier = Modifier.padding(top = 8.dp).fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Text(telemetry.tempC?.let { "%.1f°C".format(it) } ?: "—")
                    Text(telemetry.humidityPct?.let { "${it.toInt()}%" } ?: "—")
                    Text(
                        age,
                        color = if (fresh) MaterialTheme.colorScheme.onSurfaceVariant
                        else MaterialTheme.colorScheme.error,
                    )
                }
            }
        }
    }
}
