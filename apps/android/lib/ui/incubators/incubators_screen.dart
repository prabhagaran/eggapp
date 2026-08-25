import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../core/formatting.dart';
import '../../core/theme.dart';
import '../../data/api_service.dart';
import '../../data/models.dart';
import '../../state/session.dart';
import '../components/app_components.dart';
import '../components/async_view.dart';

class IncubatorsScreen extends StatelessWidget {
  const IncubatorsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final api = context.read<ApiService>();
    final farmId = context.watch<Session>().farmId;
    if (farmId == null) {
      return const EmptyState(icon: Icons.home_work_outlined, message: 'No farm selected.');
    }

    return AsyncView<List<Incubator>>(
      // 15s matches the web dashboard's cadence. Telemetry itself lands every
      // ~60s (docs/iot/telemetry-contract.md), so this is about noticing a
      // fresh reading promptly, not about polling faster than the device.
      refreshInterval: const Duration(seconds: 15),
      load: () => api.incubators(farmId),
      builder: (context, incubators) {
        if (incubators.isEmpty) {
          return const _Scrollable(
            child: EmptyState(
              icon: Icons.thermostat_outlined,
              message: 'No incubators on this farm yet.',
            ),
          );
        }
        return ListView.separated(
          padding: const EdgeInsets.all(16),
          itemCount: incubators.length,
          separatorBuilder: (_, _) => const SizedBox(height: 12),
          itemBuilder: (context, i) => _IncubatorCard(incubators[i]),
        );
      },
    );
  }
}

class _Scrollable extends StatelessWidget {
  final Widget child;
  const _Scrollable({required this.child});

  @override
  Widget build(BuildContext context) {
    // A scrollable is required for pull-to-refresh to work on an empty list.
    return ListView(children: [SizedBox(height: 240, child: child)]);
  }
}

class _IncubatorCard extends StatelessWidget {
  final Incubator incubator;
  const _IncubatorCard(this.incubator);

  @override
  Widget build(BuildContext context) {
    final t = incubator.latestTelemetry;
    final fresh = isFresh(t?.ts);

    return AppCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                child: Text(
                  incubator.name,
                  style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w700),
                ),
              ),
              _devicePill(incubator, fresh),
            ],
          ),
          const SizedBox(height: 4),
          Text(
            'Capacity ${incubator.capacity}',
            style: const TextStyle(color: AppColors.muted, fontSize: 12),
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              Expanded(
                child: Stat(value: fmtNum(t?.tempC, suffix: '°C'), label: 'temperature'),
              ),
              Expanded(
                child: Stat(value: fmtNum(t?.humidityPct, suffix: '%'), label: 'humidity'),
              ),
              Expanded(
                child: Stat(value: fmtAge(t?.ts), label: 'last reading'),
              ),
            ],
          ),
          if (t != null) ...[
            const SizedBox(height: 12),
            const Divider(),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                // null is "this firmware doesn't report the channel", which is
                // not the same as OFF — those render as a muted em dash.
                _relay('Heater', t.heaterOn),
                _relay('Cooler', t.coolerOn),
                _relay('Humidifier', t.humidifierOn),
                _relay('Fan', t.fanOn),
                _relay('Turner', t.turnerOn),
                _relay('Pump', t.pumpOn),
              ],
            ),
          ],
        ],
      ),
    );
  }

  Widget _devicePill(Incubator inc, bool fresh) {
    if (inc.device == null) {
      return const StatusPill('no device', tone: PillTone.neutral);
    }
    if (inc.device!.status != 'active') {
      return StatusPill(inc.device!.status, tone: PillTone.danger);
    }
    return fresh
        ? const StatusPill('live', tone: PillTone.ok)
        : const StatusPill('stale', tone: PillTone.warn);
  }

  Widget _relay(String label, bool? on) {
    if (on == null) {
      return StatusPill('$label $kEmpty', tone: PillTone.neutral);
    }
    return StatusPill(
      '$label ${on ? "on" : "off"}',
      tone: on ? PillTone.ok : PillTone.neutral,
    );
  }
}
