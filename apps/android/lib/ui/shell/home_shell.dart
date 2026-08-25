import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../core/theme.dart';
import '../../state/session.dart';
import '../alerts/alerts_screen.dart';
import '../components/sync_status.dart';
import '../batches/batches_screen.dart';
import '../collections/collections_screen.dart';
import '../flocks/flocks_screen.dart';
import '../incubators/incubators_screen.dart';
import '../profile/profile_screen.dart';

/// The five destinations a field worker actually needs on a phone.
///
/// This is deliberately narrower than the web dashboard's eleven tabs: coop
/// admin, device management, reporting and team settings are manager
/// workflows and stay on the web per CLAUDE.md's surface split. Profile is
/// reachable from the app bar rather than taking a sixth tab slot.
enum _Destination {
  incubators('Incubators', Icons.thermostat_outlined),
  batches('Batches', Icons.egg_outlined),
  collections('Collections', Icons.inbox_outlined),
  flocks('Flocks', Icons.groups_outlined),
  alerts('Alerts', Icons.notifications_none);

  final String label;
  final IconData icon;
  const _Destination(this.label, this.icon);
}

class HomeShell extends StatefulWidget {
  const HomeShell({super.key});

  @override
  State<HomeShell> createState() => _HomeShellState();
}

class _HomeShellState extends State<HomeShell> {
  int _index = 0;

  @override
  Widget build(BuildContext context) {
    final destination = _Destination.values[_index];
    final farmName = context.watch<Session>().farm?.name;

    return Scaffold(
      appBar: AppBar(
        title: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(destination.label, style: const TextStyle(fontWeight: FontWeight.w700)),
            if (farmName != null)
              Text(
                farmName,
                style: const TextStyle(fontSize: 12, color: AppColors.muted),
              ),
          ],
        ),
        actions: [
          IconButton(
            tooltip: 'Profile',
            icon: const Icon(Icons.account_circle_outlined),
            onPressed: () => Navigator.of(context).push(
              MaterialPageRoute<void>(
                builder: (_) => Scaffold(
                  appBar: AppBar(title: const Text('Profile')),
                  body: const ProfileScreen(),
                ),
              ),
            ),
          ),
        ],
      ),
      body: Column(
        children: [
          // Sits above every tab: outstanding field records are the one thing
          // a worker must never lose track of, whichever screen they are on.
          const SyncBanner(),
          // IndexedStack keeps each tab's loaded data and scroll position
          // alive, so switching tabs doesn't re-fetch over a slow link.
          Expanded(
            child: IndexedStack(
              index: _index,
              children: const [
                IncubatorsScreen(),
                BatchesScreen(),
                CollectionsScreen(),
                FlocksScreen(),
                AlertsScreen(),
              ],
            ),
          ),
        ],
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _index,
        onDestinationSelected: (i) => setState(() => _index = i),
        destinations: [
          for (final d in _Destination.values)
            NavigationDestination(icon: Icon(d.icon), label: d.label),
        ],
      ),
    );
  }
}
