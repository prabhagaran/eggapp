package com.eggapp.field.ui.collections

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.data.Collection
import com.eggapp.field.data.local.CollectionEntity
import java.time.LocalDate
import java.time.temporal.ChronoUnit

private const val STALE_STORAGE_DAYS = 7 // BR-011: hatchability decays past 7 days

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CollectionsScreen(viewModel: CollectionsViewModel = viewModel(), onBack: () -> Unit) {
    val state by viewModel.state.collectAsState()
    var discardTarget by remember { mutableStateOf<Collection?>(null) }

    Scaffold(topBar = { TopAppBar(title = { Text("Egg collections") }) }) { padding ->
        LazyColumn(modifier = Modifier.padding(padding).padding(16.dp)) {
            item {
                if (state.loading) CircularProgressIndicator()
                state.error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                CollectionForm { collectedOn, count, avgWeight, note ->
                    viewModel.recordCollection(collectedOn, count, avgWeight, note)
                }
            }

            if (state.localQueued.isNotEmpty()) {
                item { Text("Not yet synced", style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(top = 16.dp)) }
                items(state.localQueued) { LocalCollectionRow(it) }
            }

            item { Text("Collections", style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(top = 16.dp)) }
            if (state.serverCollections.isEmpty() && !state.loading) {
                item { Text("None recorded yet, or offline — reconnect to see server-side collections.") }
            }
            items(state.serverCollections) { c ->
                ServerCollectionRow(c, onDiscard = { discardTarget = c })
            }
        }
    }

    discardTarget?.let { target ->
        DiscardDialog(
            available = target.availableCount,
            onConfirm = { count, reason ->
                viewModel.discard(target.id, count, reason)
                discardTarget = null
            },
            onDismiss = { discardTarget = null },
        )
    }
}

@Composable
private fun CollectionForm(onSave: (String, Int, Double?, String?) -> Unit) {
    var collectedOn by remember { mutableStateOf(LocalDate.now().toString()) }
    var count by remember { mutableStateOf("") }
    var avgWeight by remember { mutableStateOf("") }
    var note by remember { mutableStateOf("") }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Record a collection", style = MaterialTheme.typography.titleMedium)
            OutlinedTextField(
                value = collectedOn,
                onValueChange = { collectedOn = it },
                label = { Text("Collected on (YYYY-MM-DD)") },
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = count,
                onValueChange = { count = it },
                label = { Text("Count") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = avgWeight,
                onValueChange = { avgWeight = it },
                label = { Text("Avg weight, grams (optional)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = note,
                onValueChange = { note = it },
                label = { Text("Source (optional, e.g. coop 1)") },
                modifier = Modifier.fillMaxWidth(),
            )
            Button(onClick = {
                val n = count.toIntOrNull()
                if (n != null && n > 0) {
                    onSave(collectedOn, n, avgWeight.toDoubleOrNull(), note.ifBlank { null })
                    count = ""; avgWeight = ""; note = ""
                }
            }) { Text("Save (works offline)") }
        }
    }
}

@Composable
private fun LocalCollectionRow(entity: CollectionEntity) {
    Row(modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp), horizontalArrangement = Arrangement.SpaceBetween) {
        Text("${entity.collectedOn} · ${entity.count} eggs")
        Text(
            entity.status,
            color = if (entity.status == "conflict") MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun ServerCollectionRow(c: Collection, onDiscard: () -> Unit) {
    val ageDays = ChronoUnit.DAYS.between(LocalDate.parse(c.collectedOn.take(10)), LocalDate.now())
    val stale = ageDays > STALE_STORAGE_DAYS

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Column(modifier = Modifier.padding(12.dp)) {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("${c.collectedOn.take(10)} · ${c.count} eggs")
                Text(
                    "${ageDays}d old",
                    color = if (stale) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Text("available ${c.availableCount} · assigned ${c.assignedCount} · discarded ${c.discardedCount}")
            c.sourceNote?.let { Text(it, style = MaterialTheme.typography.bodySmall) }
            if (c.availableCount > 0) {
                TextButton(onClick = onDiscard) { Text("Discard eggs") }
            }
        }
    }
}

@Composable
private fun DiscardDialog(available: Int, onConfirm: (Int, String) -> Unit, onDismiss: () -> Unit) {
    var count by remember { mutableStateOf("") }
    var reason by remember { mutableStateOf("") }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Discard eggs ($available available)") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = count,
                    onValueChange = { count = it },
                    label = { Text("Count") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = reason,
                    onValueChange = { reason = it },
                    label = { Text("Reason (cracked, dirty, ...)") },
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        },
        confirmButton = {
            TextButton(onClick = {
                val n = count.toIntOrNull()
                if (n != null && n in 1..available && reason.isNotBlank()) onConfirm(n, reason)
            }) { Text("Discard") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}
