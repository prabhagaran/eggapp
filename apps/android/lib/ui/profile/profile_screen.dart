import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../core/config.dart';
import '../../core/formatting.dart';
import '../../core/theme.dart';
import '../../state/session.dart';
import '../components/app_components.dart';

class ProfileScreen extends StatelessWidget {
  const ProfileScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final session = context.watch<Session>();
    final me = session.me;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        AppCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SectionHeader('Signed in as'),
              DetailRow('Name', me?.name ?? kEmpty),
              DetailRow('Email', me?.email ?? kEmpty),
            ],
          ),
        ),
        const SizedBox(height: 12),
        AppCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SectionHeader('Farm'),
              if (session.farms.isEmpty)
                const Text(
                  'No farms available.',
                  style: TextStyle(color: AppColors.muted, fontSize: 13),
                )
              else
                // Switching farm re-reads every screen from the new farm id;
                // the picker lives here rather than in the app bar because a
                // field worker belongs to one farm and switches almost never.
                ...session.farms.map(
                  (f) => RadioListTile<String>(
                    value: f.id,
                    // ignore: deprecated_member_use
                    groupValue: session.farmId,
                    // ignore: deprecated_member_use
                    onChanged: (id) => id == null ? null : session.selectFarm(id),
                    title: Text(f.name),
                    subtitle: Text('${f.role} · ${f.timezone}'),
                    contentPadding: EdgeInsets.zero,
                  ),
                ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        AppCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SectionHeader('About'),
              const DetailRow('App', 'eggAPP field · Flutter'),
              DetailRow('API', AppConfig.apiBaseUrl),
            ],
          ),
        ),
        const SizedBox(height: 24),
        OutlinedButton.icon(
          onPressed: () => _confirmLogout(context),
          icon: const Icon(Icons.logout),
          label: const Text('Sign out'),
          style: OutlinedButton.styleFrom(
            foregroundColor: AppColors.dangerText,
            minimumSize: const Size.fromHeight(48),
          ),
        ),
      ],
    );
  }

  Future<void> _confirmLogout(BuildContext context) async {
    final session = context.read<Session>();
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (dialogContext) => AlertDialog(
        title: const Text('Sign out?'),
        content: const Text('You will need to sign in again to record anything.'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(dialogContext).pop(false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(dialogContext).pop(true),
            child: const Text('Sign out'),
          ),
        ],
      ),
    );
    if (confirmed ?? false) await session.logout();
  }
}
