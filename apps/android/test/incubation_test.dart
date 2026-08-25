import 'package:eggapp_field/core/incubation.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('incubationDay', () {
    // The batch that exposed the original bug: set 2026-08-21 15:30, the
    // device reported day 3 on 2026-08-23, and the web dashboard said day 1.
    // The device was right — 8/21 is day 1.
    final setAt = DateTime(2026, 8, 21, 15, 30);

    test('the day the eggs are set is day 1', () {
      expect(incubationDay(setAt, now: DateTime(2026, 8, 21, 15, 31)), 1);
    });

    test('stays on day 1 for the rest of the set day', () {
      // The old elapsed-milliseconds formula stayed on its number until 15:30
      // the following day. Late evening on the set date is still day 1.
      expect(incubationDay(setAt, now: DateTime(2026, 8, 21, 23, 59)), 1);
    });

    test('rolls over at local midnight, not at the set time of day', () {
      expect(incubationDay(setAt, now: DateTime(2026, 8, 22, 0, 1)), 2);
      // 09:00 is before 15:30, yet it is unambiguously the next day.
      expect(incubationDay(setAt, now: DateTime(2026, 8, 22, 9, 0)), 2);
    });

    test('agrees with the device day for the batch that exposed the bug', () {
      expect(incubationDay(setAt, now: DateTime(2026, 8, 23, 14, 2)), 3);
    });

    test('reaches the species incubation length on the expected hatch date', () {
      // Chicken: 21 days. setAt + 21d is the expected hatch date.
      expect(incubationDay(setAt, now: DateTime(2026, 9, 11, 10, 0)), 22);
    });

    test('a batch scheduled for the future reads as day 1, never 0 or negative', () {
      final future = DateTime(2026, 9, 1, 10, 0);
      expect(incubationDay(future, now: DateTime(2026, 8, 23, 14, 2)), 1);
    });

    test('returns null when the batch has no set date', () {
      expect(incubationDay(null, now: DateTime(2026, 8, 23)), isNull);
    });
  });

  group('deviceDayMismatch', () {
    test('agreeing counters are not a mismatch', () {
      expect(deviceDayMismatch(3, 3), isFalse);
    });

    test('one day apart is tolerated as a midnight-boundary difference', () {
      expect(deviceDayMismatch(3, 4), isFalse);
      expect(deviceDayMismatch(4, 3), isFalse);
    });

    test('two or more days apart is a real mismatch', () {
      expect(deviceDayMismatch(1, 3), isTrue);
      expect(deviceDayMismatch(5, 2), isTrue);
    });

    test('a missing counter on either side is never a mismatch', () {
      expect(deviceDayMismatch(null, 3), isFalse);
      expect(deviceDayMismatch(3, null), isFalse);
    });
  });
}
