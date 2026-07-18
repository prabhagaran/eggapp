package com.eggapp.field.ui.flocks

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
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.data.local.FeedLogEntity
import com.eggapp.field.data.local.MortalityEntity
import com.eggapp.field.data.local.VaccinationEntity
import com.eggapp.field.data.local.WaterLogEntity
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
fun FlockDetailScreen(flockId: String, onBack: () -> Unit) {
    val application = androidx.compose.ui.platform.LocalContext.current.applicationContext as android.app.Application
    val viewModel: FlockDetailViewModel = viewModel(factory = FlockDetailViewModel.Factory(application, flockId))
    val state by viewModel.state.collectAsState()

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(state.flock?.name ?: "Flock") },
                navigationIcon = { IconButton(onClick = onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back") } },
            )
        },
    ) { padding ->
        LazyColumn(modifier = Modifier.padding(padding).padding(16.dp)) {
            item {
                if (state.loading) CircularProgressIndicator()
                state.flock?.let { flock ->
                    Text(
                        "${flock.currentCount} birds · ${flock.ageDays ?: "—"} days old · ${STAGE_LABEL[flock.stage] ?: flock.stage ?: "—"}",
                        style = MaterialTheme.typography.bodyLarge,
                    )
                }
                state.saveError?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            }

            item { MortalityForm(onSave = viewModel::recordMortality) }
            if (state.localMortality.isNotEmpty()) {
                item { Text("Not yet synced", style = MaterialTheme.typography.titleSmall) }
                items(state.localMortality) { MortalityRow(it) }
            }

            item {
                VaccinationForm(
                    complianceLabel = state.compliance.firstOrNull { it.status == "due" || it.status == "overdue" }
                        ?.let { "Due: ${it.vaccine} (${it.status})" },
                    onSave = viewModel::recordVaccination,
                )
            }
            if (state.localVaccination.isNotEmpty()) {
                item { Text("Not yet synced", style = MaterialTheme.typography.titleSmall) }
                items(state.localVaccination) { VaccinationRow(it) }
            }

            item { FeedWaterForm(onSaveFeed = viewModel::recordFeed, onSaveWater = viewModel::recordWater) }
            if (state.localFeed.isNotEmpty() || state.localWater.isNotEmpty()) {
                item { Text("Not yet synced", style = MaterialTheme.typography.titleSmall) }
                items(state.localFeed) { FeedRow(it) }
                items(state.localWater) { WaterRow(it) }
            }
        }
    }
}

@Composable
private fun MortalityForm(onSave: (String, Int, String, String?) -> Unit) {
    var date by remember { mutableStateOf(LocalDate.now().toString()) }
    var count by remember { mutableStateOf("") }
    var cause by remember { mutableStateOf("death") }
    var notes by remember { mutableStateOf("") }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Mortality / cull / sale", style = MaterialTheme.typography.titleMedium)
            OutlinedTextField(date, { date = it }, label = { Text("Date (YYYY-MM-DD)") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(
                count, { count = it }, label = { Text("Count") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth(),
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf("death", "cull", "sale").forEach { option ->
                    Button(onClick = { cause = option }, enabled = cause != option) { Text(option) }
                }
            }
            OutlinedTextField(notes, { notes = it }, label = { Text("Notes (optional)") }, modifier = Modifier.fillMaxWidth())
            Button(onClick = {
                val n = count.toIntOrNull()
                if (n != null && n > 0) {
                    onSave(date, n, cause, notes.ifBlank { null })
                    count = ""; notes = ""
                }
            }) { Text("Record (works offline)") }
        }
    }
}

@Composable
private fun VaccinationForm(
    complianceLabel: String?,
    onSave: (String?, String, String, String, String, Int, String, String?, String?, String?, String?) -> Unit,
) {
    var date by remember { mutableStateOf(LocalDate.now().toString()) }
    var vaccine by remember { mutableStateOf("") }
    var disease by remember { mutableStateOf("") }
    var route by remember { mutableStateOf("") }
    var count by remember { mutableStateOf("") }
    var administeredBy by remember { mutableStateOf("") }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Record vaccination", style = MaterialTheme.typography.titleMedium)
            complianceLabel?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            OutlinedTextField(date, { date = it }, label = { Text("Date (YYYY-MM-DD)") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(vaccine, { vaccine = it }, label = { Text("Vaccine") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(disease, { disease = it }, label = { Text("Disease") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(route, { route = it }, label = { Text("Route") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(
                count, { count = it }, label = { Text("Count") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(administeredBy, { administeredBy = it }, label = { Text("Administered by") }, modifier = Modifier.fillMaxWidth())
            Button(onClick = {
                val n = count.toIntOrNull()
                if (n != null && vaccine.isNotBlank() && disease.isNotBlank() && route.isNotBlank() && administeredBy.isNotBlank()) {
                    onSave(null, date, vaccine, disease, route, n, administeredBy, null, null, null, null)
                    vaccine = ""; disease = ""; route = ""; count = ""; administeredBy = ""
                }
            }) { Text("Record (works offline)") }
        }
    }
}

@Composable
private fun FeedWaterForm(onSaveFeed: (String, Double) -> Unit, onSaveWater: (Double) -> Unit) {
    var feedType by remember { mutableStateOf("") }
    var feedKg by remember { mutableStateOf("") }
    var waterL by remember { mutableStateOf("") }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Feed & water check", style = MaterialTheme.typography.titleMedium)
            OutlinedTextField(feedType, { feedType = it }, label = { Text("Feed type") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(
                feedKg, { feedKg = it }, label = { Text("Feed (kg)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                modifier = Modifier.fillMaxWidth(),
            )
            Button(onClick = {
                val kg = feedKg.toDoubleOrNull()
                if (kg != null && kg > 0 && feedType.isNotBlank()) {
                    onSaveFeed(feedType, kg)
                    feedType = ""; feedKg = ""
                }
            }) { Text("Log feed (works offline)") }

            OutlinedTextField(
                waterL, { waterL = it }, label = { Text("Water (L)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                modifier = Modifier.fillMaxWidth(),
            )
            Button(onClick = {
                val l = waterL.toDoubleOrNull()
                if (l != null && l > 0) {
                    onSaveWater(l)
                    waterL = ""
                }
            }) { Text("Log water (works offline)") }
        }
    }
}

private fun rowTone(status: String) = if (status == "conflict") PillTone.Danger else if (status == "synced") PillTone.Ok else PillTone.Neutral

@Composable
private fun MortalityRow(r: MortalityEntity) {
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = androidx.compose.ui.Alignment.CenterVertically,
    ) {
        Text("${r.date} · ${r.cause} ${r.count}")
        StatusPill(r.status, rowTone(r.status))
    }
}

@Composable
private fun VaccinationRow(r: VaccinationEntity) {
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = androidx.compose.ui.Alignment.CenterVertically,
    ) {
        Text("${r.date} · ${r.vaccine}")
        StatusPill(r.status, rowTone(r.status))
    }
}

@Composable
private fun FeedRow(r: FeedLogEntity) {
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = androidx.compose.ui.Alignment.CenterVertically,
    ) {
        Text("Feed: ${r.feedType} ${r.quantityKg}kg")
        StatusPill(r.status, rowTone(r.status))
    }
}

@Composable
private fun WaterRow(r: WaterLogEntity) {
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = androidx.compose.ui.Alignment.CenterVertically,
    ) {
        Text("Water: ${r.quantityLiters}L")
        StatusPill(r.status, rowTone(r.status))
    }
}
