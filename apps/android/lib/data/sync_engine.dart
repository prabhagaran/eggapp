import 'dart:async';
import 'dart:convert';

import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:flutter/foundation.dart';

import 'api_client.dart';
import 'local/database.dart';

/// Drains the offline queue to the server (FRS §10).
///
/// Runs in-process rather than as an OS background job. That is a deliberate
/// scope choice for this slice: it covers the field case — record in the
/// barn, walk back into signal with the app open — without the platform work
/// a true background worker needs on both Android and iOS. A queued record is
/// never lost either way; it simply syncs the next time the app is running
/// and connected.
class SyncEngine extends ChangeNotifier {
  final AppDatabase db;
  final ApiClient api;
  final Connectivity connectivity;

  StreamSubscription<List<ConnectivityResult>>? _connSub;
  Timer? _periodic;
  bool _running = false;

  /// Set while a pass is in flight, so the UI can show that syncing is
  /// happening rather than leaving the user guessing (§19.6).
  bool get isSyncing => _running;

  SyncEngine({
    required this.db,
    required this.api,
    Connectivity? connectivity,
  }) : connectivity = connectivity ?? Connectivity();

  /// Starts listening for connectivity and begins periodic drains.
  ///
  /// §10 requires automatic retry when connectivity becomes available; the
  /// periodic timer is the safety net for the case where the OS reports a
  /// connection that is not actually usable yet — common on a phone rejoining
  /// WiFi, and doubly so here because the API is only reachable once
  /// Tailscale has re-established.
  void start() {
    _connSub = connectivity.onConnectivityChanged.listen((results) {
      final online = results.any((r) => r != ConnectivityResult.none);
      if (online) unawaited(sync());
    });
    _periodic = Timer.periodic(const Duration(minutes: 2), (_) => sync());
    unawaited(sync());
  }

  @override
  void dispose() {
    _connSub?.cancel();
    _periodic?.cancel();
    super.dispose();
  }

  /// Attempts one drain of the queue. Safe to call concurrently — overlapping
  /// calls return immediately rather than double-posting.
  Future<void> sync() async {
    if (_running) return;
    _running = true;
    notifyListeners();
    try {
      final due = await db.dueOperations();
      for (final op in due) {
        // Backoff is checked per operation rather than per pass, so one
        // failing record cannot hold up others captured after it.
        if (!_isDue(op)) continue;
        await _push(op);
      }
    } finally {
      _running = false;
      notifyListeners();
    }
  }

  /// Exponential backoff, capped: 0s, 30s, 1m, 2m, 4m, 8m, then every 15m.
  bool _isDue(PendingOperation op) {
    final last = op.lastAttemptAt;
    if (last == null || op.retryCount == 0) return true;
    final delay = op.retryCount >= 6
        ? const Duration(minutes: 15)
        : Duration(seconds: 30 * (1 << (op.retryCount - 1)));
    return DateTime.now().difference(last) >= delay;
  }

  Future<void> _push(PendingOperation op) async {
    await db.markSyncing(op.id);
    final body = jsonDecode(op.payload) as Map<String, dynamic>;
    final path = switch (op.operationType) {
      OperationType.candling =>
        'v1/farms/${op.farmId}/batches/${op.entityId}/candlings',
      OperationType.hatch =>
        'v1/farms/${op.farmId}/batches/${op.entityId}/hatch',
    };

    try {
      await api.post(path, body: body);
      await db.markSynced(op.id);
    } on ApiException catch (e) {
      await db.incrementRetry(op.id, op.retryCount);
      // A transport failure means "not now" — keep retrying. A 4xx that isn't
      // 401/408/429 means the server has judged the record itself invalid
      // (wrong batch status, missing discrepancy note), and no amount of
      // retrying will change that; it needs the user. Marking it terminal is
      // what stops a bad record retrying forever, which the previous Kotlin
      // app handled with its `conflict` state.
      final terminal = _isTerminal(e);
      await db.markFailed(op.id, e.message, terminal: terminal);
    } catch (e) {
      await db.incrementRetry(op.id, op.retryCount);
      await db.markFailed(op.id, '$e', terminal: false);
    }
    notifyListeners();
  }

  bool _isTerminal(ApiException e) {
    final code = e.statusCode;
    if (code == null) return false; // no response at all — transport
    if (code == 401 || code == 408 || code == 429) return false;
    if (code >= 500) return false; // server-side, may recover
    return code >= 400;
  }
}
