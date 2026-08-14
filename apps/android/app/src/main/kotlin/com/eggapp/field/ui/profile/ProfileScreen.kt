package com.eggapp.field.ui.profile

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ChevronRight
import androidx.compose.material.icons.filled.Inventory
import androidx.compose.material.icons.filled.Vaccines
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.eggapp.field.ui.components.AppCard
import com.eggapp.field.ui.components.EggAppSubBar
import com.eggapp.field.ui.components.HeroCard
import com.eggapp.field.ui.components.SectionRule

@Composable
fun ProfileScreen(
    viewModel: ProfileViewModel = viewModel(),
    onBack: () -> Unit,
    onLoggedOut: () -> Unit,
    onOpenVaccinationTemplates: () -> Unit,
    onOpenInventory: () -> Unit,
) {
    val state by viewModel.state.collectAsState()

    Scaffold(topBar = { EggAppSubBar("Profile", onBack) }) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(MaterialTheme.colorScheme.background)
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 16.dp),
        ) {
            if (state.loading) {
                CircularProgressIndicator(modifier = Modifier.padding(24.dp))
                return@Column
            }

            HeroCard(modifier = Modifier.fillMaxWidth().padding(top = 16.dp)) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(14.dp),
                ) {
                    val label = (state.me?.name?.trim()?.takeIf { it.isNotEmpty() } ?: state.me?.email ?: "?")
                        .take(2).uppercase()
                    Column(
                        modifier = Modifier.size(48.dp)
                            .background(MaterialTheme.colorScheme.primaryContainer, CircleShape),
                        verticalArrangement = Arrangement.Center,
                        horizontalAlignment = Alignment.CenterHorizontally,
                    ) {
                        Text(
                            label,
                            color = MaterialTheme.colorScheme.onPrimaryContainer,
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.Bold,
                        )
                    }
                    Column {
                        Text(
                            state.me?.name?.takeIf { it.isNotBlank() } ?: "—",
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.Bold,
                        )
                        Text(state.me?.email ?: "", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }

            SectionRule("Farm")
            AppCard(modifier = Modifier.fillMaxWidth()) {
                Text(
                    state.farm?.name ?: "—",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold,
                )
                state.farm?.location?.let {
                    Text(it, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                state.farm?.role?.let {
                    Text(
                        "role: $it",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }

            SectionRule("Manage")
            AppCard(
                modifier = Modifier.fillMaxWidth(),
                contentPadding = androidx.compose.foundation.layout.PaddingValues(vertical = 4.dp),
            ) {
                NavRow("Vaccination templates", Icons.Filled.Vaccines, onOpenVaccinationTemplates)
                NavRow("Inventory", Icons.Filled.Inventory, onOpenInventory)
            }

            Button(
                onClick = { viewModel.logout(); onLoggedOut() },
                colors = ButtonDefaults.buttonColors(
                    containerColor = MaterialTheme.colorScheme.errorContainer,
                    contentColor = MaterialTheme.colorScheme.onErrorContainer,
                ),
                modifier = Modifier.fillMaxWidth().padding(top = 24.dp, bottom = 32.dp),
            ) {
                Text("Log out", fontWeight = FontWeight.Bold)
            }
        }
    }
}

@Composable
private fun NavRow(label: String, icon: ImageVector, onClick: () -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth().clickable(onClick = onClick).padding(horizontal = 14.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Icon(icon, contentDescription = null, tint = MaterialTheme.colorScheme.primary, modifier = Modifier.size(20.dp))
        Text(label, style = MaterialTheme.typography.bodyLarge, modifier = Modifier.weight(1f))
        Icon(
            Icons.Filled.ChevronRight,
            contentDescription = null,
            tint = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.size(20.dp),
        )
    }
}
