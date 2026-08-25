import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../core/formatting.dart';
import '../../core/theme.dart';
import '../../data/field_record_repository.dart';
import '../../data/local/database.dart';
import '../../data/sync_engine.dart';
import 'app_components.dart';

/// FRS §19.6: the user must be able to tell whether a record they entered is
/// pending, synchronizing, synced or failed. Without this the offline queue is
/// invisible, and a field worker has no way to know their work is safe.
StatusPill syncPill(SyncState state, {bool terminal = false}) => switch (state) {
      SyncState.pending => const StatusPill('queued', tone: PillTone.warn),
      SyncState.syncing => const StatusPill('syncing…', tone: PillTone.neutral),
      SyncState.synced => const StatusPill('synced', tone: PillTone.ok),
      SyncState.failed => StatusPill(
          terminal ? 'rejected' : 'retrying',
          tone: terminal ? PillTone.danger : PillTone.warn,
        ),
    };

String operationLabel(OperationType t) => switch (t) {
      OperationType.candling => 'Candling',
      OperationType.hatch => 'Hatch',
    };

/// Compact banner for the app bar area: how many records are still owed to
/// the server. Hidden entirely when everything is synced — a field app should
/// not nag when there is nothing to act on.
class SyncBanner extends StatelessWidget {
  const SyncBanner({super.key});

  @override
  Widget build(BuildContext context) {
    final repo = context.read<FieldRecordRepository>();
    return StreamBuilder<List<PendingOperation>>(
      stream: repo.watchUnsynced(),
      builder: (context, snap) {
        final ops = snap.data ?? const <PendingOperation>[];
        if (ops.isEmpty) return const SizedBox.shrink();

        final rejected = ops.where((o) => o.isTerminal).length;
        final syncing = context.watch<SyncEngine>().isSyncing;
        final danger = rejected > 0;

        return Container(
          width: double.infinity,
          color: danger ? AppColors.dangerBg : AppColors.warnBg,
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
          child: Row(
            children: [
              Icon(
                danger ? Icons.error_outline : Icons.cloud_upload_outlined,
                size: 18,
                color: danger ? AppColors.dangerText : AppColors.warnText,
              ),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  danger
                      ? '$rejected record${rejected == 1 ? "" : "s"} need attention'
                      : syncing
                          ? 'Syncing ${ops.length} record${ops.length == 1 ? "" : "s"}…'
                          : '${ops.length} record${ops.length == 1 ? "" : "s"} waiting to sync',
                  style: TextStyle(
                    fontSize: 12,
                    fontWeight: FontWeight.w600,
                    color: danger ? AppColors.dangerText : AppColors.warnText,
                  ),
                ),
              ),
              TextButton(
                onPressed: () => Navigator.of(context).push(
                  MaterialPageRoute<void>(builder: (_) => const SyncQueueScreen()),
                ),
                child: const Text('View'),
              ),
            ],
          ),
        );
      },
    );
  }
}

/// Full queue view — what is outstanding, and why anything failed.
class SyncQueueScreen extends StatelessWidget {
  const SyncQueueScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final repo = context.read<FieldRecordRepository>();
    return Scaffold(
      appBar: AppBar(
        title: const Text('Pending records'),
        actions: [
          IconButton(
            tooltip: 'Sync now',
            icon: const Icon(Icons.sync),
            onPressed: () => context.read<SyncEngine>().sync(),
          ),
        ],
      ),
      body: StreamBuilder<List<PendingOperation>>(
        stream: repo.watchUnsynced(),
        builder: (context, snap) {
          final ops = snap.data ?? const <PendingOperation>[];
          if (ops.isEmpty) {
            return const EmptyState(
              icon: Icons.cloud_done_outlined,
              message: 'Everything is synced.',
            );
          }
          return ListView.separated(
            padding: const EdgeInsets.all(16),
            itemCount: ops.length,
            separatorBuilder: (_, _) => const SizedBox(height: 12),
            itemBuilder: (context, i) => _OpCard(op: ops[i]),
          );
        },
      ),
    );
  }
}

class _OpCard extends StatelessWidget {
  final PendingOperation op;
  const _OpCard({required this.op});

  @override
  Widget build(BuildContext context) {
    return AppCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  operationLabel(op.operationType),
                  style: const TextStyle(fontSize: 15, fontWeight: FontWeight.w700),
                ),
              ),
              syncPill(op.syncState, terminal: op.isTerminal),
            ],
          ),
          const SizedBox(height: 4),
          Text(
            'captured ${fmtDateTime(op.createdAt)}',
            style: const TextStyle(color: AppColors.muted, fontSize: 12),
          ),
          if (op.retryCount > 0)
            Text(
              '${op.retryCount} attempt${op.retryCount == 1 ? "" : "s"}'
              '${op.lastAttemptAt == null ? "" : ", last ${fmtAge(op.lastAttemptAt)}"}',
              style: const TextStyle(color: AppColors.muted, fontSize: 12),
            ),
          if (op.errorMessage != null) ...[
            const SizedBox(height: 8),
            Container(
              width: double.infinity,
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: op.isTerminal ? AppColors.dangerBg : AppColors.warnBg,
                borderRadius: BorderRadius.circular(10),
              ),
              child: Text(
                op.errorMessage!,
                style: TextStyle(
                  fontSize: 12,
                  color: op.isTerminal ? AppColors.dangerText : AppColors.warnText,
                ),
              ),
            ),
          ],
          if (op.isTerminal) ...[
            const SizedBox(height: 12),
            // A terminal rejection will never succeed on retry. The only
            // honest options are to discard it or fix the underlying record
            // on the dashboard, so the UI offers the former explicitly rather
            // than leaving it stuck in the queue forever.
            OutlinedButton.icon(
              onPressed: () => _confirmDiscard(context),
              icon: const Icon(Icons.delete_outline),
              label: const Text('Discard this record'),
              style: OutlinedButton.styleFrom(foregroundColor: AppColors.dangerText),
            ),
          ],
        ],
      ),
    );
  }

  Future<void> _confirmDiscard(BuildContext context) async {
    final repo = context.read<FieldRecordRepository>();
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Discard record?'),
        content: const Text(
          'The server rejected this record and it will not be retried. '
          'Discarding removes it from this device permanently.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Keep'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(ctx).pop(true),
            child: const Text('Discard'),
          ),
        ],
      ),
    );
    if (ok ?? false) await repo.discard(op.id);
  }
}
