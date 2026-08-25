import 'package:intl/intl.dart';

/// Display helpers shared across screens. Mirrors the web client's
/// fmtDate/fmtAge/isFresh in apps/web/lib/useAuthedFarm.ts.

final DateFormat _dateFmt = DateFormat.yMd();
final DateFormat _dateTimeFmt = DateFormat.yMd().add_Hm();

/// An em dash, not an empty string — a missing value should read as
/// deliberately absent rather than as a rendering gap.
const String kEmpty = '—';

String fmtDate(DateTime? d) => d == null ? kEmpty : _dateFmt.format(d.toLocal());

String fmtDateTime(DateTime? d) => d == null ? kEmpty : _dateTimeFmt.format(d.toLocal());

/// US-INC-002/ENV-001 target ≤60s freshness; 90s gives one missed-interval
/// margin before flagging stale (device publish cadence is 60s).
bool isFresh(DateTime? ts) {
  if (ts == null) return false;
  return DateTime.now().difference(ts).inSeconds < 90;
}

String fmtAge(DateTime? ts) {
  if (ts == null) return kEmpty;
  final s = DateTime.now().difference(ts).inSeconds;
  if (s < 60) return '${s}s ago';
  final m = s ~/ 60;
  if (m < 60) return '${m}m ago';
  final h = m ~/ 60;
  if (h < 24) return '${h}h ago';
  return '${h ~/ 24}d ago';
}

/// Temperature/humidity readings: null is "no reading", never 0.
String fmtNum(num? v, {int decimals = 1, String suffix = ''}) =>
    v == null ? kEmpty : '${v.toStringAsFixed(decimals)}$suffix';

String fmtPct(num? v) => v == null ? kEmpty : '${v.toStringAsFixed(0)}%';
