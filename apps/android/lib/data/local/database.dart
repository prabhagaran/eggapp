import 'dart:io';

import 'package:drift/drift.dart';
import 'package:drift/native.dart';
import 'package:path/path.dart' as p;
import 'package:path_provider/path_provider.dart';

part 'database.g.dart';

/// Synchronization state of a queued field transaction (FRS §10).
///
/// `syncing` exists as a real state, not a transient flag: it prevents a
/// second sync pass picking up an operation whose request is still in flight,
/// and §19.6 requires the user to be able to see it.
enum SyncState { pending, syncing, synced, failed }

/// What kind of field record a queued operation carries. Stored explicitly
/// rather than inferred from the table, because §10 requires operation type
/// as queue metadata in its own right.
enum OperationType { candling, hatch }

/// The offline transaction queue (FRS §9, §10).
///
/// Every field write lands here **before** any network attempt. A record is
/// considered successfully captured once this row is committed — that is the
/// promise §9 makes to the user, and the reason nothing here is deleted on a
/// failed request.
@DataClassName('PendingOperation')
class PendingOperations extends Table {
  /// Unique operation id (§10). Sent to the API as `clientId`, which makes
  /// the write idempotent (§11, BR-010) — replaying it returns the original
  /// record instead of creating a second one.
  TextColumn get id => text()();

  /// The entity the operation acts on — the batch id for candling and hatch.
  TextColumn get entityId => text()();

  /// Farm scope, needed to build the request path after a restart.
  TextColumn get farmId => text()();

  TextColumn get operationType => textEnum<OperationType>()();

  DateTimeColumn get createdAt => dateTime()();

  /// Local user and device identity (§10, §19.7) — who captured this, and
  /// from which client. The server records its own view of the user, but the
  /// queue must be able to answer this while still offline.
  TextColumn get userId => text().nullable()();
  TextColumn get deviceId => text()();

  /// Request body as JSON, stored verbatim so a queued operation survives an
  /// app upgrade that changes how forms are built.
  TextColumn get payload => text()();

  TextColumn get syncState =>
      textEnum<SyncState>().withDefault(const Constant('pending'))();

  IntColumn get retryCount => integer().withDefault(const Constant(0))();

  DateTimeColumn get lastAttemptAt => dateTime().nullable()();

  /// Server or transport error from the last failed attempt (§10). Kept so
  /// the user can be told *why* something failed, not merely that it did.
  TextColumn get errorMessage => text().nullable()();

  /// True when the server rejected this on business-rule grounds (a 4xx that
  /// is not a transport failure). Retrying cannot fix it — it needs the user
  /// to amend or discard the record, so the sync worker must stop trying.
  BoolColumn get isTerminal => boolean().withDefault(const Constant(false))();

  @override
  Set<Column> get primaryKey => {id};
}

/// Read-through cache of batches (gap C8).
///
/// Not required by §8, which covers writes — but without it a cold start with
/// no connectivity shows nothing, and a worker cannot choose which batch to
/// candle. The previous Kotlin app added the same cache after on-device
/// testing showed the recording UI vanished exactly when offline.
@DataClassName('CachedBatch')
class CachedBatches extends Table {
  TextColumn get id => text()();
  TextColumn get farmId => text()();

  /// The batch list/detail JSON as returned by the API, replayed through the
  /// same model parsing as a live response so cached and live rows cannot
  /// drift apart.
  TextColumn get payload => text()();

  /// When this was last fetched from the server — surfaced in the UI so a
  /// stale reading is never mistaken for a live one (§19.5).
  DateTimeColumn get fetchedAt => dateTime()();

  @override
  Set<Column> get primaryKey => {id};
}

@DriftDatabase(tables: [PendingOperations, CachedBatches])
class AppDatabase extends _$AppDatabase {
  AppDatabase() : super(_open());

  /// Test constructor — an in-memory database with no file backing.
  AppDatabase.forTesting(super.executor);

  @override
  int get schemaVersion => 1;

  // ── Queue operations ──────────────────────────────────────────────────

  /// Enqueues a field transaction. This is the commit point that §9 calls
  /// "successfully captured".
  Future<void> enqueue(PendingOperationsCompanion op) =>
      into(pendingOperations).insert(op);

