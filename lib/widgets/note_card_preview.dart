import 'package:flutter/material.dart';

/// Simple 128×64 text-only preview that approximates
/// the firmware's Adafruit GFX bitmap font rendering.
class NoteCardPreview extends StatelessWidget {
  const NoteCardPreview({
    super.key,
    required this.text,
    required this.fontSize,
  });

  final String text;
  final int fontSize;

  @override
  Widget build(BuildContext context) {
    final noteText = text.trim().isEmpty ? 'Your note here' : text;

    // Approximate Adafruit GFX textSize pixel heights within 128×64
    final double textPx = switch (fontSize) {
      >= 4 => 24,
      3 => 16,
      2 => 12,
      _ => 8,
    };

    return SizedBox(
      width: 128,
      height: 64,
      child: DecoratedBox(
        decoration: const BoxDecoration(color: Colors.black),
        child: Center(
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 4, vertical: 2),
            child: Text(
              noteText,
              textAlign: TextAlign.center,
              style: TextStyle(
                color: Colors.white,
                fontSize: textPx,
                fontFamily: 'monospace',
                fontWeight: FontWeight.w700,
                height: 1.2,
              ),
              maxLines: switch (fontSize) {
                >= 4 => 2,
                3 => 3,
                2 => 4,
                _ => 6,
              },
              overflow: TextOverflow.clip,
            ),
          ),
        ),
      ),
    );
  }
}
