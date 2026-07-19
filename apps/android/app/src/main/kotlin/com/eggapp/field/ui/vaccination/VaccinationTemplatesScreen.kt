package com.eggapp.field.ui.vaccination

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.data.VaccinationTemplateItem
import com.eggapp.field.data.VaccinationTemplateItemRequest
import com.eggapp.field.ui.components.DropdownField
import com.eggapp.field.ui.components.MutedText

private val PURPOSE_OPTIONS = listOf("layer" to "Layer", "broiler" to "Broiler", "breeder" to "Breeder")

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun VaccinationTemplatesScreen(viewModel: VaccinationTemplatesViewModel = viewModel(), onBack: () -> Unit) {
    val state by viewModel.state.collectAsState()
    var editing by remember { mutableStateOf<VaccinationTemplateItem?>(null) }
    var showForm by remember { mutableStateOf(false) }
    var deleteTarget by remember { mutableStateOf<VaccinationTemplateItem?>(null) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Vaccination templates") },
                navigationIcon = { IconButton(onClick = onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back") } },
                actions = {
                    TextButton(onClick = { editing = null; showForm = !showForm }) { Text(if (showForm) "Cancel" else "Add") }
                },
            )
        },
    ) { padding ->
        deleteTarget?.let { target ->
            AlertDialog(
                onDismissRequest = { deleteTarget = null },
                title = { Text("Delete this template item?") },
                confirmButton = {
                    TextButton(
                        onClick = { viewModel.deleteTemplate(target.id); deleteTarget = null },
                        colors = ButtonDefaults.textButtonColors(contentColor = MaterialTheme.colorScheme.error),
                    ) { Text("Delete") }
                },
                dismissButton = { TextButton(onClick = { deleteTarget = null }) { Text("Cancel") } },
            )
        }

        LazyColumn(modifier = Modifier.padding(padding).padding(16.dp)) {
            item {
                if (state.loading) CircularProgressIndicator()
                state.error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            }

            if (showForm) {
                item {
                    TemplateForm(
                        editing = editing,
                        species = state.species,
                        saving = state.saving,
                        onSave = { body ->
                            val current = editing
                            if (current != null) {
                                viewModel.updateTemplate(current.id, body) { showForm = false; editing = null }
                            } else {
                                viewModel.createTemplate(body) { showForm = false }
                            }
                        },
                    )
                }
            }

            if (!state.loading && state.templates.isEmpty()) {
                item { MutedText("No templates yet — add one above.") }
            }

            items(state.templates) { item ->
                TemplateRow(
                    item = item,
                    speciesName = state.species.firstOrNull { it.id == item.speciesId }?.name ?: item.speciesId,
                    onEdit = { editing = item; showForm = true },
                    onDelete = { deleteTarget = item },
                )
            }
        }
    }
}

@Composable
private fun TemplateForm(
    editing: VaccinationTemplateItem?,
    species: List<com.eggapp.field.data.Species>,
    saving: Boolean,
    onSave: (VaccinationTemplateItemRequest) -> Unit,
) {
    val key = editing?.id ?: "create"
    var speciesId by remember(key) { mutableStateOf(editing?.speciesId ?: species.firstOrNull()?.id ?: "") }
    var purpose by remember(key) { mutableStateOf(editing?.purpose ?: "layer") }
    var ageDaysFrom by remember(key) { mutableStateOf(editing?.ageDaysFrom?.toString() ?: "") }
    var ageDaysTo by remember(key) { mutableStateOf(editing?.ageDaysTo?.toString() ?: "") }
    var vaccine by remember(key) { mutableStateOf(editing?.vaccine ?: "") }
    var disease by remember(key) { mutableStateOf(editing?.disease ?: "") }
    var route by remember(key) { mutableStateOf(editing?.route ?: "") }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(if (editing != null) "Edit template item" else "Add template item", style = MaterialTheme.typography.titleMedium)
            DropdownField("Species", speciesId, species.map { it.id to it.name }, { speciesId = it }, Modifier.fillMaxWidth())
            DropdownField("Purpose", purpose, PURPOSE_OPTIONS, { purpose = it }, Modifier.fillMaxWidth())
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    ageDaysFrom, { ageDaysFrom = it }, label = { Text("Age from (days)") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier.weight(1f),
                )
                OutlinedTextField(
                    ageDaysTo, { ageDaysTo = it }, label = { Text("Age to (days)") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    modifier = Modifier.weight(1f),
                )
            }
            OutlinedTextField(vaccine, { vaccine = it }, label = { Text("Vaccine") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(disease, { disease = it }, label = { Text("Disease") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(route, { route = it }, label = { Text("Route") }, modifier = Modifier.fillMaxWidth())
            Button(
                enabled = !saving && speciesId.isNotBlank(),
                onClick = {
                    val from = ageDaysFrom.toIntOrNull()
                    val to = ageDaysTo.toIntOrNull()
                    if (from != null && to != null && vaccine.isNotBlank() && disease.isNotBlank() && route.isNotBlank()) {
                        onSave(VaccinationTemplateItemRequest(speciesId, purpose, from, to, vaccine, disease, route))
                    }
                },
            ) { Text(if (saving) "Saving…" else if (editing != null) "Save changes" else "Add item") }
        }
    }
}

@Composable
private fun TemplateRow(item: VaccinationTemplateItem, speciesName: String, onEdit: () -> Unit, onDelete: () -> Unit) {
    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text("$speciesName · ${item.purpose}", style = MaterialTheme.typography.titleSmall)
            MutedText("day ${item.ageDaysFrom}–${item.ageDaysTo} · ${item.vaccine} for ${item.disease} (${item.route})")
            Row(modifier = Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.End, verticalAlignment = Alignment.CenterVertically) {
                TextButton(onClick = onEdit) { Text("Edit") }
                TextButton(onClick = onDelete, colors = ButtonDefaults.textButtonColors(contentColor = MaterialTheme.colorScheme.error)) { Text("Delete") }
            }
        }
    }
}
