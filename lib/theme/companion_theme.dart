import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

class CompanionTheme {
  CompanionTheme._();

  static const Color cream = Color(0xFFF4F4F2);
  static const Color ink = Color(0xFF050505);
  static const Color charcoal = Color(0xFF121212);
  static const Color coral = Color(0xFF111111);
  static const Color brass = Color(0xFF2D2D2D);
  static const Color blush = Color(0xFFD1D1D1);
  static const Color hotPink = Color(0xFFD94C7D);
  static const Color panel = Color(0xFFFFFFFF);
  static const Color surface = Color(0xFFF8F8F8);
  static const Color surfaceRaised = Color(0xFFECECEC);
  static const Color muted = Color(0xFF4C4C4C);

  static ThemeData get themeData {
    final base = ThemeData(useMaterial3: true);
    return base.copyWith(
      scaffoldBackgroundColor: cream,
      colorScheme: ColorScheme.fromSeed(
        seedColor: coral,
        brightness: Brightness.light,
        primary: coral,
        secondary: brass,
        surface: panel,
        onSurface: charcoal,
        onPrimary: Colors.white,
      ),
      textTheme: GoogleFonts.spaceGroteskTextTheme(base.textTheme).copyWith(
        headlineLarge: GoogleFonts.spaceGrotesk(
          fontSize: 34,
          fontWeight: FontWeight.w700,
          color: charcoal,
        ),
        headlineMedium: GoogleFonts.spaceGrotesk(
          fontSize: 24,
          fontWeight: FontWeight.w700,
          color: charcoal,
        ),
        titleMedium: GoogleFonts.spaceGrotesk(
          fontSize: 16,
          fontWeight: FontWeight.w600,
          color: charcoal,
        ),
        titleSmall: GoogleFonts.spaceGrotesk(
          fontSize: 14,
          fontWeight: FontWeight.w600,
          color: charcoal,
        ),
        bodyLarge: GoogleFonts.ibmPlexSans(
          fontSize: 16,
          color: muted,
          height: 1.35,
        ),
        bodyMedium: GoogleFonts.ibmPlexSans(
          fontSize: 14,
          color: muted,
          height: 1.35,
        ),
        bodySmall: GoogleFonts.ibmPlexSans(
          fontSize: 13,
          color: muted,
          height: 1.35,
        ),
      ),
      appBarTheme: AppBarTheme(
        backgroundColor: Colors.transparent,
        foregroundColor: charcoal,
        centerTitle: false,
        titleTextStyle: GoogleFonts.spaceGrotesk(
          fontSize: 20,
          fontWeight: FontWeight.w700,
          color: charcoal,
        ),
      ),
      cardTheme: CardThemeData(
        color: panel,
        elevation: 0,
        margin: EdgeInsets.zero,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(24),
          side: const BorderSide(color: blush, width: 1.2),
        ),
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: surface,
        labelStyle: GoogleFonts.ibmPlexSans(color: muted),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(16),
          borderSide: const BorderSide(color: blush),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(16),
          borderSide: const BorderSide(color: blush),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(16),
          borderSide: const BorderSide(color: coral, width: 1.4),
        ),
      ),
      elevatedButtonTheme: ElevatedButtonThemeData(
        style: ElevatedButton.styleFrom(
          backgroundColor: coral,
          foregroundColor: Colors.white,
          padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 14),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(16),
          ),
          textStyle: GoogleFonts.spaceGrotesk(
            fontSize: 14,
            fontWeight: FontWeight.w700,
          ),
        ),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: OutlinedButton.styleFrom(
          foregroundColor: charcoal,
          side: const BorderSide(color: blush),
          padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 14),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(16),
          ),
          textStyle: GoogleFonts.spaceGrotesk(
            fontSize: 14,
            fontWeight: FontWeight.w700,
          ),
        ),
      ),
      snackBarTheme: SnackBarThemeData(
        backgroundColor: charcoal,
        contentTextStyle: GoogleFonts.ibmPlexSans(color: Colors.white),
      ),
      sliderTheme: const SliderThemeData(
        activeTrackColor: coral,
        inactiveTrackColor: blush,
        thumbColor: charcoal,
      ),
      chipTheme: ChipThemeData(
        backgroundColor: surface,
        selectedColor: surfaceRaised,
        secondarySelectedColor: surfaceRaised,
        side: const BorderSide(color: blush),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(999)),
        labelStyle: GoogleFonts.ibmPlexSans(color: charcoal),
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      ),
      dividerColor: blush,
    );
  }
}