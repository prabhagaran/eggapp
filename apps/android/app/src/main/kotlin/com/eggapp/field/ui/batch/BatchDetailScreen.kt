package com.eggapp.field.ui.batch

import android.app.Application
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DatePicker
import androidx.compose.material3.DatePickerDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TimePicker
import androidx.compose.material3.TimePickerState
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.rememberDatePickerState
import androidx.compose.material3.rememberTimePickerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
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
import com.eggapp.field.data.BatchDetail
import com.eggapp.field.data.BatchEggSourceInput
import com.eggapp.field.data.Collection
import com.eggapp.field.data.Incubator
import com.eggapp.field.data.Species
import com.eggapp.field.data.local.CandlingEntity
import com.eggapp.field.ui.components.DropdownField
import com.eggapp.field.ui.components.MutedText
import com.eggapp.field.ui.components.PillTone
import com.eggapp.field.ui.components.StatusPill
import java.text.SimpleDateFormat
import java.time.Duration
import java.time.Instant
import java.time.LocalTime
import java.time.ZoneId
import java.util.Locale
import java.util.TimeZone

private fun batchTone(status: String) = when (status) {
    "completed", "hatching" -> PillTone.Ok
    "aborted", "closed" -> PillTone.Neutral
    else -> PillTone.Accent
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun BatchDetailScreen(batchId: String, onBack: () -> Unit) {
    val application = androidx.compose.ui.platform.LocalContext.current.applicationContext as Application
    val viewModel: BatchDetailViewModel = viewModel(factory = BatchDetailViewModel.Factory(application, batchId))
    val state by viewModel.state.collectAsState()

    LaunchedEffect(state.deleted) {
        if (state.deleted) onBack()
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(state.batch?.let { "${it.species?.name ?: it.speciesId}" } ?: "Batch") },
                navigationIcon = {
                    IconButton(onClick = onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back") }
                },
            )
        },
    ) { padding ->
        LazyColumn(modifier = Modifier.padding(padding).padding(16.dp)) {
            item {
                if (state.loading) CircularProgressIndicator()
                state.batch?.let { batch ->
                    Row(verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
                        StatusPill(batch.status, batchTone(batch.status))
                        MutedText("${batch.viableCount} viable", modifier = Modifier.padding(start = 8.dp))
                    }
                }
                state.saveError?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            }

            if (state.batch?.status in listOf("planned", "setting")) {
                item {
                    SetIncubationCard(
                        settingBatch = state.settingBatch,
                        onSet = { setAtMillis -> viewModel.setBatch(isoStringFromMillis(setAtMillis)) },
                    )
                }
            }

            // Edit: planned or incubating (server allows correcting a live
            // batch's incubator/species/sources; recomputes the schedule if
            // species changes). Delete: planned or aborted only — an
            // incubating batch has real eggs in a device, so it's aborted
            // (status transition) instead of hard-deleted.
            val canEdit = state.batch?.status in listOf("planned", "incubating")
            val canDelete = state.batch?.status in listOf("planned", "aborted")
            if (canEdit || canDelete) {
                if (state.editDetail == null) {
                    item {
                        EditDeleteCard(
                            canEdit = canEdit,
                            canDelete = canDelete,
                            deleting = state.deleting,
                            onEdit = { viewModel.loadEditData() },
                            onDelete = { viewModel.deleteBatch() },
                        )
                    }
                } else {
                    item {
                        EditBatchForm(
                            detail = state.editDetail!!,
                            species = state.editSpecies,
                            incubators = state.editIncubators,
                            collections = state.editCollections,
                            saving = state.savingEdit,
                            onSave = { incubatorId, speciesId, sources, note ->
                                viewModel.updateBatch(incubatorId, speciesId, sources, note) {}
                            },
                            onCancel = { viewModel.cancelEdit() },
                        )
                    }
                }
            }

            if (state.batch?.status == "incubating") {
                item { CandlingForm(nextDay = suggestNextDay(state.batch?.candlingDays, state.localCandlings)) { day, f, c, b, u, note ->
                    viewModel.recordCandling(day, f, c, b, u, note)
                } }
            }

            if (state.batch?.status in listOf("lockdown", "hatching") && state.localHatch == null) {
                item { HatchForm { h, p, d, u, note -> viewModel.recordHatch(h, p, d, u, note) } }
            }

            item { Text("Recorded candling sessions", style = MaterialTheme.typography.titleMedium) }
            items(state.localCandlings) { session -> CandlingRow(session) }
        }
    }
}

