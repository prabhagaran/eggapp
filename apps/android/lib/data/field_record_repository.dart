import 'dart:convert';

import 'package:drift/drift.dart';
import 'package:uuid/uuid.dart';

import 'local/database.dart';

/// Captures field records locally, then hands them to the sync engine.
///
/// This is the boundary FRS §9 describes: a record is "successfully captured"
/// the moment its row is committed here. Nothing in this class touches the
/// network — a save cannot fail because of connectivity, which is the whole
/// point of §8.
class FieldRecordRepository {
  final AppDatabase db;
  final String deviceId;
  final Uuid _uuid;

  FieldRecordRepository({
    required this.db,
    required this.deviceId,
    Uuid? uuid,
  }) : _uuid = uuid ?? const Uuid();

  /// Records a candling session (FRS §13).
  ///
  /// [dayNo] is the incubation day the session applies to; the server checks
  /// it against the batch schedule. Returns the operation id, which is also
  /// the idempotency key sent to the API as `clientId` (§11).
  Future<String> saveCandling({
    required String farmId,
    required String batchId,
    required String? userId,
    required int dayNo,
    required int fertile,
    required int clear,
    required int bloodRing,
    required int unsure,
    String? discrepancyNote,
    DateTime? candledAt,
  }) async {
    final id = _uuid.v4();
    final at = candledAt ?? DateTime.now();
    // Field names and shapes mirror the API's candlingSchema exactly, so the
    // queued payload can be POSTed verbatim without a translation step that
    // could drift from the contract.
    final payload = <String, dynamic>{
      'dayNo': dayNo,
      'candledAt': at.toUtc().toIso8601String(),
      'fertile': fertile,
      'clear': clear,
      'bloodRing': bloodRing,
      'unsure': unsure,
      if (discrepancyNote != null && discrepancyNote.isNotEmpty)
        'discrepancyNote': discrepancyNote,
      'clientId': id,
    };

    await db.enqueue(
      PendingOperationsCompanion.insert(
        id: id,
        entityId: batchId,
        farmId: farmId,
        operationType: OperationType.candling,
        createdAt: at,
        userId: Value(userId),
        deviceId: deviceId,
        payload: jsonEncode(payload),
      ),
    );
    return id;
  }

  /// Records a hatch outcome (FRS §13).
  Future<String> saveHatch({
    required String farmId,
    required String batchId,
    required String? userId,
    required int hatched,
    required int pippedDead,
    required int deadInShell,
    required int unhatched,
    String? discrepancyNote,
    DateTime? hatchedAt,
  }) async {
    final id = _uuid.v4();
    final at = hatchedAt ?? DateTime.now();
    final payload = <String, dynamic>{
      'hatchedAt': at.toUtc().toIso8601String(),
      'hatched': hatched,
      'pippedDead': pippedDead,
      'deadInShell': deadInShell,
      'unhatched': unhatched,
      if (discrepancyNote != null && discrepancyNote.isNotEmpty)
        'discrepancyNote': discrepancyNote,
      'clientId': id,
    };

    await db.enqueue(
      PendingOperationsCompanion.insert(
        id: id,
        entityId: batchId,
        farmId: farmId,
        operationType: OperationType.hatch,
        createdAt: at,
        userId: Value(userId),
        deviceId: deviceId,
        payload: jsonEncode(payload),
      ),
    );
    return id;
  }

  /// Everything not yet accepted by the server, for the status UI (§19.6).
  Stream<List<PendingOperation>> watchUnsynced() => db.watchUnsynced();

  /// Queued operations for one batch, so its detail screen can show records
  /// that exist only locally alongside the server's own (§19.6).
  Stream<List<PendingOperation>> watchForBatch(String batchId) =>
      db.watchForEntity(batchId);

  /// Drops an operation the server permanently rejected and the user has
  /// chosen not to amend.
  Future<void> discard(String operationId) => db.discardOperation(operationId);
}