  /// Operations still owed to the server, oldest first so records sync in the
  /// order they were captured.
  Future<List<PendingOperation>> dueOperations() {
    return (select(pendingOperations)
          ..where((t) =>
              t.syncState.equalsValue(SyncState.pending) |
              t.syncState.equalsValue(SyncState.failed))
          ..where((t) => t.isTerminal.equals(false))
          ..orderBy([(t) => OrderingTerm.asc(t.createdAt)]))
        .get();
  }

  /// Everything not yet accepted by the server, for the sync-status UI (§19.6).
  Stream<List<PendingOperation>> watchUnsynced() {
    return (select(pendingOperations)
          ..where((t) => t.syncState.equalsValue(SyncState.synced).not())
          ..orderBy([(t) => OrderingTerm.desc(t.createdAt)]))
        .watch();
  }

  Stream<List<PendingOperation>> watchForEntity(String entityId) {
    return (select(pendingOperations)
          ..where((t) => t.entityId.equals(entityId))
          ..orderBy([(t) => OrderingTerm.desc(t.createdAt)]))
        .watch();
  }

  Future<void> markSyncing(String id) {
    return (update(pendingOperations)..where((t) => t.id.equals(id))).write(
      PendingOperationsCompanion(
        syncState: const Value(SyncState.syncing),
        lastAttemptAt: Value(DateTime.now()),
      ),
    );
  }

  Future<void> markSynced(String id) {
    return (update(pendingOperations)..where((t) => t.id.equals(id))).write(
      PendingOperationsCompanion(
        syncState: const Value(SyncState.synced),
        errorMessage: const Value(null),
      ),
    );
  }

  /// [terminal] marks a rejection that retrying cannot fix — the worker will
  /// not pick it up again, and the user is asked to resolve it.
  Future<void> markFailed(String id, String error, {required bool terminal}) {
    return (update(pendingOperations)..where((t) => t.id.equals(id))).write(
      PendingOperationsCompanion(
        syncState: const Value(SyncState.failed),
        errorMessage: Value(error),
        isTerminal: Value(terminal),
      ),
    );
  }

  Future<void> incrementRetry(String id, int current) {
    return (update(pendingOperations)..where((t) => t.id.equals(id)))
        .write(PendingOperationsCompanion(retryCount: Value(current + 1)));
  }

  /// Drops a permanently-rejected operation the user has acknowledged.
  /// Only ever called for terminal failures — never for anything the server
  /// might still accept.
  Future<void> discardOperation(String id) =>
      (delete(pendingOperations)..where((t) => t.id.equals(id))).go();

  /// Clears synced history older than [before]; the queue is a work list, not
  /// an audit log — the server holds the record of truth once synced.
  Future<int> pruneSynced(DateTime before) {
    return (delete(pendingOperations)
          ..where((t) =>
              t.syncState.equalsValue(SyncState.synced) &
              t.createdAt.isSmallerThanValue(before)))
        .go();
  }

  // ── Read cache ────────────────────────────────────────────────────────

  Future<void> cacheBatches(String farmId, Map<String, String> payloads) async {
    final now = DateTime.now();
    await batch((b) {
      b.insertAllOnConflictUpdate(cachedBatches, [
        for (final e in payloads.entries)
          CachedBatchesCompanion.insert(
            id: e.key,
            farmId: farmId,
            payload: e.value,
            fetchedAt: now,
          ),
      ]);
    });
  }

  // Named ...For / ...ById rather than matching the table name: a method
  // called `cachedBatches` would shadow the generated table getter, and
  // `select(cachedBatches)` would then resolve to the method itself.
  Future<List<CachedBatch>> cachedBatchesFor(String farmId) =>
      (select(cachedBatches)..where((t) => t.farmId.equals(farmId))).get();

  Future<CachedBatch?> cachedBatchById(String id) =>
      (select(cachedBatches)..where((t) => t.id.equals(id))).getSingleOrNull();
}

LazyDatabase _open() {
  return LazyDatabase(() async {
    final dir = await getApplicationDocumentsDirectory();
    return NativeDatabase.createInBackground(
      File(p.join(dir.path, 'eggapp_field.sqlite')),
    );
  });
}