private fun isoStringFromMillis(millis: Long): String {
    val fmt = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US)
    fmt.timeZone = TimeZone.getTimeZone("UTC")
    return fmt.format(java.util.Date(millis))
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SetIncubationCard(settingBatch: Boolean, onSet: (Long) -> Unit) {
    var selectedMillis by remember { mutableStateOf(System.currentTimeMillis()) }
    var showDatePicker by remember { mutableStateOf(false) }
    var showTimePicker by remember { mutableStateOf(false) }

    val zone = ZoneId.systemDefault()
    val displayFmt = remember { SimpleDateFormat("MMM d, yyyy 'at' h:mm a", Locale.getDefault()) }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Set at", style = MaterialTheme.typography.titleMedium)
            MutedText(displayFmt.format(java.util.Date(selectedMillis)))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { showDatePicker = true }) { Text("Change date") }
                OutlinedButton(onClick = { showTimePicker = true }) { Text("Change time") }
                OutlinedButton(onClick = { selectedMillis = System.currentTimeMillis() }) { Text("Now") }
            }
            Button(enabled = !settingBatch, onClick = { onSet(selectedMillis) }) {
                Text(if (settingBatch) "Starting…" else "Eggs are set — start incubation")
            }
            MutedText("Fixes the lockdown and expected-hatch dates from the species schedule.")
        }
    }

    if (showDatePicker) {
        val datePickerState = rememberDatePickerState(initialSelectedDateMillis = selectedMillis)
        DatePickerDialog(
            onDismissRequest = { showDatePicker = false },
            confirmButton = {
                TextButton(onClick = {
                    val pickedUtcMillis = datePickerState.selectedDateMillis
                    if (pickedUtcMillis != null) {
                        val pickedDate = Instant.ofEpochMilli(pickedUtcMillis).atZone(ZoneId.of("UTC")).toLocalDate()
                        val currentTime = Instant.ofEpochMilli(selectedMillis).atZone(zone).toLocalTime()
                        selectedMillis = pickedDate.atTime(currentTime).atZone(zone).toInstant().toEpochMilli()
                    }
                    showDatePicker = false
                }) { Text("OK") }
            },
            dismissButton = { TextButton(onClick = { showDatePicker = false }) { Text("Cancel") } },
        ) { DatePicker(state = datePickerState) }
    }

    if (showTimePicker) {
        val currentTime = Instant.ofEpochMilli(selectedMillis).atZone(zone).toLocalTime()
        val timePickerState = rememberTimePickerState(initialHour = currentTime.hour, initialMinute = currentTime.minute)
        TimePickerDialog(
            onDismissRequest = { showTimePicker = false },
            onConfirm = {
                val currentDate = Instant.ofEpochMilli(selectedMillis).atZone(zone).toLocalDate()
                selectedMillis = currentDate
                    .atTime(LocalTime.of(timePickerState.hour, timePickerState.minute))
                    .atZone(zone).toInstant().toEpochMilli()
                showTimePicker = false
            },
            timePickerState = timePickerState,
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun TimePickerDialog(
    onDismissRequest: () -> Unit,
    onConfirm: () -> Unit,
    timePickerState: TimePickerState,
) {
    androidx.compose.material3.AlertDialog(
        onDismissRequest = onDismissRequest,
        confirmButton = { TextButton(onClick = onConfirm) { Text("OK") } },
        dismissButton = { TextButton(onClick = onDismissRequest) { Text("Cancel") } },
        text = { TimePicker(state = timePickerState) },
    )
}

@Composable
private fun EditDeleteCard(
    canEdit: Boolean,
    canDelete: Boolean,
    deleting: Boolean,
    onEdit: () -> Unit,
    onDelete: () -> Unit,
) {
    var showConfirm by remember { mutableStateOf(false) }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (canEdit) {
                    OutlinedButton(onClick = onEdit) { Text("Edit batch") }
                }
                if (canDelete) {
                    OutlinedButton(enabled = !deleting, onClick = { showConfirm = true }) {
                        Text(if (deleting) "Deleting…" else "Delete batch")
                    }
                }
            }
            MutedText(
                if (canEdit && !canDelete) "Editing a live batch changes its incubator/species/source assignment — the schedule is recomputed if species changes."
                else if (canDelete && !canEdit) "Deleting an aborted batch permanently removes its record."
                else "Only possible before eggs are set — editing after that would invalidate the computed schedule.",
            )
        }
    }

    if (showConfirm) {
        AlertDialog(
            onDismissRequest = { showConfirm = false },
            title = { Text("Delete this batch?") },
            text = { Text("Its egg sources are released back to their collections. This can't be undone.") },
            confirmButton = {
                TextButton(onClick = { showConfirm = false; onDelete() }) { Text("Delete") }
            },
            dismissButton = { TextButton(onClick = { showConfirm = false }) { Text("Cancel") } },
        )
    }
}

