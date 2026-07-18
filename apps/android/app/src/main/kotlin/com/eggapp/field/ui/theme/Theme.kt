package com.eggapp.field.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

// Mirrors apps/web/app/globals.css's palette — same app, same brand, two platforms.
private val LightColors = lightColorScheme(
    primary = Color(0xFF1F7A4D),
    onPrimary = Color(0xFFFFFFFF),
    primaryContainer = Color(0xFFE3F3E7),
    onPrimaryContainer = Color(0xFF123524),
    secondary = Color(0xFF123524),
    onSecondary = Color(0xFFFFFFFF),
    background = Color(0xFFF3F5F1),
    onBackground = Color(0xFF17231B),
    surface = Color(0xFFFFFFFF),
    onSurface = Color(0xFF17231B),
    surfaceVariant = Color(0xFFF6F7F4),
    onSurfaceVariant = Color(0xFF77816F),
    outline = Color(0xFFE8EBE3),
    outlineVariant = Color(0xFFE8EBE3),
    error = Color(0xFFB3261E),
    onError = Color(0xFFFFFFFF),
    errorContainer = Color(0xFFFCE1DE),
    onErrorContainer = Color(0xFFB3261E),
)

private val DarkColors = darkColorScheme(
    primary = Color(0xFF4FAE7B),
    onPrimary = Color(0xFF0B1A10),
    primaryContainer = Color(0xFF1C3427),
    onPrimaryContainer = Color(0xFFA9E0BF),
    secondary = Color(0xFFA9E0BF),
    onSecondary = Color(0xFF0B1A10),
    background = Color(0xFF10140F),
    onBackground = Color(0xFFE7ECE2),
    surface = Color(0xFF181D16),
    onSurface = Color(0xFFE7ECE2),
    surfaceVariant = Color(0xFF1E241A),
    onSurfaceVariant = Color(0xFFA3AB97),
    outline = Color(0xFF2A3123),
    outlineVariant = Color(0xFF2A3123),
    error = Color(0xFFEF9186),
    onError = Color(0xFF3A1A17),
    errorContainer = Color(0xFF3A1A17),
    onErrorContainer = Color(0xFFEF9186),
)

// Status tints used by StatusPill (ui/components/AppComponents.kt) — kept
// separate from the M3 scheme since they're semantic (ok/warn/danger), not
// brand colors, and need a consistent light/dark pair each.
data class StatusColors(
    val okBg: Color, val okText: Color,
    val warnBg: Color, val warnText: Color,
    val dangerBg: Color, val dangerText: Color,
    val neutralBg: Color, val neutralText: Color,
)

val LightStatusColors = StatusColors(
    okBg = Color(0xFFE1F5E7), okText = Color(0xFF1F7A4D),
    warnBg = Color(0xFFFDEFD3), warnText = Color(0xFFA6690B),
    dangerBg = Color(0xFFFCE1DE), dangerText = Color(0xFFB3261E),
    neutralBg = Color(0xFFEEF0EC), neutralText = Color(0xFF6B7267),
)

val DarkStatusColors = StatusColors(
    okBg = Color(0xFF163625), okText = Color(0xFF6FDBA0),
    warnBg = Color(0xFF3A2F10), warnText = Color(0xFFE8C467),
    dangerBg = Color(0xFF3A1A17), dangerText = Color(0xFFEF9186),
    neutralBg = Color(0xFF232920), neutralText = Color(0xFFA3AB97),
)

val AppShapes = Shapes(
    extraSmall = RoundedCornerShape(8.dp),
    small = RoundedCornerShape(10.dp),
    medium = RoundedCornerShape(16.dp),
    large = RoundedCornerShape(20.dp),
    extraLarge = RoundedCornerShape(28.dp),
)

@Composable
fun EggAppTheme(content: @Composable () -> Unit) {
    val dark = isSystemInDarkTheme()
    val statusColors = if (dark) DarkStatusColors else LightStatusColors
    androidx.compose.runtime.CompositionLocalProvider(LocalStatusColors provides statusColors) {
        MaterialTheme(
            colorScheme = if (dark) DarkColors else LightColors,
            shapes = AppShapes,
            content = content,
        )
    }
}

val LocalStatusColors = androidx.compose.runtime.staticCompositionLocalOf { LightStatusColors }
