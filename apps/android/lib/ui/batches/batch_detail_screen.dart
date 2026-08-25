import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../core/formatting.dart';
import '../../core/incubation.dart';
import '../../core/theme.dart';
import '../../data/api_service.dart';
import '../../data/field_record_repository.dart';
import '../../data/local/database.dart';
import '../../data/models.dart';
import '../../state/session.dart';
import '../components/app_components.dart';
import '../components/async_view.dart';
import '../components/sync_status.dart';
import 'candling_form.dart';
import 'hatch_form.dart';

class BatchDetailScreen extends StatelessWidget {
  final String batchId;
  final String title;

  const BatchDetailScreen({required this.batchId, required this.title, super.key});

  @override
  Widget build(BuildContext context) {
    final api = context.read<ApiService>();
    final farmId = context.watch<Session>().farmId!;

    return Scaffold(
      appBar: AppBar(title: Text(title)),
      body: AsyncView<BatchDetail>(
        load: () => api.batchDetail(farmId, batchId),
        builder: (context, detail) => _Body(detail: detail, farmId: farmId),
      ),
    );
  }
}

class _Body extends StatelessWidget {
  final BatchDetail detail;
  final String farmId;
  const _Body({required this.detail, required this.farmId});

  @override
  Widget build(BuildContext context) {
    // Locally queued records drive both the "not yet synced" list and the
    // day pre-fill, so the whole body listens to the queue rather than
    // reading it once.
    return StreamBuilder<List<PendingOperation>>(
      stream: context.read<FieldRecordRepository>().watchForBatch(detail.batch.id),
      builder: (context, snap) =>
          _content(context, snap.data ?? const <PendingOperation>[]),
    );
  }

  Widget _content(BuildContext context, List<PendingOperation> queued) {
    final b = detail.batch;
    // Days already covered, from the server's sessions and from anything
    // still queued locally — a worker who candled offline this morning must
    // not be offered day 7 again this afternoon.
    final recordedDays = <int>{
      ...detail.candlings.map((c) => c.dayNo),
      ...queuedCandlingDays(queued),
    };
    final day = incubationDay(b.setAt);
    final total = b.species?.incubationDays;
    final mismatch = deviceDayMismatch(day, b.deviceDay);

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        AppCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Expanded(
                    child: Text(
                      b.species?.name ?? 'Batch',
                      style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w700),
                    ),
                  ),
                  StatusPill(b.status, tone: batchTone(b.status)),
                ],
              ),
              const SizedBox(height: 16),
              Wrap(
                spacing: 24,
                runSpacing: 16,
                children: [
                  Stat(value: '${b.viableCount}', label: 'viable'),
                  Stat(
                    value: day == null ? kEmpty : (total == null ? '$day' : '$day/$total'),
                    label: 'day',
                  ),
                  if (b.deviceDay != null)
                    Stat(
                      value: '${b.deviceDay}',
                      label: 'device day',
                      badge: mismatch
                          ? const StatusPill('device day (mismatch)', tone: PillTone.warn)
                          : null,
                    ),
                  Stat(value: fmtPct(b.fertilityPct), label: 'fertility'),
                  Stat(value: fmtPct(b.hatchOfSetPct), label: 'hatch of set'),
                  Stat(value: fmtPct(b.hatchOfFertilePct), label: 'hatch of fertile'),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        AppCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SectionHeader('Schedule'),
              DetailRow('Set', fmtDate(b.setAt)),
              DetailRow(
                'Candling days',
                b.candlingDays.isEmpty ? kEmpty : b.candlingDays.join(', '),
              ),
              DetailRow('Lockdown', fmtDate(b.lockdownAt)),
              DetailRow('Expected hatch', fmtDate(b.expectedHatchAt)),
              if (b.deviceExpectedHatchAt != null) ...[
                const SizedBox(height: 8),
                Container(
                  width: double.infinity,
                  padding: const EdgeInsets.all(10),
                  decoration: BoxDecoration(
                    color: mismatch ? AppColors.warnBg : AppColors.surfaceAlt,
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: Text(
                    'Device reports day ${b.deviceDay}, expects hatch '
                    '${fmtDate(b.deviceExpectedHatchAt)}'
                    '${mismatch ? " — doesn't match the schedule above" : ""}',
                    style: TextStyle(
                      fontSize: 12,
                      color: mismatch ? AppColors.warnText : AppColors.muted,
                    ),
                  ),
                ),
              ],
            ],
          ),
        ),
        if (detail.sources.isNotEmpty) ...[
          const SizedBox(height: 12),
          AppCard(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const SectionHeader('Egg sources'),
                ...detail.sources.map(
                  (s) => DetailRow(
                    fmtDate(s.collection?.collectedOn),
                    '${s.count} eggs',
                    trailing: s.collection == null
                        ? null
                        : StatusPill(
                            '${s.collection!.ageDays} d',
                            tone: _ageTone(s.collection!.ageDays),
                          ),
                  ),
                ),
              ],
            ),
          ),
        ],
        const SizedBox(height: 12),
        _LocalRecords(operations: queued),
        const SizedBox(height: 12),
        // Both forms are always rendered; each decides for itself whether the
        // batch is eligible and explains why not, rather than the record
        // simply being unavailable with no reason given.
        CandlingForm(
          farmId: farmId,
          batchId: b.id,
          batchStatus: b.status,
          viableCount: b.viableCount,
          scheduleDays: b.candlingDays,
          recordedDays: recordedDays,
        ),
        const SizedBox(height: 12),
        HatchForm(
          farmId: farmId,
          batchId: b.id,
          batchStatus: b.status,
          viableCount: b.viableCount,
        ),
      ],
    );
  }

  /// BR-011 storage-age bands.
  PillTone _ageTone(int days) {
    if (days > 14) return PillTone.danger;
    if (days > 7) return PillTone.warn;
    return PillTone.ok;
  }
}

/// Candling days present in the local queue. A queued record counts as
/// "recorded" for pre-fill purposes — it exists, it just has not reached the
/// server yet.
Set<int> queuedCandlingDays(List<PendingOperation> ops) {
  final days = <int>{};
  for (final op in ops) {
    if (op.operationType != OperationType.candling) continue;
    if (op.isTerminal) continue; // rejected — the day is not actually covered
    final payload = jsonDecode(op.payload) as Map<String, dynamic>;
    final d = payload['dayNo'];
    if (d is num) days.add(d.toInt());
  }
  return days;
}

/// Records captured on this device that the server has not accepted yet.
///
/// Shown above the capture forms so a field worker can see their own work
/// immediately, regardless of connectivity — FRS §19.6, and the behaviour
/// whose absence made the previous Kotlin app's offline UI feel broken.
class _LocalRecords extends StatelessWidget {
  final List<PendingOperation> operations;
  const _LocalRecords({required this.operations});

  @override
  Widget build(BuildContext context) {
    final unsynced =
        operations.where((o) => o.syncState != SyncState.synced).toList();
    if (unsynced.isEmpty) return const SizedBox.shrink();

    return AppCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const SectionHeader('Captured on this device'),
          ...unsynced.map(
            (op) => Padding(
              padding: const EdgeInsets.symmetric(vertical: 6),
              child: Row(
                children: [
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          operationLabel(op.operationType),
                          style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w600),
                        ),
                        Text(
                          fmtDateTime(op.createdAt),
                          style: const TextStyle(color: AppColors.muted, fontSize: 12),
                        ),
                      ],
                    ),
                  ),
                  syncPill(op.syncState, terminal: op.isTerminal),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}