private fun collectionAgeDays(collectedOn: String): Long =
    Duration.between(Instant.parse(collectedOn), Instant.now()).toDays()

@Composable
private fun EditBatchForm(
    detail: BatchDetail,
    species: List<Species>,
    incubators: List<Incubator>,
    collections: List<Collection>,
    saving: Boolean,
    onSave: (String, String, List<BatchEggSourceInput>, String?) -> Unit,
    onCancel: () -> Unit,
) {
    var incubatorId by remember(detail.id) { mutableStateOf(detail.incubatorId) }
    var speciesId by remember(detail.id) { mutableStateOf(detail.speciesId) }
    var overrideNote by remember(detail.id) { mutableStateOf("") }
    // Pre-filled from this batch's current sources so re-saving without
    // changes is a no-op, not an accidental egg-count wipe.
    val picked = remember(detail.id) {
        mutableStateMapOf<String, String>().apply {
            detail.sources.forEach { put(it.collectionId, it.count.toString()) }
        }
    }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Edit batch", style = MaterialTheme.typography.titleMedium)
            DropdownField("Incubator", incubatorId, incubators.map { it.id to it.name }, { incubatorId = it }, Modifier.fillMaxWidth())
            DropdownField("Species", speciesId, species.map { it.id to it.name }, { speciesId = it }, Modifier.fillMaxWidth())

            Text("Egg collections", style = MaterialTheme.typography.titleSmall)
            val oldEggsPicked = collections.any { c ->
                val n = picked[c.id]?.toIntOrNull() ?: 0
                n > 0 && collectionAgeDays(c.collectedOn) > 14
            }
            // This batch's own current usage is already subtracted out of
            // availableCount server-side — add it back so editing
            // (including just re-saving the same count) isn't blocked by
            // the batch's own prior assignment. Also surfaces collections
            // this batch already uses even if nothing else is available.
            collections
                .map { c ->
                    val own = detail.sources.find { it.collectionId == c.id }?.count ?: 0
                    c to (c.availableCount + own)
                }
                .filter { (_, effectiveAvailable) -> effectiveAvailable > 0 }
                .forEach { (c, effectiveAvailable) ->
                    val age = collectionAgeDays(c.collectedOn)
                    Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(c.collectedOn.take(10))
                            MutedText("$age d old · $effectiveAvailable available")
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
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
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
                ) { Text(if (saving) "Saving…" else "Save changes") }
                OutlinedButton(onClick = onCancel) { Text("Cancel") }
            }
        }
    }
}

