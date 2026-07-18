package com.eggapp.field.ui.batch

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
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
import com.eggapp.field.data.Batch
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
fun BatchesScreen(viewModel: BatchesViewModel = viewModel(), onOpenBatch: (Batch) -> Unit) {
    val state by viewModel.state.collectAsState()

    Scaffold(topBar = { TopAppBar(title = { Text("Batches") }) }) { padding ->
        Column(modifier = Modifier.padding(padding).padding(horizontal = 16.dp)) {
            if (state.loading) CircularProgressIndicator(modifier = Modifier.padding(16.dp))
            state.error?.let { Text(it, color = MaterialTheme.colorScheme.error, modifier = Modifier.padding(vertical = 8.dp)) }
            if (!state.loading && state.batches.isEmpty() && state.error == null) {
                MutedText("No active batches — start one from the web dashboard.", modifier = Modifier.padding(vertical = 8.dp))
            }
            LazyColumn {
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
}
