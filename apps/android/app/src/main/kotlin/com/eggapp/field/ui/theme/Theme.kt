package com.eggapp.field.ui.theme

import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
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

// Status tints used by StatusPill (ui/components/AppComponents.kt) — kept
// separate from the M3 scheme since they're semantic (ok/warn/danger), not
// brand colors.
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

val AppShapes = Shapes(
    extraSmall = RoundedCornerShape(8.dp),
    small = RoundedCornerShape(10.dp),
    medium = RoundedCornerShape(16.dp),
    large = RoundedCornerShape(20.dp),
    extraLarge = RoundedCornerShape(28.dp),
)

// Always light — no dark mode, regardless of system setting.
@Composable
fun EggAppTheme(content: @Composable () -> Unit) {
    androidx.compose.runtime.CompositionLocalProvider(LocalStatusColors provides LightStatusColors) {
        MaterialTheme(
            colorScheme = LightColors,
            shapes = AppShapes,
            content = content,
        )
    }
}

val LocalStatusColors = androidx.compose.runtime.staticCompositionLocalOf { LightStatusColors }