private fun suggestNextDay(scheduleDays: List<Int>?, recorded: List<CandlingEntity>): Int {
    val done = recorded.map { it.dayNo }.toSet()
    return scheduleDays?.firstOrNull { it !in done } ?: (scheduleDays?.firstOrNull() ?: 7)
}

@Composable
private fun CandlingForm(nextDay: Int, onSave: (Int, Int, Int, Int, Int, String?) -> Unit) {
    var day by remember(nextDay) { mutableStateOf(nextDay.toString()) }
    var fertile by remember { mutableStateOf("") }
    var clear by remember { mutableStateOf("") }
    var bloodRing by remember { mutableStateOf("") }
    var unsure by remember { mutableStateOf("") }
    var note by remember { mutableStateOf("") }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Record candling", style = MaterialTheme.typography.titleMedium)
            NumberField("Day", day) { day = it }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                NumberField("Fertile", fertile, Modifier.fillMaxWidth()) { fertile = it }
            }
            NumberField("Clear", clear) { clear = it }
            NumberField("Blood ring", bloodRing) { bloodRing = it }
            NumberField("Unsure", unsure) { unsure = it }
            OutlinedTextField(note, { note = it }, label = { Text("Discrepancy note (if counts don't match)") }, modifier = Modifier.fillMaxWidth())
            Button(onClick = {
                onSave(
                    day.toIntOrNull() ?: 0,
                    fertile.toIntOrNull() ?: 0,
                    clear.toIntOrNull() ?: 0,
                    bloodRing.toIntOrNull() ?: 0,
                    unsure.toIntOrNull() ?: 0,
                    note.ifBlank { null },
                )
                fertile = ""; clear = ""; bloodRing = ""; unsure = ""; note = ""
            }) { Text("Save (works offline)") }
        }
    }
}

@Composable
private fun HatchForm(onSave: (Int, Int, Int, Int, String?) -> Unit) {
    var hatched by remember { mutableStateOf("") }
    var pippedDead by remember { mutableStateOf("") }
    var deadInShell by remember { mutableStateOf("") }
    var unhatched by remember { mutableStateOf("") }
    var note by remember { mutableStateOf("") }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Record hatch outcome", style = MaterialTheme.typography.titleMedium)
            NumberField("Hatched", hatched) { hatched = it }
            NumberField("Pipped, died", pippedDead) { pippedDead = it }
            NumberField("Dead in shell", deadInShell) { deadInShell = it }
            NumberField("Unhatched", unhatched) { unhatched = it }
            OutlinedTextField(note, { note = it }, label = { Text("Discrepancy note (if counts don't match)") }, modifier = Modifier.fillMaxWidth())
            Button(onClick = {
                onSave(
                    hatched.toIntOrNull() ?: 0,
                    pippedDead.toIntOrNull() ?: 0,
                    deadInShell.toIntOrNull() ?: 0,
                    unhatched.toIntOrNull() ?: 0,
                    note.ifBlank { null },
                )
            }) { Text("Save (works offline) — completes the batch") }
        }
    }
}

@Composable
private fun NumberField(label: String, value: String, modifier: Modifier = Modifier, onChange: (String) -> Unit) {
    OutlinedTextField(
        value = value,
        onValueChange = onChange,
        label = { Text(label) },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
        modifier = modifier.fillMaxWidth(),
    )
}

@Composable
private fun CandlingRow(session: CandlingEntity) {
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = androidx.compose.ui.Alignment.CenterVertically,
    ) {
        Text("Day ${session.dayNo}")
        MutedText("fertile ${session.fertile}, clear ${session.clear}")
        StatusPill(session.status, if (session.status == "synced") PillTone.Ok else if (session.status == "conflict") PillTone.Danger else PillTone.Neutral)
    }
}
