package com.eggapp.field.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable

private val EggAppColors = lightColorScheme(
    primary = androidx.compose.ui.graphics.Color(0xFF4A7C2A), // matches apps/web/app/globals.css --accent
)

@Composable
fun EggAppTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = EggAppColors, content = content)
}
