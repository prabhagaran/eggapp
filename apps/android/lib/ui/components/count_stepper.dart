import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../../core/theme.dart';

/// Count entry for field use (FRS §19.4).
///
/// Large +/- targets rather than a bare text field: the person using this is
/// at an incubator, often with gloves on and wet hands, counting eggs. The
/// number stays typable for large corrections, but the common case — nudging
/// a count up one at a time — never opens a keyboard.
class CountStepper extends StatelessWidget {
  final String label;
  final int value;
  final ValueChanged<int> onChanged;
  final int min;
  final int max;

  const CountStepper({
    required this.label,
    required this.value,
    required this.onChanged,
    this.min = 0,
    this.max = 100000,
    super.key,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        children: [
          Expanded(
            child: Text(label, style: const TextStyle(fontSize: 15)),
          ),
          _Btn(
            icon: Icons.remove,
            // Disabled rather than hidden at the boundary, so the control
            // does not change shape under the user's thumb.
            onTap: value > min ? () => onChanged(value - 1) : null,
            semanticLabel: 'Decrease $label',
          ),
          SizedBox(
            width: 72,
            child: TextField(
              key: ValueKey('$label-$value'),
              controller: TextEditingController(text: '$value')
                ..selection = TextSelection.collapsed(offset: '$value'.length),
              textAlign: TextAlign.center,
              keyboardType: TextInputType.number,
              inputFormatters: [FilteringTextInputFormatter.digitsOnly],
              style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w700),
              decoration: const InputDecoration(
                contentPadding: EdgeInsets.symmetric(vertical: 8),
                isDense: true,
              ),
              onChanged: (t) {
                final n = int.tryParse(t);
                if (n != null && n >= min && n <= max) onChanged(n);
              },
            ),
          ),
          _Btn(
            icon: Icons.add,
            onTap: value < max ? () => onChanged(value + 1) : null,
            semanticLabel: 'Increase $label',
          ),
        ],
      ),
    );
  }
}

class _Btn extends StatelessWidget {
  final IconData icon;
  final VoidCallback? onTap;
  final String semanticLabel;

  const _Btn({required this.icon, required this.onTap, required this.semanticLabel});

  @override
  Widget build(BuildContext context) {
    return Semantics(
      label: semanticLabel,
      button: true,
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(10),
        // 48dp minimum — the Material touch-target floor, and the practical
        // floor for a gloved thumb.
        child: Container(
          width: 48,
          height: 48,
          decoration: BoxDecoration(
            color: onTap == null ? AppColors.neutralBg : AppColors.surfaceAlt,
            borderRadius: BorderRadius.circular(10),
            border: Border.all(color: AppColors.outline),
          ),
          child: Icon(
            icon,
            size: 22,
            color: onTap == null ? AppColors.muted : AppColors.accentDark,
          ),
        ),
      ),
    );
  }
}
