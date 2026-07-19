package com.eggapp.field.ui.batch

import android.app.Application
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
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
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.data.local.CandlingEntity
import com.eggapp.field.ui.components.MutedText
import com.eggapp.field.ui.components.PillTone
import com.eggapp.field.ui.components.StatusPill

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
                    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
                        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                            Button(enabled = !state.settingBatch, onClick = viewModel::setBatch) {
                                Text(if (state.settingBatch) "Starting…" else "Eggs are set — start incubation")
                            }
                            MutedText("Fixes the lockdown and expected-hatch dates from the species schedule.")
                        }
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
