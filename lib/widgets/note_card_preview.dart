import 'dart:math' as math;

import 'package:flutter/material.dart';

/// 128×64 preview approximating the firmware's GFX rendering,
/// including optional native-style border and icon decorations.
class NoteCardPreview extends StatelessWidget {
  const NoteCardPreview({
    super.key,
    required this.text,
    required this.fontSize,
    this.border = 0,
    this.icons = const [],
  });

  final String text;
  final int fontSize;
  /// 0=none 1=rounded 2=stitched 3=hearts 4=dots  (mirrors firmware)
  final int border;
  /// List of icon names: 'heart', 'star', 'flower', 'note', 'moon'
  final List<String> icons;

  @override
  Widget build(BuildContext context) {
    final noteText = text.trim().isEmpty ? 'Your note here' : text;
    final hasIcons = icons.isNotEmpty;

    final double textPx = switch (fontSize) {
      >= 4 => 24,
      3 => 16,
      2 => 12,
      _ => 8,
    };

    return SizedBox(
      width: 128,
      height: 64,
      child: CustomPaint(
        painter: _NoteBorderPainter(border: border),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            if (hasIcons) ...[
              const SizedBox(height: 4),
              _IconRow(icons: icons),
              const SizedBox(height: 2),
            ],
            Expanded(
              child: Center(
                child: Padding(
                  padding: EdgeInsets.symmetric(
                    horizontal: border > 0 ? 8 : 4,
                    vertical: 2,
                  ),
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
          ],
        ),
      ),
    );
  }
}

// ─── Border painter ───────────────────────────────────────────────────────────

class _NoteBorderPainter extends CustomPainter {
  const _NoteBorderPainter({required this.border});
  final int border;

  static final _white = Paint()..color = Colors.white;
  static final _black = Paint()..color = Colors.black;

  @override
  void paint(Canvas canvas, Size size) {
    // Fill background
    canvas.drawRect(Rect.fromLTWH(0, 0, size.width, size.height), _black);

    switch (border) {
      case 1: // rounded outline
        canvas.drawRRect(
          RRect.fromRectAndRadius(
            Rect.fromLTWH(1, 1, size.width - 2, size.height - 2),
            const Radius.circular(8),
          ),
          _white..style = PaintingStyle.stroke,
        );
        break;
      case 2: // stitched dashes
        final p = _white..style = PaintingStyle.stroke..strokeWidth = 1;
        for (double x = 6; x < size.width - 6; x += 7) {
          canvas.drawLine(Offset(x, 3), Offset(x + 3, 3), p);
          canvas.drawLine(Offset(x, size.height - 4), Offset(x + 3, size.height - 4), p);
        }
        for (double y = 6; y < size.height - 6; y += 7) {
          canvas.drawLine(Offset(3, y), Offset(3, y + 3), p);
          canvas.drawLine(Offset(size.width - 4, y), Offset(size.width - 4, y + 3), p);
        }
        break;
      case 3: // hearts in corners
        _drawHeart(canvas, const Offset(9, 9), 3);
        _drawHeart(canvas, Offset(size.width - 9, 9), 3);
        _drawHeart(canvas, Offset(9, size.height - 9), 3);
        _drawHeart(canvas, Offset(size.width - 9, size.height - 9), 3);
        break;
      case 4: // dot border
        final p = Paint()..color = Colors.white;
        for (double x = 5; x < size.width; x += 6) {
          canvas.drawCircle(Offset(x, 3), 1, p);
          canvas.drawCircle(Offset(x, size.height - 4), 1, p);
        }
        for (double y = 9; y < size.height - 6; y += 6) {
          canvas.drawCircle(Offset(3, y), 1, p);
          canvas.drawCircle(Offset(size.width - 4, y), 1, p);
        }
        break;
    }
  }

