package com.eggapp.field.ui.components

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.eggapp.field.ui.theme.LocalStatusColors

// The Compose half of apps/web/app/globals.css. Every primitive here has a
// counterpart class over there (.card, .card.dark, .section-rule, .stat-card,
// .live-band, .sensor-tile) and is deliberately named after it — when one
// side's look changes, the other has an obvious place to follow.

/** `.card.dark` / `.live-band` — the brand gradient, hero-start → hero-end. */
val HeroBrush: Brush
    @Composable get() = Brush.linearGradient(
        listOf(MaterialTheme.colorScheme.secondary, MaterialTheme.colorScheme.primary),
    )

/** `.card` — white surface, hairline border, 22px radius, soft shadow. */
@Composable
fun AppCard(
    modifier: Modifier = Modifier,
    onClick: (() -> Unit)? = null,
    contentPadding: androidx.compose.foundation.layout.PaddingValues =
        androidx.compose.foundation.layout.PaddingValues(horizontal = 18.dp, vertical = 16.dp),
    content: @Composable ColumnScope.() -> Unit,
) {
    Surface(
        modifier = if (onClick != null) modifier.clickable(onClick = onClick) else modifier,
        shape = RoundedCornerShape(22.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline),
        shadowElevation = 1.dp,
    ) {
        Column(modifier = Modifier.padding(contentPadding), content = content)
    }
}

/** `.card.dark` — same shell, brand gradient, white content. */
@Composable
fun HeroCard(
    modifier: Modifier = Modifier,
    onClick: (() -> Unit)? = null,
    content: @Composable ColumnScope.() -> Unit,
) {
    val brush = HeroBrush
    Surface(
        modifier = if (onClick != null) modifier.clickable(onClick = onClick) else modifier,
        shape = RoundedCornerShape(22.dp),
        color = Color.Transparent,
        shadowElevation = 1.dp,
    ) {
        Box(modifier = Modifier.background(brush)) {
            CompositionLocalProvider(LocalContentColor provides Color.White) {
                Column(modifier = Modifier.padding(horizontal = 18.dp, vertical = 16.dp), content = content)
            }
        }
    }
}

/** `.section-rule` — heading with the little accent bar in front of it. */
@Composable
fun SectionRule(text: String, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier.padding(top = 22.dp, bottom = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Box(
            modifier = Modifier
                .width(4.dp).height(17.dp)
                .background(MaterialTheme.colorScheme.primary, RoundedCornerShape(2.dp)),
        )
        Text(text, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
    }
}

/**
 * `.stat-card` — label + icon on top, oversized value, caption underneath.
 * `dark = true` is the web's `.card.dark.stat-card`, used for the one
 * headline figure per row.
 */
@Composable
fun StatCard(
    label: String,
    value: String,
    caption: String,
    icon: ImageVector,
    modifier: Modifier = Modifier,
    dark: Boolean = false,
    onClick: (() -> Unit)? = null,
) {
    val body: @Composable ColumnScope.() -> Unit = {
        val labelColor = if (dark) Color.White.copy(alpha = 0.8f) else MaterialTheme.colorScheme.onSurfaceVariant
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(label, style = MaterialTheme.typography.labelLarge, color = labelColor)
            Box(
                modifier = Modifier
                    .size(30.dp)
                    .background(
                        if (dark) Color.White.copy(alpha = 0.15f) else MaterialTheme.colorScheme.primaryContainer,
                        CircleShape,
                    ),
                contentAlignment = Alignment.Center,
            ) {
                Icon(
                    icon,
                    contentDescription = null,
                    modifier = Modifier.size(17.dp),
                    tint = if (dark) Color.White else MaterialTheme.colorScheme.primary,
                )
            }
        }
        Text(
            value,
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(top = 10.dp),
        )
        Text(
            caption,
            style = MaterialTheme.typography.bodySmall,
            color = labelColor,
            modifier = Modifier.padding(top = 2.dp),
        )
    }
    if (dark) HeroCard(modifier, onClick, body) else AppCard(modifier, onClick, content = body)
}

/**
 * `.live-band` — the full-width gradient strip above a tile grid. `tone`
 * turns it red for simulated data, matching the web's `.live-band.simulated`:
 * fabricated readings must never look like measurements.
 */
