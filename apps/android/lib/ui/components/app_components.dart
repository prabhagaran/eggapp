import 'package:flutter/material.dart';

import '../../core/theme.dart';

/// Semantic pill — ok / warn / danger / neutral. Deliberately not tied to the
/// brand scheme: these mean "this is fine" or "look at this", not "this is
/// eggAPP".
enum PillTone { ok, warn, danger, neutral }

class StatusPill extends StatelessWidget {
  final String label;
  final PillTone tone;

  const StatusPill(this.label, {this.tone = PillTone.neutral, super.key});

  @override
  Widget build(BuildContext context) {
    final (bg, fg) = switch (tone) {
      PillTone.ok => (AppColors.okBg, AppColors.okText),
      PillTone.warn => (AppColors.warnBg, AppColors.warnText),
      PillTone.danger => (AppColors.dangerBg, AppColors.dangerText),
      PillTone.neutral => (AppColors.neutralBg, AppColors.neutralText),
    };
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
      decoration: BoxDecoration(color: bg, borderRadius: BorderRadius.circular(999)),
      child: Text(
        label,
        style: TextStyle(color: fg, fontSize: 12, fontWeight: FontWeight.w600),
      ),
    );
  }
}

/// Maps a batch status to a tone. Mirrors `batchBadgeClass()` in
/// apps/web/lib/useAuthedFarm.ts so the same status reads the same on both
/// surfaces.
PillTone batchTone(String status) => switch (status) {
      'completed' || 'hatching' => PillTone.ok,
      'aborted' || 'closed' => PillTone.neutral,
      _ => PillTone.warn,
    };

class AppCard extends StatelessWidget {
  final Widget child;
  final VoidCallback? onTap;
  final EdgeInsetsGeometry padding;

  const AppCard({
    required this.child,
    this.onTap,
    this.padding = const EdgeInsets.all(16),
    super.key,
  });

  @override
  Widget build(BuildContext context) {
    final card = Card(
      child: Padding(padding: padding, child: child),
    );
    if (onTap == null) return card;
    return InkWell(
      borderRadius: BorderRadius.circular(16),
      onTap: onTap,
      child: card,
    );
  }
}

/// A labelled figure — the "32 viable / day 3 of 21" row on batch cards.
class Stat extends StatelessWidget {
  final String value;
  final String label;
  final Widget? badge;

  const Stat({required this.value, required this.label, this.badge, super.key});

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          value,
          style: const TextStyle(fontSize: 20, fontWeight: FontWeight.w700, color: AppColors.text),
        ),
        const SizedBox(height: 2),
        badge ??
            Text(label, style: const TextStyle(fontSize: 12, color: AppColors.muted)),
      ],
    );
  }
}

class SectionHeader extends StatelessWidget {
  final String title;
  final Widget? trailing;

  const SectionHeader(this.title, {this.trailing, super.key});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8, top: 4),
      child: Row(
        children: [
          Expanded(
            child: Text(
              title,
              style: const TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.w700,
                color: AppColors.text,
              ),
            ),
          ),
          ?trailing,
        ],
      ),
    );
  }
}

/// Empty state — says what is missing, not just "no data".
class EmptyState extends StatelessWidget {
  final IconData icon;
  final String message;

  const EmptyState({required this.icon, required this.message, super.key});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 40, color: AppColors.muted),
            const SizedBox(height: 12),
            Text(
              message,
              textAlign: TextAlign.center,
              style: const TextStyle(color: AppColors.muted),
            ),
          ],
        ),
      ),
    );
  }
}

/// Failure state with a retry, used by every list screen. A field app loses
/// connectivity constantly; the error must be recoverable in one tap and must
/// say what actually went wrong.
class ErrorState extends StatelessWidget {
  final String message;
  final VoidCallback onRetry;

  const ErrorState({required this.message, required this.onRetry, super.key});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(Icons.cloud_off, size: 40, color: AppColors.muted),
            const SizedBox(height: 12),
            Text(message, textAlign: TextAlign.center),
            const SizedBox(height: 16),
            OutlinedButton.icon(
              onPressed: onRetry,
              icon: const Icon(Icons.refresh),
              label: const Text('Try again'),
            ),
          ],
        ),
      ),
    );
  }
}

/// One row of `label: value` — the workhorse of the detail screens.
class DetailRow extends StatelessWidget {
  final String label;
  final String value;
  final Widget? trailing;

  const DetailRow(this.label, this.value, {this.trailing, super.key});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 132,
            child: Text(label, style: const TextStyle(color: AppColors.muted, fontSize: 13)),
          ),
          Expanded(child: Text(value, style: const TextStyle(fontSize: 13))),
          ?trailing,
        ],
      ),
    );
  }
}
