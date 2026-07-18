package com.eggapp.field.ui.incubators

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Inventory2
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.data.Incubator
import com.eggapp.field.ui.components.MutedText
import com.eggapp.field.ui.components.PillTone
import com.eggapp.field.ui.components.StatusPill
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

    // Drives the on-screen "Xs ago" labels forward between poll cycles.
    var now by remember { mutableLongStateOf(System.currentTimeMillis()) }
    LaunchedEffect(Unit) {
        while (true) {
            delay(1_000)
            now = System.currentTimeMillis()
        }
    }

    val onlineCount = state.incubators.count { it.device?.status == "active" }

    LazyColumn(modifier = Modifier.fillMaxWidth().statusBarsPadding().padding(horizontal = 16.dp)) {
        item {
            Column(modifier = Modifier.padding(top = 20.dp, bottom = 16.dp)) {
                Text("Hello,", style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text(
                    state.farmName ?: "your farm",
                    style = MaterialTheme.typography.headlineSmall,
                    fontWeight = FontWeight.Bold,
                )
            }
        }

        item {
            Row(
                modifier = Modifier.fillMaxWidth().padding(bottom = 16.dp),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                HeroStatCard(
                    label = "Incubators online",
                    value = "$onlineCount",
                    total = "of ${state.incubators.size}",
                    modifier = Modifier.weight(1f),
                )
                Card(
                    modifier = Modifier.weight(1f),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
                    onClick = onOpenCollections,
                ) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Icon(Icons.Filled.Inventory2, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                        Text("Egg Collections", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(top = 8.dp))
                        MutedText("record a check →")
                    }
                }
            }
        }

        item {
            Text("Incubators", style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(bottom = 8.dp))
        }

        if (state.loading) {
            item { CircularProgressIndicator(modifier = Modifier.padding(16.dp)) }
        }
        state.error?.let { err ->
            item { Text(err, color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(bottom = 8.dp)) }
        }
        if (!state.loading && state.incubators.isEmpty() && state.error == null) {
            item { MutedText("No incubators yet — add one from the web dashboard.") }
        }

        items(state.incubators) { inc -> IncubatorCard(inc, now, onOpenSetpoints) }
    }
}

@Composable
private fun HeroStatCard(label: String, value: String, total: String, modifier: Modifier = Modifier) {
    Card(
        modifier = modifier,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.secondary,
            contentColor = MaterialTheme.colorScheme.onSecondary,
        ),
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(label, style = MaterialTheme.typography.labelMedium)
            Text(value, style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Bold, modifier = Modifier.padding(top = 6.dp))
            Text(total, style = MaterialTheme.typography.bodySmall)
        }
    }
}

@Composable
private fun IncubatorCard(inc: Incubator, nowMillis: Long, onOpenSetpoints: (String) -> Unit) {
    Card(
        modifier = Modifier.fillMaxWidth().padding(bottom = 12.dp),
        onClick = { if (inc.device != null) onOpenSetpoints(inc.id) },
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(inc.name, style = MaterialTheme.typography.titleMedium)
                MutedText("capacity ${inc.capacity}")
            }

            val device = inc.device
            if (device != null) {
                StatusPill(
                    text = device.status,
                    tone = if (device.status == "active") PillTone.Ok else PillTone.Danger,
                    modifier = Modifier.padding(top = 8.dp),
                )
            } else {
                StatusPill(text = "no device", tone = PillTone.Neutral, modifier = Modifier.padding(top = 8.dp))
            }

            val telemetry = inc.latestTelemetry
            if (telemetry != null) {
                val (age, fresh) = ageLabel(telemetry.ts, nowMillis)
                Row(
                    modifier = Modifier.padding(top = 12.dp).fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(telemetry.tempC?.let { "%.1f°C".format(it) } ?: "—", style = MaterialTheme.typography.titleMedium)
                    Text(telemetry.humidityPct?.let { "${it.toInt()}%" } ?: "—", style = MaterialTheme.typography.titleMedium)
                    MutedText(if (fresh) age else "$age ⚠")
                }
            }
        }
    }
}