@Composable
fun LiveBand(
    title: String,
    subtitle: String,
    modifier: Modifier = Modifier,
    simulated: Boolean = false,
    connected: Boolean? = null,
    connectionLabel: String? = null,
    ageLabel: String? = null,
    trailing: (@Composable () -> Unit)? = null,
) {
    val brush = if (simulated) {
        Brush.linearGradient(listOf(Color(0xFF7F1D1D), Color(0xFFB3261E)))
    } else {
        HeroBrush
    }
    Surface(
        modifier = modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        color = Color.Transparent,
    ) {
        Column(modifier = Modifier.background(brush).padding(horizontal = 18.dp, vertical = 16.dp)) {
            CompositionLocalProvider(LocalContentColor provides Color.White) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                    if (simulated) {
                        Text(
                            "SIMULATED DATA",
                            style = MaterialTheme.typography.labelSmall,
                            fontWeight = FontWeight.Bold,
                            color = Color(0xFF7F1D1D),
                            modifier = Modifier
                                .background(Color.White, RoundedCornerShape(999.dp))
                                .padding(horizontal = 8.dp, vertical = 2.dp),
                        )
                    }
                }
                Text(
                    subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    color = Color.White.copy(alpha = 0.8f),
                    modifier = Modifier.padding(top = 4.dp),
                )
                if (connected != null) {
                    LiveChip(
                        connected = connected,
                        label = connectionLabel ?: if (connected) "Connected" else "No recent data",
                        age = ageLabel,
                        modifier = Modifier.padding(top = 12.dp),
                    )
                }
                trailing?.let {
                    Box(modifier = Modifier.padding(top = 12.dp)) { it() }
                }
            }
        }
    }
}

/** `.live-chip` — the status pill that rides inside a live band. */
@Composable
fun LiveChip(connected: Boolean, label: String, age: String?, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .background(Color.White.copy(alpha = 0.14f), RoundedCornerShape(999.dp))
            .padding(horizontal = 14.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(9.dp),
    ) {
        Box(
            modifier = Modifier
                .size(9.dp)
                .background(if (connected) Color(0xFF6EE7A0) else Color(0xFFF0B429), CircleShape),
        )
        Column {
            Text(label, style = MaterialTheme.typography.labelLarge, color = Color.White)
            age?.let {
                Text("Last update: $it", style = MaterialTheme.typography.labelSmall, color = Color.White.copy(alpha = 0.8f))
            }
        }
    }
}

/**
 * `.strip-tabs` — machine picker above a tile grid. Renders nothing for a
 * single machine, same as the web: one chip would just be a label, and would
 * imply there is something to switch to.
 */
