import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../core/formatting.dart';
import '../../core/theme.dart';
import '../../data/api_service.dart';
import '../../data/models.dart';
import '../../state/session.dart';
import '../components/app_components.dart';
import '../components/async_view.dart';
import 'flock_detail_screen.dart';

class FlocksScreen extends StatelessWidget {
  const FlocksScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final api = context.read<ApiService>();
    final farmId = context.watch<Session>().farmId;
    if (farmId == null) {
      return const EmptyState(icon: Icons.home_work_outlined, message: 'No farm selected.');
    }

    return AsyncView<List<Flock>>(
      load: () => api.flocks(farmId),
      builder: (context, flocks) {
        if (flocks.isEmpty) {
          return ListView(
            children: const [
              SizedBox(
                height: 240,
                child: EmptyState(
                  icon: Icons.groups_outlined,
                  message: 'No flocks on this farm yet.',
                ),
              ),
            ],
          );
        }
        return ListView.separated(
          padding: const EdgeInsets.all(16),
          itemCount: flocks.length,
          separatorBuilder: (_, _) => const SizedBox(height: 12),
          itemBuilder: (context, i) => _FlockCard(flocks[i]),
        );
      },
    );
  }
}

class _FlockCard extends StatelessWidget {
  final Flock flock;
  const _FlockCard(this.flock);

  @override
  Widget build(BuildContext context) {
    return AppCard(
      onTap: () => Navigator.of(context).push(
        MaterialPageRoute<void>(
          builder: (_) => FlockDetailScreen(flockId: flock.id, title: flock.name),
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  flock.name,
                  style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w700),
                ),
              ),
              if (flock.stage != null) StatusPill(flock.stage!, tone: PillTone.neutral),
            ],
          ),
          const SizedBox(height: 4),
          Text(
            '${flock.speciesName ?? kEmpty} · ${flock.purpose}',
            style: const TextStyle(color: AppColors.muted, fontSize: 12),
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              Expanded(child: Stat(value: '${flock.currentCount}', label: 'birds')),
              Expanded(
                child: Stat(
                  value: flock.ageDays == null ? kEmpty : '${flock.ageDays}',
                  label: 'days old',
                ),
              ),
              Expanded(child: Stat(value: '${flock.placedCount}', label: 'placed')),
            ],
          ),
        ],
      ),
    );
  }
}
