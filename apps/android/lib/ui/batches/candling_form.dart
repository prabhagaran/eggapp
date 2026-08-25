import 'dart:async';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../core/batch_eligibility.dart';
import '../../core/theme.dart';
import '../../data/field_record_repository.dart';
import '../../data/sync_engine.dart';
import '../../state/session.dart';
import '../components/app_components.dart';
import '../components/count_stepper.dart';

/// Candling capture (FRS §13).
///
/// The save path never touches the network: it writes to the local queue and
/// returns. That is what lets this be used in a shed with no signal, and why
/// the confirmation says "saved" rather than "sent".
class CandlingForm extends StatefulWidget {
  final String farmId;
  final String batchId;
  final String batchStatus;
  final int viableCount;
  final List<int> scheduleDays;
  final Set<int> recordedDays;

  const CandlingForm({
    required this.farmId,
    required this.batchId,
    required this.batchStatus,
    required this.viableCount,
    required this.scheduleDays,
    required this.recordedDays,
    super.key,
  });

  @override
  State<CandlingForm> createState() => _CandlingFormState();
}

class _CandlingFormState extends State<CandlingForm> {
  late int _dayNo;
  int _fertile = 0;
  int _clear = 0;
  int _bloodRing = 0;
  int _unsure = 0;
  final _note = TextEditingController();
  bool _saving = false;

  @override
  void initState() {
    super.initState();
    _dayNo = suggestNextCandlingDay(widget.scheduleDays, widget.recordedDays);
    // Pre-fill fertile with the current viable count: at most candlings the
    // majority are fertile, so the user adjusts down rather than counting up
    // from zero. It also makes the BR-003 reconciliation start balanced.
    _fertile = widget.viableCount;
  }

  @override
  void dispose() {
    _note.dispose();
    super.dispose();
  }

  int get _discrepancy => candlingDiscrepancy(
        viableCount: widget.viableCount,
        fertile: _fertile,
        clear: _clear,
        bloodRing: _bloodRing,
        unsure: _unsure,
      );

  bool get _needsNote => _discrepancy != 0;

  Future<void> _save() async {
    // BR-003 is enforced server-side; checking here means the user learns
    // about it while still at the incubator rather than on sync, days later.
    if (_needsNote && _note.text.trim().isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            'Counts total ${widget.viableCount + _discrepancy} but '
            '${widget.viableCount} eggs are viable. Add a note explaining the '
            'difference.',
          ),
        ),
      );
      return;
    }

    setState(() => _saving = true);
    final repo = context.read<FieldRecordRepository>();
    final session = context.read<Session>();
    try {
      await repo.saveCandling(
        farmId: widget.farmId,
        batchId: widget.batchId,
        userId: session.me?.id,
        dayNo: _dayNo,
        fertile: _fertile,
        clear: _clear,
        bloodRing: _bloodRing,
        unsure: _unsure,
        discrepancyNote: _note.text.trim(),
      );
      if (!mounted) return;
      // Saving is complete at this point regardless of connectivity; the sync
      // attempt below is opportunistic.
      unawaited(context.read<SyncEngine>().sync());
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Candling saved. It will sync when online.')),
      );
      setState(() {
        // Reset to the same balanced default the form opened with, fertile
        // included. Zeroing only the loss counts would leave fertile at the
        // value just submitted and immediately show a false "counts don't
        // balance" warning — alarming, and about a record that saved fine.
        _fertile = widget.viableCount;
        _clear = 0;
        _bloodRing = 0;
        _unsure = 0;
        _note.clear();
        _saving = false;
      });
    } catch (e) {
      if (!mounted) return;
      setState(() => _saving = false);
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text('Could not save: $e')));
    }
  }

  @override
  Widget build(BuildContext context) {
    final blocked = candlingBlockedReason(widget.batchStatus);
    if (blocked != null) {
      return AppCard(
        child: Row(
          children: [
            const Icon(Icons.info_outline, color: AppColors.muted),
            const SizedBox(width: 12),
            Expanded(
              child: Text(blocked,
                  style: const TextStyle(color: AppColors.muted, fontSize: 13)),
            ),
          ],
        ),
      );
    }

    return AppCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const SectionHeader('Record candling'),
          Row(
            children: [
              const Text('Day', style: TextStyle(fontSize: 15)),
              const SizedBox(width: 16),
              Expanded(
                child: Wrap(
                  spacing: 8,
                  children: [
                    for (final d in widget.scheduleDays)
                      ChoiceChip(
                        label: Text('$d'),
                        selected: _dayNo == d,
                        onSelected: (_) => setState(() => _dayNo = d),
                        // Days already recorded stay selectable — a correction
                        // is a legitimate second session, and the server keys
                        // on clientId so it will not collide.
                        avatar: widget.recordedDays.contains(d)
                            ? const Icon(Icons.check, size: 16)
                            : null,
                      ),
                  ],
                ),
              ),
            ],
          ),
          const Divider(height: 24),
          CountStepper(
            label: 'Fertile',
            value: _fertile,
            onChanged: (v) => setState(() => _fertile = v),
          ),
          CountStepper(
            label: 'Clear (infertile)',
            value: _clear,
            onChanged: (v) => setState(() => _clear = v),
          ),
          CountStepper(
            label: 'Blood ring',
            value: _bloodRing,
            onChanged: (v) => setState(() => _bloodRing = v),
          ),
          CountStepper(
            label: 'Unsure',
            value: _unsure,
            onChanged: (v) => setState(() => _unsure = v),
          ),
          const SizedBox(height: 8),
          _Reconciliation(
            viable: widget.viableCount,
            entered: widget.viableCount + _discrepancy,
            discrepancy: _discrepancy,
          ),
          if (_needsNote) ...[
            const SizedBox(height: 12),
            TextField(
              controller: _note,
              minLines: 2,
              maxLines: 3,
              decoration: const InputDecoration(
                labelText: 'Discrepancy note (required)',
                hintText: 'e.g. 2 eggs cracked and removed',
              ),
              onChanged: (_) => setState(() {}),
            ),
          ],
          const SizedBox(height: 16),
          FilledButton.icon(
            onPressed: _saving ? null : _save,
            icon: const Icon(Icons.save_outlined),
            label: Text(_saving ? 'Saving…' : 'Save candling'),
          ),
        ],
      ),
    );
  }
}

/// Live BR-003 reconciliation, so the user sees the arithmetic as they enter
/// it instead of hitting a rejection at the end.
class _Reconciliation extends StatelessWidget {
  final int viable;
  final int entered;
  final int discrepancy;

  const _Reconciliation({
    required this.viable,
    required this.entered,
    required this.discrepancy,
  });

  @override
  Widget build(BuildContext context) {
    final ok = discrepancy == 0;
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(10),
      decoration: BoxDecoration(
        color: ok ? AppColors.okBg : AppColors.warnBg,
        borderRadius: BorderRadius.circular(10),
      ),
      child: Text(
        ok
            ? 'Counts balance: $entered of $viable viable.'
            : 'Counts total $entered, but $viable are viable '
                '(${discrepancy > 0 ? "+" : ""}$discrepancy). A note is required.',
        style: TextStyle(
          fontSize: 12,
          color: ok ? AppColors.okText : AppColors.warnText,
        ),
      ),
    );
  }
}
