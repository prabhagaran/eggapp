import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../core/formatting.dart';
import '../../core/incubation.dart';
import '../../core/theme.dart';
import '../../data/api_service.dart';
import '../../data/models.dart';
import '../../state/session.dart';
import '../components/app_components.dart';
import '../components/async_view.dart';
import 'batch_detail_screen.dart';

class BatchesScreen extends StatelessWidget {
  const BatchesScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final api = context.read<ApiService>();
    final farmId = context.watch<Session>().farmId;
    if (farmId == null) {
      return const EmptyState(icon: Icons.home_work_outlined, message: 'No farm selected.');
    }

    return AsyncView<List<Batch>>(
      load: () => api.batches(farmId),
      builder: (context, batches) {
        if (batches.isEmpty) {
          return ListView(
            children: const [
              SizedBox(
                height: 240,
                child: EmptyState(
                  icon: Icons.egg_outlined,
                  message: 'No batches yet.\nStart one from the web dashboard.',
                ),
              ),
            ],
          );
        }
        return ListView.separated(
          padding: const EdgeInsets.all(16),
          itemCount: batches.length,
          separatorBuilder: (_, _) => const SizedBox(height: 12),
          itemBuilder: (context, i) => BatchCard(batch: batches[i]),
        );
      },
    );
  }
}

class BatchCard extends StatelessWidget {
  final Batch batch;
  const BatchCard({required this.batch, super.key});

  @override
  Widget build(BuildContext context) {
    final day = incubationDay(batch.setAt);
    final total = batch.species?.incubationDays;
    final mismatch = deviceDayMismatch(day, batch.deviceDay);

    return AppCard(
      onTap: () => Navigator.of(context).push(
        MaterialPageRoute<void>(
          builder: (_) => BatchDetailScreen(batchId: batch.id, title: _title()),
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  _title(),
                  style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w700),
                ),
              ),
              StatusPill(batch.status, tone: batchTone(batch.status)),
            ],
          ),
          const SizedBox(height: 4),
          Text(
            'set ${fmtDate(batch.setAt)} · hatch ${fmtDate(batch.expectedHatchAt)}',
            style: const TextStyle(color: AppColors.muted, fontSize: 12),
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              Expanded(
                child: Stat(value: '${batch.viableCount}', label: 'viable'),
              ),
              Expanded(
                child: Stat(
                  value: day == null ? kEmpty : (total == null ? '$day' : '$day/$total'),
                  label: 'day',
                ),
              ),
              Expanded(
                child: Stat(value: fmtPct(batch.fertilityPct), label: 'fertility'),
              ),
            ],
          ),
          if (mismatch) ...[
            const SizedBox(height: 12),
            _MismatchNotice(batch: batch, appDay: day!),
          ],
        ],
      ),
    );
  }

  String _title() =>
      '${batch.species?.name ?? "Batch"} · ${batch.incubator?.name ?? kEmpty}';
}

/// Shown only when the device's day counter genuinely disagrees with the
/// batch record — more than a midnight-boundary apart. Both numbers are
/// displayed, because which one is right depends on which was set wrong.
class _MismatchNotice extends StatelessWidget {
  final Batch batch;
  final int appDay;

  const _MismatchNotice({required this.batch, required this.appDay});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(10),
      decoration: BoxDecoration(
        color: AppColors.warnBg,
        borderRadius: BorderRadius.circular(10),
      ),
      child: Text(
        'Device reports day ${batch.deviceDay}, schedule says day $appDay '
        '(hatch ${fmtDate(batch.deviceExpectedHatchAt)}).',
        style: const TextStyle(color: AppColors.warnText, fontSize: 12),
      ),
    );
  }
}
