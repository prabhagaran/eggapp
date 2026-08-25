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

/// Hatch outcome capture (FRS §13). Same offline-first contract as candling:
/// the save completes locally and syncs later.
class HatchForm extends StatefulWidget {
  final String farmId;
  final String batchId;
  final String batchStatus;
  final int viableCount;

  const HatchForm({
    required this.farmId,
    required this.batchId,
    required this.batchStatus,
    required this.viableCount,
    super.key,
  });

  @override
  State<HatchForm> createState() => _HatchFormState();
}

class _HatchFormState extends State<HatchForm> {
  int _hatched = 0;
  int _pippedDead = 0;
  int _deadInShell = 0;
  int _unhatched = 0;
  final _note = TextEditingController();
  bool _saving = false;

  @override
  void initState() {
    super.initState();
    // Start with every viable egg hatched — the outcome being hoped for, and
    // the one that makes BR-003 balance without further input.
    _hatched = widget.viableCount;
  }

  @override
  void dispose() {
    _note.dispose();
    super.dispose();
  }

  int get _discrepancy => hatchDiscrepancy(
        viableCount: widget.viableCount,
        hatched: _hatched,
        pippedDead: _pippedDead,
        deadInShell: _deadInShell,
        unhatched: _unhatched,
      );

  bool get _needsNote => _discrepancy != 0;

  Future<void> _save() async {
    if (_needsNote && _note.text.trim().isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(
            'Counts total ${widget.viableCount + _discrepancy} but '
            '${widget.viableCount} eggs are viable. Add a note.',
          ),
        ),
      );
      return;
    }

    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Record hatch?'),
        // Hatch closes out the batch server-side, so unlike candling this is
        // not a routine repeatable entry — worth one confirmation.
        content: Text(
          'This completes the batch with $_hatched hatched of '
          '${widget.viableCount} viable.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(ctx).pop(true),
            child: const Text('Record hatch'),
          ),
        ],
      ),
    );
    if (confirmed != true || !mounted) return;

    setState(() => _saving = true);
    final repo = context.read<FieldRecordRepository>();
    final session = context.read<Session>();
    try {
      await repo.saveHatch(
        farmId: widget.farmId,
        batchId: widget.batchId,
        userId: session.me?.id,
        hatched: _hatched,
        pippedDead: _pippedDead,
        deadInShell: _deadInShell,
        unhatched: _unhatched,
        discrepancyNote: _note.text.trim(),
      );
      if (!mounted) return;
      unawaited(context.read<SyncEngine>().sync());
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Hatch saved. It will sync when online.')),
      );
      setState(() => _saving = false);
    } catch (e) {
      if (!mounted) return;
      setState(() => _saving = false);
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text('Could not save: $e')));
    }
  }

  @override
  Widget build(BuildContext context) {
    final blocked = hatchBlockedReason(widget.batchStatus);
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

    final balanced = _discrepancy == 0;
    return AppCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const SectionHeader('Record hatch'),
          CountStepper(
            label: 'Hatched',
            value: _hatched,
            onChanged: (v) => setState(() => _hatched = v),
          ),
          CountStepper(
            label: 'Pipped, dead',
            value: _pippedDead,
            onChanged: (v) => setState(() => _pippedDead = v),
          ),
          CountStepper(
            label: 'Dead in shell',
            value: _deadInShell,
            onChanged: (v) => setState(() => _deadInShell = v),
          ),
          CountStepper(
            label: 'Unhatched',
            value: _unhatched,
            onChanged: (v) => setState(() => _unhatched = v),
          ),
          const SizedBox(height: 8),
          Container(
            width: double.infinity,
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              color: balanced ? AppColors.okBg : AppColors.warnBg,
              borderRadius: BorderRadius.circular(10),
            ),
            child: Text(
              balanced
                  ? 'Counts balance against ${widget.viableCount} viable.'
                  : 'Counts total ${widget.viableCount + _discrepancy}, but '
                      '${widget.viableCount} are viable. A note is required.',
              style: TextStyle(
                fontSize: 12,
                color: balanced ? AppColors.okText : AppColors.warnText,
              ),
            ),
          ),
          if (_needsNote) ...[
            const SizedBox(height: 12),
            TextField(
              controller: _note,
              minLines: 2,
              maxLines: 3,
              decoration: const InputDecoration(
                labelText: 'Discrepancy note (required)',
              ),
              onChanged: (_) => setState(() {}),
            ),
          ],
          const SizedBox(height: 16),
          FilledButton.icon(
            onPressed: _saving ? null : _save,
            icon: const Icon(Icons.egg_alt_outlined),
            label: Text(_saving ? 'Saving…' : 'Save hatch'),
          ),
        ],
      ),
    );
  }
}
