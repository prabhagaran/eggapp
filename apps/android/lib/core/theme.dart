import 'package:flutter/material.dart';

/// Brand palette lifted from apps/web/app/globals.css — same product, same
/// colours, three surfaces. The hex values are the web custom properties:
/// --accent #1f7a4d, --accent-dark #123524, --text #17231b.
class AppColors {
  const AppColors._();

  static const accent = Color(0xFF1F7A4D);
  static const accentDark = Color(0xFF123524);
  static const text = Color(0xFF17231B);
  static const muted = Color(0xFF77816F);
  static const background = Color(0xFFF3F5F1);
  static const surface = Color(0xFFFFFFFF);
  static const surfaceAlt = Color(0xFFF6F7F4);
  static const outline = Color(0xFFE8EBE3);

  // Semantic status tints, kept separate from the brand scheme because they
  // mean ok/warn/danger rather than "this is eggAPP green".
  static const okBg = Color(0xFFE1F5E7);
  static const okText = Color(0xFF1F7A4D);
  static const warnBg = Color(0xFFFDEFD3);
  static const warnText = Color(0xFFA6690B);
  static const dangerBg = Color(0xFFFCE1DE);
  static const dangerText = Color(0xFFB3261E);
  static const neutralBg = Color(0xFFEEF0EC);
  static const neutralText = Color(0xFF6B7267);
}

/// Always light, regardless of system setting — carried over from the Kotlin
/// app's EggAppTheme. A field app is used outdoors in daylight; a dark theme
/// that the OS flips on at dusk would be harder to read in a shed, not easier.
ThemeData buildAppTheme() {
  const scheme = ColorScheme.light(
    primary: AppColors.accent,
    onPrimary: Colors.white,
    primaryContainer: Color(0xFFE3F3E7),
    onPrimaryContainer: AppColors.accentDark,
    secondary: AppColors.accentDark,
    onSecondary: Colors.white,
    surface: AppColors.surface,
    onSurface: AppColors.text,
    surfaceContainerHighest: AppColors.surfaceAlt,
    onSurfaceVariant: AppColors.muted,
    outline: AppColors.outline,
    outlineVariant: AppColors.outline,
    error: AppColors.dangerText,
    onError: Colors.white,
    errorContainer: AppColors.dangerBg,
    onErrorContainer: AppColors.dangerText,
  );

  return ThemeData(
    useMaterial3: true,
    colorScheme: scheme,
    scaffoldBackgroundColor: AppColors.background,
    appBarTheme: const AppBarTheme(
      backgroundColor: AppColors.surface,
      foregroundColor: AppColors.text,
      elevation: 0,
      scrolledUnderElevation: 1,
      centerTitle: false,
    ),
    cardTheme: CardThemeData(
      color: AppColors.surface,
      elevation: 0,
      margin: EdgeInsets.zero,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
        side: const BorderSide(color: AppColors.outline),
      ),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: AppColors.surface,
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(10),
        borderSide: const BorderSide(color: AppColors.outline),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(10),
        borderSide: const BorderSide(color: AppColors.outline),
      ),
    ),
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(
        minimumSize: const Size.fromHeight(48),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
      ),
    ),
    dividerTheme: const DividerThemeData(color: AppColors.outline, space: 1),
  );
}
