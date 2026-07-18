package com.eggapp.field.ui.flocks

import androidx.compose.foundation.layout.Column
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
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.data.Flock

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

    Scaffold(topBar = { TopAppBar(title = { Text("Flocks") }) }) { padding ->
        Column(modifier = Modifier.padding(padding)) {
            if (state.loading) CircularProgressIndicator(modifier = Modifier.padding(16.dp))
            state.error?.let { Text(it, color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(16.dp)) }
            if (!state.loading && state.flocks.isEmpty() && state.error == null) {
                Text("No flocks yet — add one from the web dashboard.", modifier = Modifier.padding(16.dp))
            }
            LazyColumn {
                items(state.flocks) { flock ->
                    Card(modifier = Modifier.fillMaxWidth().padding(12.dp), onClick = { onOpenFlock(flock) }) {
                        Column(modifier = Modifier.padding(16.dp)) {
                            Text(flock.name, style = MaterialTheme.typography.titleMedium)
                            Text(
                                "${flock.species?.name ?: flock.speciesId} · ${flock.purpose} · ${STAGE_LABEL[flock.stage] ?: flock.stage ?: "—"}",
                                style = MaterialTheme.typography.bodyMedium,
                            )
                            Text("${flock.currentCount} birds · ${flock.ageDays ?: "—"} days old", style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
            }
        }
    }
}
