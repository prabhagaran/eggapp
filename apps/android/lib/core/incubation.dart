/// Incubation-day arithmetic, kept deliberately in one place so the phone,
/// the web dashboard and the device all agree on what "day N" means.
///
/// The day the eggs are set is **day 1**, and the number rolls over at local
/// midnight — not at the set timestamp's time of day. That matches how a
/// hatchery counts, and it matches the ESP32 firmware's `calcIncubationDay()`
/// (`(now - start) / 86400 + 1`).
///
/// This mirrors `dayOf()` in apps/web/lib/useAuthedFarm.ts. The web version
/// originally used elapsed-milliseconds division, which was both zero-based
/// and time-of-day sensitive: a batch set at 15:30 stayed on "day 0" until
/// 15:30 the next day, and read two days behind the device. Both surfaces now
/// use calendar days so the numbers agree.
library;

/// Incubation day for a batch set at [setAt], as of [now] (defaults to the
/// current time). Returns null when the batch has no set date yet.
int? incubationDay(DateTime? setAt, {DateTime? now}) {
  if (setAt == null) return null;
  final today = _localMidnight(now ?? DateTime.now());
  final start = _localMidnight(setAt.toLocal());
  // Difference between two local midnights, so a DST transition cannot
  // produce a 23- or 25-hour "day" and shift the count.
  final day = today.difference(start).inDays + 1;
  // A batch scheduled ahead of time reads as day 1 rather than 0 or negative,
  // matching the firmware's own `nowEpoch < startEpoch` guard.
  return day < 1 ? 1 : day;
}

DateTime _localMidnight(DateTime d) {
  final local = d.toLocal();
  return DateTime(local.year, local.month, local.day);
}

/// Whether the app's own day count disagrees with the day the device reports.
///
/// A tolerance of 1 absorbs the two clocks being on opposite sides of a
/// midnight boundary. A wider gap means the device was started against a
/// different date than the batch record — worth showing the user.
bool deviceDayMismatch(int? appDay, int? deviceDay) {
  if (appDay == null || deviceDay == null) return false;
  return (appDay - deviceDay).abs() > 1;
}
