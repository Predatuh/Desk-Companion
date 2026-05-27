import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

class CompanionTheme {
  CompanionTheme._();

  // ── Core dark palette ─────────────────────────────────────────────────────
  static const Color background   = Color(0xFF0D0D14);
  static const Color surface      = Color(0xFF1A1624);
  static const Color surfaceRaised = Color(0xFF241E33);
  static const Color accent       = Color(0xFFE0457C);
  static const Color accentBlue   = Color(0xFF00C2FF);
  static const Color muted        = Color(0xFF9090A8);
  static const Color ink          = Color(0xFFFFFFFF);
  static const Color border       = Color(0xFF2E2840);

  // ── Legacy aliases (used throughout studio screen) ────────────────────────
  static const Color cream        = background;
  static const Color charcoal     = Color(0xFF1E1A2E);
  static const Color coral        = accent;
  static const Color brass        = surfaceRaised;
  static const Color blush        = border;
  static const Color hotPink      = accent;
  static const Color panel        = surface;

  // ── Theme ─────────────────────────────────────────────────────────────────
  static ThemeData get themeData {
    final cs = ColorScheme.fromSeed(
      seedColor: accent,
      brightness: Brightness.dark,
    ).copyWith(
      primary: accent,
      onPrimary: Colors.white,
      secondary: accentBlue,
      onSecondary: Colors.black,
      surface: surface,
      onSurface: ink,
      surfaceContainerHighest: surfaceRaised,
      onSurfaceVariant: muted,
      outline: border,
      outlineVariant: const Color(0xFF1E1A2E),
      error: const Color(0xFFCF6679),
      onError: Colors.black,
    );

    return ThemeData(
      colorScheme: cs,
      useMaterial3: true,
      scaffoldBackgroundColor: background,
      // ── Typography ────────────────────────────────────────────────────────
      textTheme: GoogleFonts.ibmPlexSansTextTheme(
        const TextTheme(
          displayLarge:   TextStyle(color: ink,  fontWeight: FontWeight.w700),
          displayMedium:  TextStyle(color: ink,  fontWeight: FontWeight.w700),
          displaySmall:   TextStyle(color: ink,  fontWeight: FontWeight.w600),
          headlineLarge:  TextStyle(color: ink,  fontWeight: FontWeight.w700),
          headlineMedium: TextStyle(color: ink,  fontWeight: FontWeight.w700),
          headlineSmall:  TextStyle(color: ink,  fontWeight: FontWeight.w600),
          titleLarge:     TextStyle(color: ink,  fontWeight: FontWeight.w700),
          titleMedium:    TextStyle(color: ink,  fontWeight: FontWeight.w600),
          titleSmall:     TextStyle(color: ink,  fontWeight: FontWeight.w600),
          bodyLarge:      TextStyle(color: ink,  fontWeight: FontWeight.w400),
          bodyMedium:     TextStyle(color: ink,  fontWeight: FontWeight.w400),
          bodySmall:      TextStyle(color: muted, fontWeight: FontWeight.w400),
          labelLarge:     TextStyle(color: ink,  fontWeight: FontWeight.w600),
          labelMedium:    TextStyle(color: muted, fontWeight: FontWeight.w500),
          labelSmall:     TextStyle(color: muted, fontWeight: FontWeight.w400),
        ),
      ).copyWith(
        displayLarge:   GoogleFonts.spaceGrotesk(color: ink,  fontWeight: FontWeight.w800, fontSize: 57),
        displayMedium:  GoogleFonts.spaceGrotesk(color: ink,  fontWeight: FontWeight.w700, fontSize: 45),
        headlineLarge:  GoogleFonts.spaceGrotesk(color: ink,  fontWeight: FontWeight.w700, fontSize: 32),
        headlineMedium: GoogleFonts.spaceGrotesk(color: ink,  fontWeight: FontWeight.w700, fontSize: 24),
        headlineSmall:  GoogleFonts.spaceGrotesk(color: ink,  fontWeight: FontWeight.w600, fontSize: 20),
        titleLarge:     GoogleFonts.spaceGrotesk(color: ink,  fontWeight: FontWeight.w700, fontSize: 18),
        titleMedium:    GoogleFonts.spaceGrotesk(color: ink,  fontWeight: FontWeight.w600, fontSize: 16),
        titleSmall:     GoogleFonts.spaceGrotesk(color: ink,  fontWeight: FontWeight.w600, fontSize: 14),
      ),
      // ── AppBar ────────────────────────────────────────────────────────────
      appBarTheme: AppBarTheme(
        backgroundColor: background,
        foregroundColor: ink,
        elevation: 0,
        scrolledUnderElevation: 0,
        titleTextStyle: GoogleFonts.spaceGrotesk(
          color: ink,
          fontSize: 18,
          fontWeight: FontWeight.w700,
        ),
      ),
      // ── Card ─────────────────────────────────────────────────────────────
      cardTheme: CardThemeData(
        color: surface,
        elevation: 0,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: const BorderSide(color: border),
        ),
      ),
      // ── NavigationBar ─────────────────────────────────────────────────────
      navigationBarTheme: NavigationBarThemeData(
        backgroundColor: surface,
        indicatorColor: accent.withValues(alpha: 0.2),
        iconTheme: WidgetStateProperty.resolveWith((states) {
          if (states.contains(WidgetState.selected)) {
            return const IconThemeData(color: accent, size: 22);
          }
          return const IconThemeData(color: muted, size: 22);
        }),
        labelTextStyle: WidgetStateProperty.resolveWith((states) {
          if (states.contains(WidgetState.selected)) {
            return GoogleFonts.ibmPlexSans(
              color: accent, fontSize: 11, fontWeight: FontWeight.w600,
            );
          }
          return GoogleFonts.ibmPlexSans(
            color: muted, fontSize: 11, fontWeight: FontWeight.w400,
          );
        }),
        height: 68,
        elevation: 0,
        overlayColor: WidgetStateProperty.all(Colors.transparent),
      ),
      // ── Chips ────────────────────────────────────────────────────────────
      chipTheme: ChipThemeData(
        backgroundColor: surfaceRaised,
        selectedColor: accent.withValues(alpha: 0.18),
        secondarySelectedColor: accent.withValues(alpha: 0.18),
        side: const BorderSide(color: border),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(999),
        ),
        labelStyle: GoogleFonts.ibmPlexSans(
          color: muted,
          fontSize: 13,
          fontWeight: FontWeight.w500,
        ),
        secondaryLabelStyle: GoogleFonts.ibmPlexSans(
          color: accent,
          fontSize: 13,
          fontWeight: FontWeight.w600,
        ),
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        checkmarkColor: accent,
        showCheckmark: false,
      ),
      // ── Buttons ──────────────────────────────────────────────────────────
      elevatedButtonTheme: ElevatedButtonThemeData(
        style: ElevatedButton.styleFrom(
          backgroundColor: accent,
          foregroundColor: Colors.white,
          elevation: 0,
          padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 14),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(14),
          ),
          textStyle: GoogleFonts.spaceGrotesk(
            fontSize: 14,
            fontWeight: FontWeight.w700,
          ),
        ),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: OutlinedButton.styleFrom(
          foregroundColor: accent,
          side: const BorderSide(color: accent),
          padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 14),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(14),
          ),
          textStyle: GoogleFonts.spaceGrotesk(
            fontSize: 14,
            fontWeight: FontWeight.w700,
          ),
        ),
      ),
      textButtonTheme: TextButtonThemeData(
        style: TextButton.styleFrom(
          foregroundColor: accent,
          textStyle: GoogleFonts.ibmPlexSans(
            fontSize: 13,
            fontWeight: FontWeight.w600,
          ),
        ),
      ),
      // ── Input ────────────────────────────────────────────────────────────
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: surfaceRaised,
        hintStyle: const TextStyle(color: muted),
        labelStyle: const TextStyle(color: muted),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: const BorderSide(color: border),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: const BorderSide(color: border),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: const BorderSide(color: accent, width: 1.5),
        ),
        contentPadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      ),
      // ── Slider ───────────────────────────────────────────────────────────
      sliderTheme: const SliderThemeData(
        activeTrackColor: accent,
        inactiveTrackColor: border,
        thumbColor: ink,
        overlayColor: Color(0x33E0457C),
        trackHeight: 3,
      ),
      // ── Divider ──────────────────────────────────────────────────────────
      dividerColor: border,
      dividerTheme: const DividerThemeData(color: border, space: 1, thickness: 1),
      // ── Snackbar ─────────────────────────────────────────────────────────
      snackBarTheme: SnackBarThemeData(
        backgroundColor: surfaceRaised,
        contentTextStyle: GoogleFonts.ibmPlexSans(color: ink),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        behavior: SnackBarBehavior.floating,
      ),
      // ── ExpansionTile ────────────────────────────────────────────────────
      expansionTileTheme: const ExpansionTileThemeData(
        collapsedIconColor: muted,
        iconColor: accent,
        textColor: ink,
        collapsedTextColor: ink,
        backgroundColor: Colors.transparent,
        collapsedBackgroundColor: Colors.transparent,
      ),
      // ── Switch ──────────────────────────────────────────────────────────
      switchTheme: SwitchThemeData(
        thumbColor: WidgetStateProperty.resolveWith((s) =>
            s.contains(WidgetState.selected) ? accent : muted),
        trackColor: WidgetStateProperty.resolveWith((s) =>
            s.contains(WidgetState.selected)
                ? accent.withValues(alpha: 0.4)
                : surfaceRaised),
      ),
    );
  }
}
