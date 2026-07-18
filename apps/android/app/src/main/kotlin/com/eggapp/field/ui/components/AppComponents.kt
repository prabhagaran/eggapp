package com.eggapp.field.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.eggapp.field.ui.theme.LocalStatusColors

enum class PillTone { Ok, Warn, Danger, Neutral, Accent }

@Composable
fun StatusPill(text: String, tone: PillTone, modifier: Modifier = Modifier) {
    val status = LocalStatusColors.current
    val (bg, fg) = when (tone) {
        PillTone.Ok -> status.okBg to status.okText
        PillTone.Warn -> status.warnBg to status.warnText
        PillTone.Danger -> status.dangerBg to status.dangerText
        PillTone.Neutral -> status.neutralBg to status.neutralText
        PillTone.Accent -> MaterialTheme.colorScheme.primaryContainer to MaterialTheme.colorScheme.onPrimaryContainer
    }
    Text(
        text = text,
        color = fg,
        style = MaterialTheme.typography.labelMedium,
        modifier = modifier
            .background(bg, RoundedCornerShape(999.dp))
            .padding(horizontal = 10.dp, vertical = 3.dp),
    )
}

@Composable
fun SectionHeading(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text,
        style = MaterialTheme.typography.titleMedium,
        color = MaterialTheme.colorScheme.onBackground,
        modifier = modifier.padding(top = 20.dp, bottom = 8.dp),
    )
}

@Composable
fun MutedText(text: String, modifier: Modifier = Modifier) {
    Text(text, color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.bodySmall, modifier = modifier)
}