@Composable
fun StripSelector(
    items: List<Triple<String, String, Boolean>>, // id, name, fresh
    selectedId: String?,
    onSelect: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    if (items.size < 2) return
    Row(
        modifier = modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(top = 10.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        items.forEach { (id, name, fresh) ->
            val selected = id == selectedId
            Row(
                modifier = Modifier
                    .background(
                        if (selected) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surface,
                        RoundedCornerShape(999.dp),
                    )
                    .clickable { onSelect(id) }
                    .padding(horizontal = 14.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(7.dp),
            ) {
                Box(
                    modifier = Modifier
                        .size(8.dp)
                        .background(
                            if (fresh) MaterialTheme.colorScheme.primary else LocalStatusColors.current.warnText,
                            CircleShape,
                        ),
                )
                Text(
                    name,
                    style = MaterialTheme.typography.labelLarge,
                    fontWeight = if (selected) FontWeight.Bold else FontWeight.Normal,
                    color = if (selected) MaterialTheme.colorScheme.onPrimaryContainer else MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

// ── Sensor tiles ────────────────────────────────────────────────────────

enum class TileStatus { Normal, Warn, Critical, Unavailable }

/**
 * A reading's band, mirroring `Range` in apps/web/components/SensorTile.tsx.
 * `min`/`max` are the edges of normal; `warnBand` is how far outside that
 * still counts as a warning before it becomes critical. Either edge may be
 * omitted for one-sided metrics (CO₂ has a ceiling but no useful floor).
 */
data class Range(val min: Double? = null, val max: Double? = null, val warnBand: Double = 0.0)

fun classify(value: Double?, range: Range?): TileStatus {
    if (value == null) return TileStatus.Unavailable
    if (range == null) return TileStatus.Normal
    range.min?.let { if (value < it) return if (value < it - range.warnBand) TileStatus.Critical else TileStatus.Warn }
    range.max?.let { if (value > it) return if (value > it + range.warnBand) TileStatus.Critical else TileStatus.Warn }
    return TileStatus.Normal
}

fun rangeHint(range: Range?, unit: String?): String? {
    if (range == null) return null
    val u = unit?.let { " $it" } ?: ""
    val min = range.min?.let { trimNumber(it) }
    val max = range.max?.let { trimNumber(it) }
    return when {
        min != null && max != null -> "Optimal: $min–$max$u"
        max != null -> "Optimal: <$max$u"
        min != null -> "Optimal: >$min$u"
        else -> null
    }
}

private fun trimNumber(v: Double): String =
    if (v == v.toLong().toDouble()) v.toLong().toString() else "%.1f".format(v)

@Composable
private fun tileColors(status: TileStatus): Pair<Color, Color> {
    val s = LocalStatusColors.current
    return when (status) {
        TileStatus.Normal -> s.okBg to s.okText
        TileStatus.Warn -> s.warnBg to s.warnText
        TileStatus.Critical -> s.dangerBg to s.dangerText
        TileStatus.Unavailable -> s.neutralBg to s.neutralText
    }
}

/** The shared shell behind [SensorTile] and [StateTile] — `.sensor-tile`. */
@Composable
private fun TileShell(
    icon: ImageVector,
    pillText: String,
    status: TileStatus,
    label: String,
    hint: String,
    modifier: Modifier = Modifier,
    value: @Composable RowScope.() -> Unit,
) {
    val (bg, fg) = tileColors(status)
    AppCard(modifier = modifier, contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp)) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Box(
                modifier = Modifier.size(34.dp).background(bg, RoundedCornerShape(10.dp)),
                contentAlignment = Alignment.Center,
            ) {
                Icon(icon, contentDescription = null, tint = fg, modifier = Modifier.size(19.dp))
            }
            Text(
                pillText,
                style = MaterialTheme.typography.labelSmall,
                fontWeight = FontWeight.Bold,
                color = fg,
                modifier = Modifier.background(bg, RoundedCornerShape(999.dp)).padding(horizontal = 9.dp, vertical = 3.dp),
            )
        }
        Text(
            label,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(top = 14.dp),
        )
        Row(verticalAlignment = Alignment.Bottom, modifier = Modifier.padding(top = 2.dp), content = value)
        Text(
            hint,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(top = 6.dp),
        )
    }
}

@Composable
fun SensorTile(
    icon: ImageVector,
    label: String,
    value: Double?,
    unit: String? = null,
    range: Range? = null,
    hint: String? = null,
    decimals: Int = 1,
    modifier: Modifier = Modifier,
) {
    val status = classify(value, range)
    val unavailable = status == TileStatus.Unavailable
    TileShell(
        icon = icon,
        pillText = when (status) {
            TileStatus.Normal -> "NORMAL"
            TileStatus.Warn -> "WARNING"
            TileStatus.Critical -> "CRITICAL"
            TileStatus.Unavailable -> "NO SENSOR"
        },
        status = status,
        label = label,
        hint = if (unavailable) "Not fitted on this device" else hint ?: rangeHint(range, unit) ?: " ",
        modifier = modifier,
    ) {
        // An em dash, never a 0 — a missing sensor must not read as a real
        // measurement of zero.
        Text(
            if (unavailable) "—" else "%.${decimals}f".format(value),
            style = MaterialTheme.typography.headlineSmall,
            fontWeight = FontWeight.Bold,
            fontSize = 30.sp,
            color = if (unavailable) MaterialTheme.colorScheme.onSurfaceVariant else MaterialTheme.colorScheme.onSurface,
        )
        if (!unavailable && unit != null) {
            Text(
                unit,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = 4.dp, bottom = 4.dp),
            )
        }
    }
}

/**
 * On/off actuator tile — same shell so a mixed grid reads as one system.
 * `state == null` means the device never reported this relay; that renders
 * as "—", never as OFF, since "we don't know" and "it is off" are different
 * claims about hardware.
 */
@Composable
fun StateTile(
    icon: ImageVector,
    label: String,
    state: Boolean?,
    hint: String? = null,
    modifier: Modifier = Modifier,
) {
    val unknown = state == null
    TileShell(
        icon = icon,
        pillText = if (unknown) "NO DATA" else if (state == true) "ACTIVE" else "IDLE",
        // An actuator being on isn't inherently good or bad — a running
        // heater is normal mid-cycle. Colour by "is it doing something".
        status = if (unknown || state != true) TileStatus.Unavailable else TileStatus.Normal,
        label = label,
        hint = if (unknown) "Not reported by this device" else hint ?: " ",
        modifier = modifier,
    ) {
        Text(
            if (unknown) "—" else if (state == true) "ON" else "OFF",
            style = MaterialTheme.typography.headlineSmall,
            fontWeight = FontWeight.Bold,
            fontSize = 30.sp,
            color = if (state == true) MaterialTheme.colorScheme.onSurface else MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

/** Two tiles per row — the phone equivalent of the web's auto-fit grid. */
@Composable
fun TileRow(modifier: Modifier = Modifier, content: @Composable RowScope.() -> Unit) {
    Row(
        modifier = modifier.fillMaxWidth().padding(bottom = 12.dp),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
        content = content,
    )
}

/** Centred, muted placeholder for "nothing here yet" states. */
@Composable
fun EmptyNote(text: String, modifier: Modifier = Modifier) {
    Text(
        text,
        style = MaterialTheme.typography.bodyMedium,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        textAlign = TextAlign.Center,
        modifier = modifier.fillMaxWidth().padding(vertical = 20.dp),
    )
}
