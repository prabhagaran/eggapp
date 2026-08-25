import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:uuid/uuid.dart';

import 'core/theme.dart';
import 'data/api_client.dart';
import 'data/api_service.dart';
import 'data/field_record_repository.dart';
import 'data/local/database.dart';
import 'data/sync_engine.dart';
import 'data/token_store.dart';
import 'state/session.dart';
import 'ui/login/login_screen.dart';
import 'ui/shell/home_shell.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  final tokenStore = TokenStore();
  final apiClient = ApiClient(tokenStore: tokenStore);
  final api = ApiService(apiClient);
  final session = Session(tokenStore: tokenStore, api: api);

  // A refresh that fails means the session is genuinely over — drop to the
  // login screen rather than leaving every screen showing a 401.
  apiClient.onSessionExpired = session.expire;

  await session.restore();

  // Offline capture (FRS §8–§10). The database opens before the first frame
  // so a queued record is never missed by a screen that builds early.
  final db = AppDatabase();
  final deviceId = await resolveDeviceId(tokenStore);
  final records = FieldRecordRepository(db: db, deviceId: deviceId);
  final syncEngine = SyncEngine(db: db, api: apiClient)..start();

  runApp(
    MultiProvider(
      providers: [
        Provider<TokenStore>.value(value: tokenStore),
        Provider<ApiClient>.value(value: apiClient),
        Provider<ApiService>.value(value: api),
        Provider<AppDatabase>.value(value: db),
        Provider<FieldRecordRepository>.value(value: records),
        ChangeNotifierProvider<SyncEngine>.value(value: syncEngine),
        ChangeNotifierProvider<Session>.value(value: session),
      ],
      child: const EggApp(),
    ),
  );
}

/// A stable per-install identifier, stored alongside the tokens.
///
/// FRS §10 and §19.7 require knowing which device captured a record. This is
/// deliberately a random per-install id rather than a hardware identifier —
/// it answers "which client did this come from" without collecting anything
/// about the phone itself.
Future<String> resolveDeviceId(TokenStore store) async {
  final existing = await store.deviceId();
  if (existing != null) return existing;
  final id = const Uuid().v4();
  await store.saveDeviceId(id);
  return id;
}

class EggApp extends StatelessWidget {
  const EggApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'eggAPP',
      debugShowCheckedModeBanner: false,
      theme: buildAppTheme(),
      home: const _Root(),
    );
  }
}

class _Root extends StatelessWidget {
  const _Root();

  @override
  Widget build(BuildContext context) {
    final status = context.watch<Session>().status;
    return switch (status) {
      SessionStatus.loading => const Scaffold(
          body: Center(child: CircularProgressIndicator()),
        ),
      SessionStatus.loggedOut => const LoginScreen(),
      SessionStatus.loggedIn => const HomeShell(),
    };
  }
}
