import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../core/formatting.dart';
import '../../core/theme.dart';
import '../../data/api_service.dart';
import '../../data/models.dart';
import '../../state/session.dart';
import '../components/app_components.dart';
import '../components/async_view.dart';

/// Flock detail plus vaccination compliance, fetched together — the
/// compliance table is a separate endpoint but is useless on its own, so
/// showing them apart would mean two spinners for one screen.
class _FlockBundle {
  final FlockDetail flock;
  final List<ComplianceItem> compliance;
  const _FlockBundle(this.flock, this.compliance);
}

class FlockDetailScreen extends StatelessWidget {
  final String flockId;
  final String title;

  const FlockDetailScreen({required this.flockId, required this.title, super.key});

  @override
  Widget build(BuildContext context) {
    final api = context.read<ApiService>();
    final farmId = context.watch<Session>().farmId!;

    return Scaffold(
      appBar: AppBar(title: Text(title)),
      body: AsyncView<_FlockBundle>(
        load: () async {
          final results = await Future.wait([
            api.flock(farmId, flockId),
            api.vaccinationCompliance(farmId, flockId),
          ]);
          return _FlockBundle(
            results[0] as FlockDetail,
            results[1] as List<ComplianceItem>,
          );
        },
        builder: (context, bundle) => _Body(bundle: bundle),
      ),
    );
  }
}

class _Body extends StatelessWidget {
  final _FlockBundle bundle;
  const _Body({required this.bundle});

  @override
  Widget build(BuildContext context) {
    final f = bundle.flock;

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
                      f.name,
                      style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w700),
                    ),
                  ),
                  if (f.stage != null) StatusPill(f.stage!, tone: PillTone.neutral),
                ],
              ),
              const SizedBox(height: 16),
              Row(
                children: [
                  Expanded(child: Stat(value: '${f.currentCount}', label: 'birds')),
                  Expanded(
                    child: Stat(
                      value: f.ageDays == null ? kEmpty : '${f.ageDays}',
                      label: 'days old',
                    ),
                  ),
                  Expanded(child: Stat(value: '${f.placedCount}', label: 'placed')),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        _VaccinationCompliance(items: bundle.compliance),
        const SizedBox(height: 12),
        _MortalityHistory(records: f.mortalityRecords),
        const SizedBox(height: 12),
        _FeedWater(feed: f.recentFeed, water: f.recentWater),
        const SizedBox(height: 12),
        const AppCard(
          child: Row(
            children: [
              Icon(Icons.edit_note_outlined, color: AppColors.muted),
              SizedBox(width: 12),
              Expanded(
                child: Text(
                  'Mortality, vaccination and feed/water recording arrive in '
                  'Phase 2, backed by the offline queue.',
                  style: TextStyle(color: AppColors.muted, fontSize: 13),
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}

class _VaccinationCompliance extends StatelessWidget {
  final List<ComplianceItem> items;
  const _VaccinationCompliance({required this.items});

  @override
  Widget build(BuildContext context) {
    return AppCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const SectionHeader('Vaccination schedule'),
          if (items.isEmpty)
            const Text(
              'No vaccination template applies to this flock.',
              style: TextStyle(color: AppColors.muted, fontSize: 13),
            )
          else
            ...items.map(
              (i) => DetailRow(
                i.vaccine,
                '${i.disease} · ${i.route} · due ${fmtDate(i.dueDate)}',
                trailing: StatusPill(i.status, tone: _complianceTone(i.status)),
              ),
            ),
        ],
      ),
    );
  }

  PillTone _complianceTone(String status) => switch (status) {
        'administered' => PillTone.ok,
        'overdue' => PillTone.danger,
        'due' => PillTone.warn,
        _ => PillTone.neutral,
      };
}

class _MortalityHistory extends StatelessWidget {
  final List<MortalityRecord> records;
  const _MortalityHistory({required this.records});

  @override
  Widget build(BuildContext context) {
    return AppCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const SectionHeader('Mortality'),
          if (records.isEmpty)
            const Text(
              'No losses recorded.',
              style: TextStyle(color: AppColors.muted, fontSize: 13),
            )
          else
            ...records.map(
              (r) => DetailRow(
                fmtDate(r.date),
                '${r.count} · ${r.cause}${r.notes == null ? "" : " — ${r.notes}"}',
              ),
            ),
        ],
      ),
    );
  }
}

class _FeedWater extends StatelessWidget {
  final List<FeedLog> feed;
  final List<WaterLog> water;

  const _FeedWater({required this.feed, required this.water});

  @override
  Widget build(BuildContext context) {
    return AppCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const SectionHeader('Recent feed & water'),
          if (feed.isEmpty && water.isEmpty)
            const Text(
              'No feed or water checks logged.',
              style: TextStyle(color: AppColors.muted, fontSize: 13),
            ),
          ...feed.map(
            (f) => DetailRow(
              fmtDate(f.loggedAt),
              '${f.feedType} · ${f.quantityKg} kg',
              // The server flags feed that doesn't match the flock's derived
              // stage — a grower ration going to chicks, say.
              trailing: f.stageMismatch
                  ? const StatusPill('stage mismatch', tone: PillTone.warn)
                  : null,
            ),
          ),
          ...water.map(
            (w) => DetailRow(fmtDate(w.loggedAt), '${w.quantityLiters} L water'),
          ),
        ],
      ),
    );
  }
}
