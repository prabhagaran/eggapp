import 'dart:convert';

import 'package:drift/native.dart';
import 'package:eggapp_field/data/field_record_repository.dart';
import 'package:eggapp_field/data/local/database.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  late AppDatabase db;
  late FieldRecordRepository repo;

  setUp(() {
    db = AppDatabase.forTesting(NativeDatabase.memory());
    repo = FieldRecordRepository(db: db, deviceId: 'test-device');
  });

  tearDown(() => db.close());

  group('capture (FRS §9)', () {
    test('a candling record is durable before any network attempt', () async {
      final id = await repo.saveCandling(
        farmId: 'farm-1',
        batchId: 'batch-1',
        userId: 'user-1',
        dayNo: 7,
        fertile: 30,
        clear: 2,
        bloodRing: 0,
        unsure: 0,
      );

      final ops = await db.dueOperations();
      expect(ops, hasLength(1));
      expect(ops.single.id, id);
      expect(ops.single.syncState, SyncState.pending);
      expect(ops.single.entityId, 'batch-1');
    });

    test('carries every §10 metadata field', () async {
      await repo.saveCandling(
        farmId: 'farm-1',
        batchId: 'batch-1',
        userId: 'user-1',
        dayNo: 7,
        fertile: 1,
        clear: 0,
        bloodRing: 0,
        unsure: 0,
      );
      final op = (await db.dueOperations()).single;

      expect(op.id, isNotEmpty); // unique operation id
      expect(op.entityId, 'batch-1'); // entity id
      expect(op.operationType, OperationType.candling); // operation type
      expect(op.createdAt, isNotNull); // creation timestamp
      expect(op.userId, 'user-1'); // local user identity
      expect(op.deviceId, 'test-device'); // device identity
      expect(op.payload, isNotEmpty); // payload
      expect(op.syncState, SyncState.pending); // sync state
      expect(op.retryCount, 0); // retry count
      expect(op.lastAttemptAt, isNull); // last attempt
      expect(op.errorMessage, isNull); // error info
    });

    test('the operation id is sent as the API idempotency key (§11)', () async {
      final id = await repo.saveCandling(
        farmId: 'farm-1',
        batchId: 'batch-1',
        userId: null,
        dayNo: 7,
        fertile: 1,
        clear: 0,
        bloodRing: 0,
        unsure: 0,
      );
      final op = (await db.dueOperations()).single;
      final payload = jsonDecode(op.payload) as Map<String, dynamic>;
      expect(payload['clientId'], id);
    });

    test('payload matches the API candling contract', () async {
      await repo.saveCandling(
        farmId: 'f',
        batchId: 'b',
        userId: null,
        dayNo: 14,
        fertile: 28,
        clear: 3,
        bloodRing: 1,
        unsure: 0,
        discrepancyNote: 'two removed',
      );
      final payload = jsonDecode((await db.dueOperations()).single.payload)
          as Map<String, dynamic>;

      expect(payload.keys, containsAll(<String>[
        'dayNo', 'candledAt', 'fertile', 'clear', 'bloodRing', 'unsure',
        'discrepancyNote', 'clientId',
      ]));
      expect(payload['dayNo'], 14);
      expect(payload['fertile'], 28);
      expect(payload['discrepancyNote'], 'two removed');
      // The API parses candledAt with z.coerce.date(); an ISO-8601 UTC
      // instant is what it expects.
      expect(DateTime.parse(payload['candledAt'] as String).isUtc, isTrue);
    });

    test('an empty discrepancy note is omitted, not sent as ""', () async {
      await repo.saveHatch(
        farmId: 'f',
        batchId: 'b',
        userId: null,
        hatched: 30,
        pippedDead: 0,
        deadInShell: 0,
        unhatched: 0,
        discrepancyNote: '',
      );
      final payload = jsonDecode((await db.dueOperations()).single.payload)
          as Map<String, dynamic>;
      expect(payload.containsKey('discrepancyNote'), isFalse);
    });

    test('records keep capture order', () async {
      await repo.saveCandling(
        farmId: 'f', batchId: 'b', userId: null, dayNo: 7,
        fertile: 1, clear: 0, bloodRing: 0, unsure: 0,
        candledAt: DateTime(2026, 8, 27, 9),
      );
      await repo.saveCandling(
        farmId: 'f', batchId: 'b', userId: null, dayNo: 14,
        fertile: 1, clear: 0, bloodRing: 0, unsure: 0,
        candledAt: DateTime(2026, 9, 3, 9),
      );
      final ops = await db.dueOperations();
      expect(ops.map((o) => o.createdAt).toList(),
          [DateTime(2026, 8, 27, 9), DateTime(2026, 9, 3, 9)]);
    });
  });

  group('sync state machine (FRS §10)', () {
    Future<String> enqueue() => repo.saveCandling(
          farmId: 'f', batchId: 'b', userId: null, dayNo: 7,
          fertile: 1, clear: 0, bloodRing: 0, unsure: 0,
        );

    test('pending → syncing → synced', () async {
      final id = await enqueue();

      await db.markSyncing(id);
      var op = await (db.select(db.pendingOperations)
            ..where((t) => t.id.equals(id)))
          .getSingle();
      expect(op.syncState, SyncState.syncing);
      expect(op.lastAttemptAt, isNotNull);

      await db.markSynced(id);
      op = await (db.select(db.pendingOperations)..where((t) => t.id.equals(id)))
          .getSingle();
      expect(op.syncState, SyncState.synced);
      // A synced record is no longer owed to the server.
      expect(await db.dueOperations(), isEmpty);
    });

    test('a transport failure stays retryable', () async {
      final id = await enqueue();
      await db.markFailed(id, 'connection refused', terminal: false);

      final due = await db.dueOperations();
      expect(due, hasLength(1), reason: 'must be retried when back online');
      expect(due.single.syncState, SyncState.failed);
      expect(due.single.errorMessage, 'connection refused');
    });

    test('a server rejection is terminal and stops retrying', () async {
      final id = await enqueue();
      await db.markFailed(id, 'discrepancy note required', terminal: true);

      expect(await db.dueOperations(), isEmpty,
          reason: 'retrying cannot fix a business-rule rejection');
      // It stays visible to the user so they can act on it.
      final unsynced = await db.watchUnsynced().first;
      expect(unsynced, hasLength(1));
      expect(unsynced.single.isTerminal, isTrue);
    });

    test('retry count increments and is preserved', () async {
      final id = await enqueue();
      await db.incrementRetry(id, 0);
      await db.incrementRetry(id, 1);
      final op = (await db.dueOperations()).single;
      expect(op.retryCount, 2);
    });

    test('a discarded terminal record is gone for good', () async {
      final id = await enqueue();
      await db.markFailed(id, 'rejected', terminal: true);
      await repo.discard(id);
      expect(await db.watchUnsynced().first, isEmpty);
    });
  });

  group('data survives (FRS §19.1)', () {
    test('queued records outlive the repository instance', () async {
      await repo.saveCandling(
        farmId: 'f', batchId: 'b', userId: null, dayNo: 7,
        fertile: 1, clear: 0, bloodRing: 0, unsure: 0,
      );

      // A new repository over the same database is what a restarted app sees.
      final reopened = FieldRecordRepository(db: db, deviceId: 'test-device');
      expect(await reopened.watchUnsynced().first, hasLength(1));
    });

    test('pruning clears synced history but never unsynced work', () async {
      final synced = await repo.saveCandling(
        farmId: 'f', batchId: 'b', userId: null, dayNo: 7,
        fertile: 1, clear: 0, bloodRing: 0, unsure: 0,
        candledAt: DateTime(2026, 1, 1),
      );
      await repo.saveCandling(
        farmId: 'f', batchId: 'b', userId: null, dayNo: 14,
        fertile: 1, clear: 0, bloodRing: 0, unsure: 0,
        candledAt: DateTime(2026, 1, 1),
      );
      await db.markSynced(synced);

      await db.pruneSynced(DateTime(2026, 6, 1));
      final left = await db.watchUnsynced().first;
      expect(left, hasLength(1), reason: 'the un-synced record must remain');
      expect(left.single.syncState, SyncState.pending);
    });
  });

  group('read cache (gap C8)', () {
    test('batches are readable back offline', () async {
      await db.cacheBatches('farm-1', {
        'batch-1': '{"id":"batch-1","status":"incubating"}',
      });
      final cached = await db.cachedBatchesFor('farm-1');
      expect(cached, hasLength(1));
      expect(cached.single.payload, contains('incubating'));
      expect(cached.single.fetchedAt, isNotNull);
    });

    test('re-caching updates rather than duplicating', () async {
      await db.cacheBatches('farm-1', {'b': '{"v":1}'});
      await db.cacheBatches('farm-1', {'b': '{"v":2}'});
      final cached = await db.cachedBatchesFor('farm-1');
      expect(cached, hasLength(1));
      expect(cached.single.payload, '{"v":2}');
    });
  });
}
