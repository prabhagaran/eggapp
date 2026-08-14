package com.eggapp.field.ui.alerts

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.eggapp.field.data.Alert
import com.eggapp.field.ui.components.AppCard
import com.eggapp.field.ui.components.EggAppSubBar
import com.eggapp.field.ui.components.EmptyNote
import com.eggapp.field.ui.components.PillTone
import com.eggapp.field.ui.components.StatusPill
import java.time.Duration
import java.time.Instant

/**
 * Read-only alert list — the "review →" target from the dashboard card.
 * Acknowledging and resolving stay on the web dashboard (CLAUDE.md's surface
 * split puts alert management with the admin, not the field worker), so this
 * screen deliberately offers no actions.
 */
@Composable
fun AlertsScreen(alerts: List<Alert>, onBack: () -> Unit) {
    Scaffold(topBar = { EggAppSubBar("Open alerts", onBack) }) { padding ->
        LazyColumn(modifier = Modifier.fillMaxSize().padding(padding).padding(horizontal = 16.dp)) {
            item {
                Text(
                    if (alerts.isEmpty()) "Nothing open" else "${alerts.size} open right now",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(vertical = 14.dp),
                )
            }
            if (alerts.isEmpty()) {
                item { EmptyNote("No open alerts — every machine is inside its band.") }
            }
            items(alerts.size) { index ->
                val a = alerts[index]
                AppCard(modifier = Modifier.fillMaxWidth().padding(bottom = 12.dp)) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        StatusPill(
                            text = a.severity.uppercase(),
                            tone = if (a.severity == "critical") PillTone.Danger else PillTone.Warn,
                        )
                        Text(
                            fmtAge(a.triggeredAt),
                            style = MaterialTheme.typography.labelMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    Text(
                        a.message,
                        style = MaterialTheme.typography.bodyLarge,
                        fontWeight = FontWeight.Medium,
                        modifier = Modifier.padding(top = 10.dp),
                    )
                }
            }
            item { Column(modifier = Modifier.padding(bottom = 24.dp)) {} }
        }
    }
}

private fun fmtAge(ts: String): String = runCatching {
    val secs = Duration.between(Instant.parse(ts), Instant.now()).seconds
    when {
        secs < 60 -> "${secs}s ago"
        secs < 3600 -> "${secs / 60}m ago"
        secs < 86_400 -> "${secs / 3600}h ago"
        else -> "${secs / 86_400}d ago"
    }
}.getOrDefault("—")
