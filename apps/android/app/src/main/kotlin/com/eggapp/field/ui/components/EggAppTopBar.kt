package com.eggapp.field.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.ExpandMore
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp

/** One entry in the top bar's overflow (kebab) menu. */
data class OverflowItem(
    val label: String,
    val icon: ImageVector,
    val badge: Int? = null,
    val onClick: () -> Unit,
)

/**
 * The Android counterpart of the web header (`.topnav` in globals.css):
 * brand block on the left, farm switcher + avatar + kebab on the right, and
 * the accent rule underneath.
 *
 * The kebab carries the destinations that don't earn a bottom-bar slot —
 * the same role the web's "More" menu plays once the tab row stops fitting.
 */
@Composable
fun EggAppTopBar(
    farmName: String?,
    farms: List<Pair<String, String>>, // id to name
    selectedFarmId: String?,
    onSelectFarm: (String) -> Unit,
    userLabel: String,
    overflow: List<OverflowItem>,
    modifier: Modifier = Modifier,
) {
    var farmMenuOpen by remember { mutableStateOf(false) }
    var moreOpen by remember { mutableStateOf(false) }

    Surface(color = MaterialTheme.colorScheme.surface, shadowElevation = 1.dp, modifier = modifier) {
        Column {
            Row(
                modifier = Modifier.fillMaxWidth().statusBarsPadding().padding(horizontal = 14.dp, vertical = 10.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                Box(
                    modifier = Modifier
                        .size(36.dp)
                        .background(MaterialTheme.colorScheme.secondary, RoundedCornerShape(10.dp)),
                    contentAlignment = Alignment.Center,
                ) {
                    Text("🥚", style = MaterialTheme.typography.titleMedium)
                }
                Column(modifier = Modifier.weight(1f)) {
                    Text("eggAPP", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                    Text(
                        "Poultry Farm Management",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.primary,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }

                // Farm switcher. Rendered only when there is somewhere to
                // switch to — a lone farm needs no picker, and the name is
                // already on the dashboard.
                if (farms.size > 1) {
                    Box {
                        Row(
                            modifier = Modifier
                                .background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(10.dp))
                                .clickable { farmMenuOpen = true }
                                .padding(start = 10.dp, end = 6.dp, top = 6.dp, bottom = 6.dp)
                                .widthIn(max = 110.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text(
                                farmName ?: "Farm",
                                style = MaterialTheme.typography.labelLarge,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis,
                                modifier = Modifier.weight(1f, fill = false),
                            )
                            Icon(Icons.Filled.ExpandMore, contentDescription = null, modifier = Modifier.size(18.dp))
                        }
                        DropdownMenu(expanded = farmMenuOpen, onDismissRequest = { farmMenuOpen = false }) {
                            farms.forEach { (id, name) ->
                                DropdownMenuItem(
                                    text = {
                                        Text(
                                            name,
                                            fontWeight = if (id == selectedFarmId) FontWeight.Bold else FontWeight.Normal,
                                        )
                                    },
                                    onClick = { farmMenuOpen = false; onSelectFarm(id) },
                                )
                            }
                        }
                    }
                }

                Box(
                    modifier = Modifier.size(34.dp).background(MaterialTheme.colorScheme.primaryContainer, CircleShape),
                    contentAlignment = Alignment.Center,
                ) {
                    Text(
                        userLabel,
                        style = MaterialTheme.typography.labelLarge,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onPrimaryContainer,
                    )
                }

                Box {
                    IconButton(onClick = { moreOpen = true }, modifier = Modifier.size(34.dp)) {
                        Icon(Icons.Filled.MoreVert, contentDescription = "More")
                    }
                    DropdownMenu(expanded = moreOpen, onDismissRequest = { moreOpen = false }) {
                        overflow.forEachIndexed { index, item ->
                            // Separates the account entry from the
                            // destinations above it.
                            if (index == overflow.lastIndex && overflow.size > 1) HorizontalDivider()
                            DropdownMenuItem(
                                leadingIcon = { Icon(item.icon, contentDescription = null) },
                                text = { Text(item.label) },
                                trailingIcon = item.badge?.takeIf { it > 0 }?.let { count ->
                                    {
                                        Text(
                                            "$count",
                                            style = MaterialTheme.typography.labelSmall,
                                            fontWeight = FontWeight.Bold,
                                            color = Color.White,
                                            modifier = Modifier
                                                .background(MaterialTheme.colorScheme.error, CircleShape)
                                                .padding(horizontal = 7.dp, vertical = 2.dp),
                                        )
                                    }
                                },
                                onClick = { moreOpen = false; item.onClick() },
                            )
                        }
                    }
                }
            }
            // `.topnav`'s 3px accent rule.
            Box(modifier = Modifier.fillMaxWidth().height(3.dp).background(MaterialTheme.colorScheme.primary))
        }
    }
}

/**
 * Header for the screens reached from the kebab or by drilling in — same
 * accent rule and surface as [EggAppTopBar], with a back affordance instead
 * of the brand block.
 */
@Composable
fun EggAppSubBar(
    title: String,
    onBack: () -> Unit,
    modifier: Modifier = Modifier,
    actions: @Composable () -> Unit = {},
) {
    Surface(color = MaterialTheme.colorScheme.surface, shadowElevation = 1.dp, modifier = modifier) {
        Column {
            Row(
                modifier = Modifier.fillMaxWidth().statusBarsPadding().padding(horizontal = 6.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                IconButton(onClick = onBack) {
                    Icon(Icons.Filled.ArrowBack, contentDescription = "Back")
                }
                Text(
                    title,
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f).padding(start = 2.dp),
                )
                actions()
            }
            Box(modifier = Modifier.fillMaxWidth().height(3.dp).background(MaterialTheme.colorScheme.primary))
        }
    }
}