  void _drawHeart(Canvas canvas, Offset center, double s) {
    final path = Path();
    final cx = center.dx, cy = center.dy;
    path.addOval(Rect.fromCircle(center: Offset(cx - s, cy - s / 3), radius: s));
    path.addOval(Rect.fromCircle(center: Offset(cx + s, cy - s / 3), radius: s));
    path.addPolygon([
      Offset(cx - s * 2, cy - s / 3 + 1),
      Offset(cx + s * 2, cy - s / 3 + 1),
      Offset(cx, cy + s * 2),
    ], true);
    canvas.drawPath(path, _white..style = PaintingStyle.fill);
  }

  @override
  bool shouldRepaint(_NoteBorderPainter old) => old.border != border;
}

// ─── Icon row ─────────────────────────────────────────────────────────────────

class _IconRow extends StatelessWidget {
  const _IconRow({required this.icons});
  final List<String> icons;

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: icons
          .take(8)
          .map((name) => Padding(
                padding: const EdgeInsets.symmetric(horizontal: 2),
                child: CustomPaint(
                  size: const Size(10, 10),
                  painter: _IconPainter(name: name),
                ),
              ))
          .toList(growable: false),
    );
  }
}

class _IconPainter extends CustomPainter {
  const _IconPainter({required this.name});
  final String name;

  static final _white = Paint()..color = Colors.white..style = PaintingStyle.fill;

  @override
  void paint(Canvas canvas, Size size) {
    final cx = size.width / 2, cy = size.height / 2;
    switch (name) {
      case 'heart':
        final s = size.width / 4;
        final path = Path()
          ..addOval(Rect.fromCircle(center: Offset(cx - s, cy - s / 3), radius: s))
          ..addOval(Rect.fromCircle(center: Offset(cx + s, cy - s / 3), radius: s))
          ..addPolygon([
            Offset(cx - s * 2, cy - s / 3 + 1),
            Offset(cx + s * 2, cy - s / 3 + 1),
            Offset(cx, cy + s * 2),
          ], true);
        canvas.drawPath(path, _white);
        break;
      case 'star':
        final starPath = Path();
        final r = size.width / 2;
        for (int i = 0; i < 6; i++) {
          final angle = i * math.pi / 3.0;
          final x2 = cx + r * 0.95 * math.cos(angle);
          final y2 = cy + r * 0.95 * math.sin(angle);
          starPath.moveTo(cx, cy);
          starPath.lineTo(x2, y2);
        }
        canvas.drawPath(starPath, _white..strokeWidth = 1.5..style = PaintingStyle.stroke);
        canvas.drawCircle(Offset(cx, cy), r / 3, _white..style = PaintingStyle.fill);
        break;
      case 'flower':
        final pr = size.width / 4;
        for (int i = 0; i < 5; i++) {
          final angle = i * math.pi * 2 / 5;
          canvas.drawCircle(
            Offset(cx + pr * math.cos(angle), cy + pr * math.sin(angle)),
            pr,
            _white,
          );
        }
        canvas.drawCircle(Offset(cx, cy), pr + 1, _white);
        break;
      case 'note':
        final np = Paint()..color = Colors.white..style = PaintingStyle.fill;
        canvas.drawCircle(Offset(cx - 1, cy + 2), 2, np);
        canvas.drawLine(
          Offset(cx + 1, cy + 2), Offset(cx + 1, cy - 3),
          np..strokeWidth = 1.5..style = PaintingStyle.stroke,
        );
        canvas.drawLine(
          Offset(cx + 1, cy - 3), Offset(cx + 3.5, cy - 2),
          np,
        );
        break;
      case 'moon':
        canvas.drawCircle(Offset(cx, cy), size.width / 2 - 0.5, _white);
        canvas.drawCircle(
          Offset(cx + size.width / 4, cy - size.width / 5),
          size.width / 2 - 1,
          Paint()..color = Colors.black..style = PaintingStyle.fill,
        );
        break;
    }
  }

  @override
  bool shouldRepaint(_IconPainter old) => old.name != name;
}

