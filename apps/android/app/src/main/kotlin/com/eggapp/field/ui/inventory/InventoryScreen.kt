package com.eggapp.field.ui.inventory

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
import com.eggapp.field.data.CreateInventoryItemRequest
import com.eggapp.field.data.InventoryItem
import com.eggapp.field.data.UpdateInventoryItemRequest
import com.eggapp.field.ui.components.DropdownField
import com.eggapp.field.ui.components.MutedText
import com.eggapp.field.ui.components.PillTone
import com.eggapp.field.ui.components.StatusPill

private val KIND_LABEL = mapOf("feed" to "Feed", "vaccine" to "Vaccine", "consumable" to "Consumable")
private val KIND_OPTIONS = listOf("feed" to "Feed", "vaccine" to "Vaccine", "consumable" to "Consumable")

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun InventoryScreen(viewModel: InventoryViewModel = viewModel(), onBack: () -> Unit) {
    val state by viewModel.state.collectAsState()
    var showCreate by remember { mutableStateOf(false) }
    var editing by remember { mutableStateOf<InventoryItem?>(null) }
    var deleteTarget by remember { mutableStateOf<InventoryItem?>(null) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Inventory") },
                navigationIcon = { IconButton(onClick = onBack) { Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back") } },
                actions = {
                    TextButton(onClick = { showCreate = !showCreate }) { Text(if (showCreate) "Cancel" else "Add") }
                },
            )
        },
    ) { padding ->
        deleteTarget?.let { target ->
            AlertDialog(
                onDismissRequest = { deleteTarget = null },
                title = { Text("Delete \"${target.name}\"?") },
                confirmButton = {
                    TextButton(
                        onClick = { viewModel.deleteItem(target.id); deleteTarget = null },
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

            if (showCreate) {
                item {
                    CreateForm(saving = state.saving, onSave = { body -> viewModel.createItem(body) { showCreate = false } })
                }
            }

            if (!state.loading && state.items.isEmpty()) {
                item { MutedText("No inventory items yet — add one above.") }
            }

            items(state.items) { item ->
                if (editing?.id == item.id) {
                    EditForm(
                        item = item,
                        saving = state.saving,
                        onSave = { body -> viewModel.updateItem(item.id, body) { editing = null } },
                        onCancel = { editing = null },
                    )
                } else {
                    ItemRow(item = item, onEdit = { editing = item }, onDelete = { deleteTarget = item })
                }
            }
        }
    }
}

@Composable
private fun CreateForm(saving: Boolean, onSave: (CreateInventoryItemRequest) -> Unit) {
    var kind by remember { mutableStateOf("feed") }
    var name by remember { mutableStateOf("") }
    var unit by remember { mutableStateOf("") }
    var quantity by remember { mutableStateOf("0") }
    var lotNumber by remember { mutableStateOf("") }
    var expiry by remember { mutableStateOf("") }
    var lowStockThreshold by remember { mutableStateOf("") }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Add inventory item", style = MaterialTheme.typography.titleMedium)
            DropdownField("Kind", kind, KIND_OPTIONS, { kind = it }, Modifier.fillMaxWidth())
            OutlinedTextField(name, { name = it }, label = { Text("Name") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(unit, { unit = it }, label = { Text("Unit (kg, doses, units...)") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(
                quantity, { quantity = it }, label = { Text("Starting quantity") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                lowStockThreshold, { lowStockThreshold = it }, label = { Text("Low-stock threshold (optional)") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                modifier = Modifier.fillMaxWidth(),
            )
            if (kind == "vaccine") {
                OutlinedTextField(lotNumber, { lotNumber = it }, label = { Text("Lot number") }, modifier = Modifier.fillMaxWidth())
                OutlinedTextField(expiry, { expiry = it }, label = { Text("Expiry (YYYY-MM-DD)") }, modifier = Modifier.fillMaxWidth())
            }
            Button(
                enabled = !saving && name.isNotBlank() && unit.isNotBlank(),
                onClick = {
                    val q = quantity.toDoubleOrNull()
                    if (q != null && (kind != "vaccine" || (lotNumber.isNotBlank() && expiry.isNotBlank()))) {
                        onSave(
                            CreateInventoryItemRequest(
                                kind = kind,
                                name = name,
                                unit = unit,
                                quantity = q,
                                lotNumber = lotNumber.ifBlank { null },
                                expiry = expiry.ifBlank { null },
                                lowStockThreshold = lowStockThreshold.toDoubleOrNull(),
                            ),
                        )
                    }
                },
            ) { Text(if (saving) "Saving…" else "Add item") }
        }
    }
}

@Composable
private fun EditForm(item: InventoryItem, saving: Boolean, onSave: (UpdateInventoryItemRequest) -> Unit, onCancel: () -> Unit) {
    var name by remember(item.id) { mutableStateOf(item.name) }
    var unit by remember(item.id) { mutableStateOf(item.unit) }
    var lotNumber by remember(item.id) { mutableStateOf(item.lotNumber ?: "") }
    var expiry by remember(item.id) { mutableStateOf(item.expiry?.take(10) ?: "") }
    var lowStockThreshold by remember(item.id) { mutableStateOf(item.lowStockThreshold?.toString() ?: "") }

    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Edit ${item.name}", style = MaterialTheme.typography.titleMedium)
            OutlinedTextField(name, { name = it }, label = { Text("Name") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(unit, { unit = it }, label = { Text("Unit") }, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(
                lowStockThreshold, { lowStockThreshold = it }, label = { Text("Low-stock threshold") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                modifier = Modifier.fillMaxWidth(),
            )
            if (item.kind == "vaccine") {
                OutlinedTextField(lotNumber, { lotNumber = it }, label = { Text("Lot number") }, modifier = Modifier.fillMaxWidth())
                OutlinedTextField(expiry, { expiry = it }, label = { Text("Expiry (YYYY-MM-DD)") }, modifier = Modifier.fillMaxWidth())
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(
                    enabled = !saving && name.isNotBlank() && unit.isNotBlank(),
                    onClick = {
                        onSave(
                            UpdateInventoryItemRequest(
                                name = name,
                                unit = unit,
                                lotNumber = lotNumber.ifBlank { null },
                                expiry = expiry.ifBlank { null },
                                lowStockThreshold = lowStockThreshold.toDoubleOrNull(),
                            ),
                        )
                    },
                ) { Text(if (saving) "Saving…" else "Save") }
                TextButton(onClick = onCancel) { Text("Cancel") }
            }
        }
    }
}

@Composable
private fun ItemRow(item: InventoryItem, onEdit: () -> Unit, onDelete: () -> Unit) {
    val low = item.lowStockThreshold != null && item.quantity <= item.lowStockThreshold
    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Column(modifier = Modifier.padding(16.dp)) {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Column {
                    Text(item.name, style = MaterialTheme.typography.titleSmall)
                    MutedText(KIND_LABEL[item.kind] ?: item.kind)
                }
                StatusPill("${item.quantity} ${item.unit}", if (low) PillTone.Warn else PillTone.Ok)
            }
            item.lotNumber?.let { MutedText("lot $it", modifier = Modifier.padding(top = 4.dp)) }
            item.expiry?.let { MutedText("expires ${it.take(10)}") }
            Row(modifier = Modifier.fillMaxWidth().padding(top = 8.dp), horizontalArrangement = Arrangement.End) {
                TextButton(onClick = onEdit) { Text("Edit") }
                TextButton(onClick = onDelete, colors = ButtonDefaults.textButtonColors(contentColor = MaterialTheme.colorScheme.error)) { Text("Delete") }
            }
        }
    }
}
