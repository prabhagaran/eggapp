package com.eggapp.field.ui.flocks

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.RadioButton
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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.data.CreateFlockRequest
import com.eggapp.field.data.Flock
import com.eggapp.field.data.Species
import com.eggapp.field.ui.components.DropdownField
import com.eggapp.field.ui.components.MutedText
import com.eggapp.field.ui.components.PillTone
import com.eggapp.field.ui.components.StatusPill
import java.time.LocalDate

private val STAGE_LABEL = mapOf(
    "brooding" to "Brooding",
    "grower" to "Grower",
    "pre_lay" to "Pre-lay",
    "layer" to "Layer",
    "broiler_starter" to "Broiler starter",
    "broiler_grower" to "Broiler grower",
    "broiler_finisher" to "Broiler finisher",
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FlocksScreen(viewModel: FlocksViewModel = viewModel(), onOpenFlock: (Flock) -> Unit) {
    val state by viewModel.state.collectAsState()
    var showCreate by remember { mutableStateOf(false) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Flocks") },
                actions = { TextButton(onClick = { showCreate = !showCreate }) { Text(if (showCreate) "Cancel" else "Add") } },
            )
        },
    ) { padding ->
        LazyColumn(modifier = Modifier.padding(padding)) {
            item {
                if (state.loading) CircularProgressIndicator(modifier = Modifier.padding(16.dp))
                state.error?.let { Text(it, color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(16.dp)) }
            }
            if (showCreate) {
                item {
                    CreateFlockForm(
                        species = state.species,
                        completedHatches = state.completedHatches,
                        saving = state.saving,
                        onSave = { body -> viewModel.createFlock(body) { showCreate = false } },
                    )
                }
            }
            if (!state.loading && state.flocks.isEmpty() && state.error == null) {
                item { Text("No flocks yet — add one above.", modifier = Modifier.padding(16.dp)) }
            }
            items(state.flocks) { flock ->
                Card(modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp), onClick = { onOpenFlock(flock) }) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Row(verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
                            Text(flock.name, style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
                            StatusPill(STAGE_LABEL[flock.stage] ?: flock.stage ?: "—", PillTone.Accent)
                        }
                        MutedText(
                            "${flock.species?.name ?: flock.speciesId} · ${flock.purpose}",
                            modifier = Modifier.padding(top = 4.dp),
                        )
                        MutedText("${flock.currentCount} birds · ${flock.ageDays ?: "—"} days old")
                    }
                }
            }
        }
    }
}

private val PURPOSE_OPTIONS = listOf("layer" to "Layer", "broiler" to "Broiler", "breeder" to "Breeder")

@Composable
private fun CreateFlockForm(
    species: List<Species>,
    completedHatches: List<CompletedHatchOption>,
    saving: Boolean,
    onSave: (CreateFlockRequest) -> Unit,
) {
    var name by remember { mutableStateOf("") }
    var speciesId by remember { mutableStateOf("") }
    var purpose by remember { mutableStateOf("layer") }
    var placedCount by remember { mutableStateOf("") }
    var fromHatch by remember { mutableStateOf(false) }
    var acquisitionDate by remember { mutableStateOf(LocalDate.now().toString()) }
    var acquisitionAgeDays by remember { mutableStateOf("") }
    var acquisitionNote by remember { mutableStateOf("") }
    var hatchEventId by remember { mutableStateOf(completedHatches.firstOrNull()?.hatchEventId ?: "") }

    Card(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Add flock", style = MaterialTheme.typography.titleMedium)
            OutlinedTextField(name, { name = it }, label = { Text("Name") }, modifier = Modifier.fillMaxWidth())
            DropdownField("Species", speciesId, species.map { it.id to it.name }, { speciesId = it }, Modifier.fillMaxWidth())
            DropdownField("Purpose", purpose, PURPOSE_OPTIONS, { purpose = it }, Modifier.fillMaxWidth())
            OutlinedTextField(
                placedCount, { placedCount = it }, label = { Text("Placed count") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth(),
            )

            Row(verticalAlignment = Alignment.CenterVertically) {
                RadioButton(selected = !fromHatch, onClick = { fromHatch = false })
                Text("Manually acquired", modifier = Modifier.padding(end = 12.dp))
                RadioButton(selected = fromHatch, onClick = { fromHatch = true }, enabled = completedHatches.isNotEmpty())
                Text("From a completed hatch")
            }

            if (fromHatch) {
                DropdownField(
                    "Hatch batch",
                    hatchEventId,
                    completedHatches.map { it.hatchEventId to it.label },
                    { hatchEventId = it },
                    Modifier.fillMaxWidth(),
                )
            } else {
                OutlinedTextField(
                    acquisitionDate, { acquisitionDate = it }, label = { Text("Acquisition date (YYYY-MM-DD)") },
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    acquisitionAgeDays, { acquisitionAgeDays = it }, label = { Text("Age at acquisition (days)") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    acquisitionNote, { acquisitionNote = it }, label = { Text("Provenance note (optional)") },
                    modifier = Modifier.fillMaxWidth(),
                )
            }

            Button(
                enabled = !saving && name.isNotBlank() && speciesId.isNotBlank() &&
                    (fromHatch || acquisitionAgeDays.toIntOrNull() != null),
                onClick = {
                    val count = placedCount.toIntOrNull()
                    if (count == null) return@Button
                    val body = if (fromHatch) {
                        CreateFlockRequest(name, speciesId, purpose, count, hatchEventId = hatchEventId)
                    } else {
                        CreateFlockRequest(
                            name, speciesId, purpose, count,
                            acquisitionDate = "${acquisitionDate}T00:00:00.000Z",
                            acquisitionAgeDays = acquisitionAgeDays.toIntOrNull(),
                            acquisitionNote = acquisitionNote.ifBlank { null },
                        )
                    }
                    onSave(body)
                },
            ) { Text(if (saving) "Saving…" else "Add flock") }
        }
    }
}
