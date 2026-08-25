/// Client-side batch eligibility for field records (FRS §13).
///
/// These rules mirror the server's, which remains authoritative — §21 puts
/// business rules in the backend, and a queued record is re-validated when it
/// syncs. They exist here only so the app can tell a user *offline* that a
/// record will not be accepted, rather than letting them fill in a form whose
/// rejection they would discover days later.
///
/// Mirrors `recordCandling` / `recordHatch` in
/// apps/api/src/services/batch.service.ts. If those change, change these.
library;

/// Candling is allowed only while the batch is incubating (BR-002).
bool canRecordCandling(String batchStatus) => batchStatus == 'incubating';

/// Hatch is allowed once the batch is in lockdown or actively hatching.
bool canRecordHatch(String batchStatus) =>
    batchStatus == 'lockdown' || batchStatus == 'hatching';

/// Why a record cannot be captured, phrased for a field user rather than
/// echoing the API's error code.
String? candlingBlockedReason(String batchStatus) {
  if (canRecordCandling(batchStatus)) return null;
  return switch (batchStatus) {
    'planned' || 'setting' => 'Candling starts once the eggs are set.',
    'lockdown' || 'hatching' => 'This batch is past candling — record the hatch instead.',
    _ => 'Candling is not available for a $batchStatus batch.',
  };
}

String? hatchBlockedReason(String batchStatus) {
  if (canRecordHatch(batchStatus)) return null;
  return switch (batchStatus) {
    'incubating' => 'Hatch recording opens at lockdown.',
    'planned' || 'setting' => 'This batch has not started incubating yet.',
    _ => 'Hatch recording is not available for a $batchStatus batch.',
  };
}

/// The next scheduled candling day not yet recorded, used to pre-fill the
/// form — a field user should not have to work out which day they are on.
/// [recordedDays] covers both server-held and locally queued sessions.
int suggestNextCandlingDay(List<int> scheduleDays, Set<int> recordedDays) {
  for (final d in scheduleDays) {
    if (!recordedDays.contains(d)) return d;
  }
  return scheduleDays.isEmpty ? 7 : scheduleDays.last;
}

/// BR-003: the counts entered must reconcile against the batch's viable
/// count, or the user must explain the difference. The server enforces this
/// and rejects without a note — checking here means the user finds out while
/// they are still standing at the incubator, not on sync.
int candlingDiscrepancy({
  required int viableCount,
  required int fertile,
  required int clear,
  required int bloodRing,
  required int unsure,
}) =>
    (fertile + clear + bloodRing + unsure) - viableCount;

int hatchDiscrepancy({
  required int viableCount,
  required int hatched,
  required int pippedDead,
  required int deadInShell,
  required int unhatched,
}) =>
    (hatched + pippedDead + deadInShell + unhatched) - viableCount;
