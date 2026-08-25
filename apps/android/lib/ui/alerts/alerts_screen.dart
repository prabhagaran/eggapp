import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../core/formatting.dart';
import '../../core/theme.dart';
import '../../data/api_service.dart';
import '../../data/models.dart';
import '../../state/session.dart';
import '../components/app_components.dart';
import '../components/async_view.dart';

class AlertsScreen extends StatelessWidget {
  const AlertsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final api = context.read<ApiService>();
    final farmId = context.watch<Session>().farmId;
    if (farmId == null) {
      return const EmptyState(icon: Icons.home_work_outlined, message: 'No farm selected.');
    }

    return AsyncView<List<Alert>>(
      refreshInterval: const Duration(seconds: 30),
      load: () => api.alerts(farmId),
      builder: (context, alerts) {
        if (alerts.isEmpty) {
          return ListView(
            children: const [
              SizedBox(
                height: 240,
                child: EmptyState(
                  icon: Icons.notifications_none,
                  message: 'No alerts. Everything is within range.',
                ),
              ),
            ],
          );
        }
        return ListView.separated(
          padding: const EdgeInsets.all(16),
          itemCount: alerts.length,
          separatorBuilder: (_, _) => const SizedBox(height: 12),
          itemBuilder: (context, i) => _AlertCard(alerts[i]),
        );
      },
    );
  }
}

class _AlertCard extends StatelessWidget {
  final Alert alert;
  const _AlertCard(this.alert);

  @override
  Widget build(BuildContext context) {
    final critical = alert.severity == 'critical';
    final resolved = alert.state == 'resolved';

    return AppCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(
                critical ? Icons.error_outline : Icons.warning_amber_outlined,
                size: 20,
                // A resolved alert is history, not an active problem — it
                // keeps its severity label but loses the alarming colour.
                color: resolved
                    ? AppColors.muted
                    : (critical ? AppColors.dangerText : AppColors.warnText),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  alert.message,
                  style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w600),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              StatusPill(
                alert.severity,
                tone: resolved
                    ? PillTone.neutral
                    : (critical ? PillTone.danger : PillTone.warn),
              ),
              const SizedBox(width: 8),
              StatusPill(alert.state, tone: resolved ? PillTone.ok : PillTone.neutral),
              const Spacer(),
              Text(
                fmtAge(alert.triggeredAt),
                style: const TextStyle(color: AppColors.muted, fontSize: 12),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
