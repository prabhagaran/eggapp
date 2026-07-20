package com.eggapp.field.ui.batch

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
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
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.data.Batch
import com.eggapp.field.data.BatchEggSourceInput
import com.eggapp.field.data.Collection
import com.eggapp.field.data.Incubator
import com.eggapp.field.data.Species
import com.eggapp.field.ui.components.DropdownField
import com.eggapp.field.ui.components.MutedText
import com.eggapp.field.ui.components.PillTone
import com.eggapp.field.ui.components.StatusPill
import java.time.Duration
import java.time.Instant

private fun batchTone(status: String) = when (status) {
    "completed", "hatching" -> PillTone.Ok
    "aborted", "closed" -> PillTone.Neutral
    else -> PillTone.Accent
}

private fun collectionAgeDays(collectedOn: String): Long =
    Duration.between(Instant.parse(collectedOn), Instant.now()).toDays()

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun BatchesScreen(viewModel: BatchesViewModel = viewModel(), onOpenBatch: (Batch) -> Unit) {
    val state by viewModel.state.collectAsState()
    var showCreate by remember { mutableStateOf(false) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Batches") },
                actions = { TextButton(onClick = { showCreate = !showCreate }) { Text(if (showCreate) "Cancel" else "Add") } },
            )
        },
    ) { padding ->
        LazyColumn(modifier = Modifier.padding(padding).padding(horizontal = 16.dp)) {
            item {
                if (state.loading) CircularProgressIndicator(modifier = Modifier.padding(16.dp))
                state.error?.let { Text(it, color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(vertical = 8.dp)) }
            }
            if (showCreate) {
                item {
                    CreateBatchForm(
                        species = state.species,
                        incubators = state.incubators,
                        collections = state.collections,
                        saving = state.saving,
                        onSave = { incubatorId, speciesId, sources, note ->
                            viewModel.createBatch(incubatorId, speciesId, sources, note) { showCreate = false }
                        },
                    )
                }
            }
            if (!state.loading && state.batches.isEmpty() && state.error == null) {
                item { MutedText("No active batches — start one above.", modifier = Modifier.padding(vertical = 8.dp)) }
            }
            items(state.batches) { batch ->
                Card(
                    modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp),
                    onClick = { onOpenBatch(batch) },
                ) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text(
                            "${batch.species?.name ?: batch.speciesId} — ${batch.incubator?.name ?: ""}",
                            style = MaterialTheme.typography.titleMedium,
                        )
                        Row(
                            modifier = Modifier.padding(top = 6.dp).fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                        ) {
                            StatusPill(batch.status, batchTone(batch.status))
                            MutedText("${batch.viableCount} viable")
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun CreateBatchForm(
    species: List<Species>,
    incubators: List<Incubator>,
    collections: List<Collection>,
    saving: Boolean,
    onSave: (String, String, List<BatchEggSourceInput>, String?) -> Unit,
) {
    var incubatorId by remember(incubators) { mutableStateOf(incubators.firstOrNull()?.id ?: "") }
    var speciesId by remember { mutableStateOf("chicken") }
    var overrideNote by remember { mutableStateOf("") }
    val picked = remember { mutableStateMapOf<String, String>() }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("New batch", style = MaterialTheme.typography.titleMedium)
            when {
                collections.isEmpty() -> MutedText("No collections with available eggs — record one first.")
                incubators.isEmpty() -> MutedText("No incubators yet — add one on the Home tab first.")
                else -> {
                    DropdownField("Incubator", incubatorId, incubators.map { it.id to it.name }, { incubatorId = it }, Modifier.fillMaxWidth())
                    DropdownField("Species", speciesId, species.map { it.id to it.name }, { speciesId = it }, Modifier.fillMaxWidth())

                    Text("Egg collections", style = MaterialTheme.typography.titleSmall)
                    val oldEggsPicked = collections.any { c ->
                        val n = picked[c.id]?.toIntOrNull() ?: 0
                        n > 0 && collectionAgeDays(c.collectedOn) > 14
                    }
                    collections.forEach { c ->
                        val age = collectionAgeDays(c.collectedOn)
                        Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                            Column(modifier = Modifier.weight(1f)) {
                                Text(c.collectedOn.take(10))
                                MutedText("$age d old · ${c.availableCount} available")
                            }
                            OutlinedTextField(
                                value = picked[c.id] ?: "",
                                onValueChange = { picked[c.id] = it },
                                label = { Text("Use") },
                                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                                modifier = Modifier.width(96.dp),
                            )
                        }
                    }
                    if (oldEggsPicked) {
                        OutlinedTextField(
                            overrideNote, { overrideNote = it },
                            label = { Text("Override note (required — eggs older than 14 days)") },
                            modifier = Modifier.fillMaxWidth(),
                        )
                    }
                    Button(
                        enabled = !saving && incubatorId.isNotBlank() && speciesId.isNotBlank() &&
                            (!oldEggsPicked || overrideNote.isNotBlank()),
                        onClick = {
                            val sources = picked.entries
                                .mapNotNull { (id, n) -> n.toIntOrNull()?.takeIf { it > 0 }?.let { BatchEggSourceInput(id, it) } }
                            if (sources.isNotEmpty()) {
                                onSave(incubatorId, speciesId, sources, overrideNote.ifBlank { null })
                            }
                        },
                    ) { Text(if (saving) "Saving…" else "Create batch") }
                }
            }
        }
    }
}
