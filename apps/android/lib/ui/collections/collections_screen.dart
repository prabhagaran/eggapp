import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../core/formatting.dart';
import '../../core/theme.dart';
import '../../data/api_service.dart';
import '../../data/models.dart';
import '../../state/session.dart';
import '../components/app_components.dart';
import '../components/async_view.dart';

class CollectionsScreen extends StatelessWidget {
  const CollectionsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final api = context.read<ApiService>();
    final farmId = context.watch<Session>().farmId;
    if (farmId == null) {
      return const EmptyState(icon: Icons.home_work_outlined, message: 'No farm selected.');
    }

    return AsyncView<List<EggCollection>>(
      load: () => api.collections(farmId),
      builder: (context, collections) {
        if (collections.isEmpty) {
          return ListView(
            children: const [
              SizedBox(
                height: 240,
                child: EmptyState(
                  icon: Icons.inbox_outlined,
                  message: 'No egg collections recorded yet.',
                ),
              ),
            ],
          );
        }
        return ListView.separated(
          padding: const EdgeInsets.all(16),
          itemCount: collections.length,
          separatorBuilder: (_, _) => const SizedBox(height: 12),
          itemBuilder: (context, i) => _CollectionCard(collections[i]),
        );
      },
    );
  }
}

class _CollectionCard extends StatelessWidget {
  final EggCollection collection;
  const _CollectionCard(this.collection);

  @override
  Widget build(BuildContext context) {
    final age = collection.ageDays;

    return AppCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  fmtDate(collection.collectedOn),
                  style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w700),
                ),
              ),
              // BR-011: over 14 days the eggs can't be set without an override
              // note, over 7 is a warning. The chip says which band this is in.
              StatusPill('$age d', tone: _ageTone(age)),
            ],
          ),
          if (collection.sourceNote != null && collection.sourceNote!.isNotEmpty) ...[
            const SizedBox(height: 4),
            Text(
              collection.sourceNote!,
              style: const TextStyle(color: AppColors.muted, fontSize: 12),
            ),
          ],
          const SizedBox(height: 16),
          Row(
            children: [
              Expanded(child: Stat(value: '${collection.count}', label: 'collected')),
              Expanded(child: Stat(value: '${collection.availableCount}', label: 'available')),
              Expanded(child: Stat(value: '${collection.assignedCount}', label: 'assigned')),
              Expanded(child: Stat(value: '${collection.discardedCount}', label: 'discarded')),
            ],
          ),
        ],
      ),
    );
  }

  PillTone _ageTone(int days) {
    if (days > 14) return PillTone.danger;
    if (days > 7) return PillTone.warn;
    return PillTone.ok;
  }
}
