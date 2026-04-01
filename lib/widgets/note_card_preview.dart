import 'dart:math' as math;

import 'package:flutter/material.dart';

class NoteCardPreview extends StatelessWidget {
  const NoteCardPreview({
    super.key,
    required this.text,
    required this.fontSize,
    this.border = 0,
    this.icons = const [],
    this.flowerAccent,
  });

  final String text;
  final int fontSize;
  final int border;
  final List<String> icons;
  final String? flowerAccent;

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: 128,
      height: 64,
      child: CustomPaint(
        painter: _NoteCardPainter(
          text: text.trim().isEmpty ? 'Your note here' : text.trim(),
          fontSize: fontSize,
          border: border,
          icons: icons,
          flowerAccent: flowerAccent,
        ),
      ),
    );
  }
}

class _NoteCardPainter extends CustomPainter {
  const _NoteCardPainter({
    required this.text,
    required this.fontSize,
    required this.border,
    required this.icons,
    required this.flowerAccent,
  });

  final String text;
  final int fontSize;
  final int border;
  final List<String> icons;
  final String? flowerAccent;

  static const Size _canvasSize = Size(128, 64);

  @override
  void paint(Canvas canvas, Size size) {
    canvas.save();
    canvas.scale(size.width / _canvasSize.width, size.height / _canvasSize.height);

    final background = Paint()
      ..color = Colors.black
      ..style = PaintingStyle.fill;
    canvas.drawRect(Offset.zero & _canvasSize, background);

    final accent = flowerAccent?.trim() ?? '';
    if (accent.isNotEmpty) {
      _drawFlowerAccentCard(canvas, accent);
    } else {
      _drawStandardCard(canvas);
    }

    canvas.restore();
  }

  void _drawStandardCard(Canvas canvas) {
    _drawBorder(canvas, border);
    final safeFontSize = fontSize.clamp(1, 4);
    final hasIcons = icons.isNotEmpty;
    final topPad = hasIcons ? 14 : 4;
    final horizontalPadding = border > 0 ? 8 : 4;
    final availableHeight = 64 - topPad - 4;
    final lineHeight = (8 * safeFontSize) + 2;
    final maxLines = availableHeight ~/ lineHeight;
    final maxChars = (128 - (horizontalPadding * 2)) ~/ (6 * safeFontSize);
    final wrappedLines = _wrapLines(text, maxChars, maxLines);
    final totalHeight = wrappedLines.length * lineHeight;
    var cursorY = topPad + ((availableHeight - totalHeight) ~/ 2);
    if (cursorY < topPad) {
      cursorY = topPad;
    }

    if (hasIcons) {
      _drawNoteIcons(canvas, icons.take(7).toList(growable: false), 7);
    }

    for (final line in wrappedLines) {
      final width = _lineVisualWidth(line, safeFontSize);
      final startX = (128 - width) ~/ 2;
      _drawLineWithSymbols(canvas, line, startX.toDouble(), cursorY.toDouble(), safeFontSize);
      cursorY += lineHeight;
    }
  }

  void _drawFlowerAccentCard(Canvas canvas, String accent) {
    _drawFlowerAccent(canvas, accent);
    final divider = Paint()
      ..color = Colors.white
      ..strokeWidth = 1.2
      ..style = PaintingStyle.stroke;
    canvas.drawLine(const Offset(42, 2), const Offset(42, 61), divider);

    const safeFontSize = 1;
    const textLeft = 46;
    const textAreaWidth = 128 - textLeft - 4;
    const lineHeight = (8 * safeFontSize) + 2;
    const maxLines = (64 - 8) ~/ lineHeight;
    const maxChars = textAreaWidth ~/ (6 * safeFontSize);
    final wrappedLines = _wrapLines(text, maxChars, maxLines);
    final totalHeight = wrappedLines.length * lineHeight;
    var startY = 4 + ((64 - 8 - totalHeight) ~/ 2);
    if (startY < 4) {
      startY = 4;
    }

    for (var index = 0; index < wrappedLines.length; index++) {
      _drawLineWithSymbols(
        canvas,
        wrappedLines[index],
        textLeft.toDouble(),
        (startY + (index * lineHeight)).toDouble(),
        safeFontSize,
      );
    }
  }

  List<String> _wrapLines(String value, int maxChars, int maxLines) {
    final lines = <String>[];
    var remaining = value.trim();
    while (remaining.isNotEmpty && lines.length < maxLines) {
      var lineLength = remaining.length > maxChars ? maxChars : remaining.length;
      if (lineLength < remaining.length) {
        final split = remaining.lastIndexOf(' ', lineLength);
        if (split > 0) {
          lineLength = split;
        }
      }

      var line = remaining.substring(0, lineLength).trim();
      if (line.isEmpty) {
        line = remaining.substring(0, maxChars);
        lineLength = line.length;
      }

      lines.add(line);
      remaining = remaining.substring(lineLength).trim();
    }
    return lines;
  }

  int _lineVisualWidth(String line, int safeFontSize) {
    final charWidth = 6 * safeFontSize;
    var width = 0;
    var index = 0;
    while (index < line.length) {
      if (line[index] == '<' && index + 1 < line.length) {
        if (line[index + 1] == '3') {
          width += charWidth * 2;
          index += 2;
          continue;
        }
        if (index + 2 < line.length && line[index + 2] == '>') {
          final symbol = line[index + 1];
          if (symbol == '*' || symbol == '~' || symbol == 'n' || symbol == 'm') {
            width += charWidth * 2;
            index += 3;
            continue;
          }
        }
      }
      width += charWidth;
      index += 1;
    }
    return width;
  }

  void _drawLineWithSymbols(Canvas canvas, String line, double startX, double startY, int safeFontSize) {
    final charWidth = 6.0 * safeFontSize;
    final charHeight = 8.0 * safeFontSize;
    final iconSize = charHeight - 2;
    var cursorX = startX;
    var index = 0;
    while (index < line.length) {
      if (line[index] == '<' && index + 1 < line.length) {
        if (line[index + 1] == '3') {
          _drawIconHeart(canvas, Offset(cursorX + charWidth, startY + (charHeight / 2)), iconSize / 3);
          cursorX += charWidth * 2;
          index += 2;
          continue;
        }
        if (index + 2 < line.length && line[index + 2] == '>') {
          final symbol = line[index + 1];
          if (symbol == '*') {
            _drawIconStar(canvas, Offset(cursorX + charWidth, startY + (charHeight / 2)), iconSize / 2);
            cursorX += charWidth * 2;
            index += 3;
            continue;
          }
          if (symbol == '~') {
            _drawIconFlower(canvas, Offset(cursorX + charWidth, startY + (charHeight / 2)), iconSize / 3);
            cursorX += charWidth * 2;
            index += 3;
            continue;
          }
          if (symbol == 'n') {
            _drawIconNote(canvas, Offset(cursorX + charWidth, startY + (charHeight / 2)), iconSize / 3);
            cursorX += charWidth * 2;
            index += 3;
            continue;
          }
          if (symbol == 'm') {
            _drawIconMoon(canvas, Offset(cursorX + charWidth, startY + (charHeight / 2)), iconSize / 3);
            cursorX += charWidth * 2;
            index += 3;
            continue;
          }
        }
      }

      _drawGlyph(canvas, line[index], Offset(cursorX, startY), safeFontSize);
      cursorX += charWidth;
      index += 1;
    }
  }

  void _drawGlyph(Canvas canvas, String character, Offset origin, int safeFontSize) {
    final painter = TextPainter(
      text: TextSpan(
        text: character,
        style: TextStyle(
          color: Colors.white,
          fontSize: (7.3 * safeFontSize),
          fontFamily: 'Courier',
          height: 1,
          fontWeight: FontWeight.w700,
        ),
      ),
      textDirection: TextDirection.ltr,
    )..layout(minWidth: 0, maxWidth: 6.0 * safeFontSize);
    painter.paint(canvas, origin.translate(0, -1));
  }

  void _drawBorder(Canvas canvas, int style) {
    final stroke = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1;
    final fill = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;

    switch (style) {
      case 1:
        canvas.drawRRect(
          RRect.fromRectAndRadius(
            const Rect.fromLTWH(1, 1, 126, 62),
            const Radius.circular(8),
          ),
          stroke,
        );
        break;
      case 2:
        for (double x = 6; x < 122; x += 7) {
          canvas.drawLine(Offset(x, 3), Offset(x + 3, 3), stroke);
          canvas.drawLine(Offset(x, 60), Offset(x + 3, 60), stroke);
        }
        for (double y = 6; y < 58; y += 7) {
          canvas.drawLine(Offset(3, y), Offset(3, y + 3), stroke);
          canvas.drawLine(Offset(124, y), Offset(124, y + 3), stroke);
        }
        break;
      case 3:
        _drawIconHeart(canvas, const Offset(9, 9), 3);
        _drawIconHeart(canvas, const Offset(119, 9), 3);
        _drawIconHeart(canvas, const Offset(9, 55), 3);
        _drawIconHeart(canvas, const Offset(119, 55), 3);
        break;
      case 4:
        for (double x = 5; x < 128; x += 6) {
          canvas.drawCircle(Offset(x, 3), 1, fill);
          canvas.drawCircle(Offset(x, 60), 1, fill);
        }
        for (double y = 9; y < 58; y += 6) {
          canvas.drawCircle(Offset(3, y), 1, fill);
          canvas.drawCircle(Offset(124, y), 1, fill);
        }
        break;
    }
  }

  void _drawNoteIcons(Canvas canvas, List<String> iconNames, double y) {
    if (iconNames.isEmpty) {
      return;
    }
    final drawCount = math.min(iconNames.length, 7);
    final totalWidth = drawCount * 14;
    final startX = ((128 - totalWidth) / 2) + 7;
    for (var index = 0; index < drawCount; index++) {
      final center = Offset(startX + (index * 14), y);
      switch (iconNames[index]) {
        case 'heart':
          _drawIconHeart(canvas, center, 3);
        case 'star':
          _drawIconStar(canvas, center, 5);
        case 'flower':
          _drawIconFlower(canvas, center, 4);
        case 'note':
          _drawIconNote(canvas, center, 5);
        case 'moon':
          _drawIconMoon(canvas, center, 4);
      }
    }
  }

  void _drawFlowerAccent(Canvas canvas, String accent) {
    switch (accent) {
      case 'sunflower':
        _drawSunflower(canvas, const Offset(20, 30), 4);
      case 'king_protea':
        _drawKingProtea(canvas, const Offset(20, 30), 4);
      default:
        _drawRose(canvas, const Offset(20, 30), 4);
    }
  }

  void _drawRose(Canvas canvas, Offset center, double scale) {
    final fill = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;
    final stroke = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1;
    canvas.drawLine(Offset(center.dx, center.dy + 6 * scale), Offset(center.dx, 58), stroke);
    for (var layer = 0; layer < 4; layer++) {
      final radius = (2 + layer) * scale;
      canvas.drawCircle(Offset(center.dx, center.dy - (layer * scale)), radius, stroke);
    }
    canvas.drawCircle(center, 2.5 * scale, fill);
    canvas.drawOval(Rect.fromCenter(center: Offset(center.dx - 7 * scale, center.dy + 12 * scale), width: 10 * scale, height: 4 * scale), stroke);
    canvas.drawOval(Rect.fromCenter(center: Offset(center.dx + 7 * scale, center.dy + 12 * scale), width: 10 * scale, height: 4 * scale), stroke);
  }

  void _drawSunflower(Canvas canvas, Offset center, double scale) {
    final fill = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;
    final stroke = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1;
    for (var index = 0; index < 12; index++) {
      final angle = (math.pi * 2 * index) / 12;
      final petalCenter = Offset(
        center.dx + math.cos(angle) * 9 * scale / 2,
        center.dy + math.sin(angle) * 9 * scale / 2,
      );
      canvas.drawOval(
        Rect.fromCenter(center: petalCenter, width: 4 * scale, height: 8 * scale),
        stroke,
      );
    }
    canvas.drawCircle(center, 4 * scale, fill);
    canvas.drawLine(Offset(center.dx, center.dy + 6 * scale), Offset(center.dx, 58), stroke);
  }

  void _drawKingProtea(Canvas canvas, Offset center, double scale) {
    final stroke = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1;
    final fill = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;
    for (var index = 0; index < 14; index++) {
      final angle = (math.pi * 2 * index) / 14;
      final outer = Offset(
        center.dx + math.cos(angle) * 9 * scale,
        center.dy + math.sin(angle) * 6 * scale,
      );
      final inner = Offset(
        center.dx + math.cos(angle) * 4 * scale,
        center.dy + math.sin(angle) * 3 * scale,
      );
      canvas.drawLine(inner, outer, stroke);
    }
    canvas.drawOval(Rect.fromCenter(center: center, width: 10 * scale, height: 8 * scale), stroke);
    canvas.drawCircle(center, 2 * scale, fill);
    canvas.drawLine(Offset(center.dx, center.dy + 7 * scale), Offset(center.dx, 58), stroke);
  }

  void _drawIconHeart(Canvas canvas, Offset center, double size) {
    final fill = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;
    canvas.drawCircle(Offset(center.dx - size, center.dy - (size / 3)), size, fill);
    canvas.drawCircle(Offset(center.dx + size, center.dy - (size / 3)), size, fill);
    final path = Path()
      ..moveTo(center.dx - size * 2, center.dy - (size / 3) + 1)
      ..lineTo(center.dx + size * 2, center.dy - (size / 3) + 1)
      ..lineTo(center.dx, center.dy + size * 2)
      ..close();
    canvas.drawPath(path, fill);
  }

  void _drawIconStar(Canvas canvas, Offset center, double radius) {
    final stroke = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1;
    for (var index = 0; index < 6; index++) {
      final angle = index * math.pi / 3;
      canvas.drawLine(
        center,
        Offset(center.dx + math.cos(angle) * radius, center.dy + math.sin(angle) * radius),
        stroke,
      );
    }
  }

  void _drawIconFlower(Canvas canvas, Offset center, double radius) {
    final fill = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;
    for (var index = 0; index < 5; index++) {
      final angle = (math.pi * 2 * index) / 5;
      canvas.drawCircle(
        Offset(center.dx + math.cos(angle) * radius, center.dy + math.sin(angle) * radius),
        radius,
        fill,
      );
    }
    canvas.drawCircle(center, radius + 1, fill);
  }

  void _drawIconNote(Canvas canvas, Offset center, double size) {
    final fill = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;
    final stroke = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.2;
    canvas.drawCircle(Offset(center.dx - (size / 2), center.dy + size - 1), size - 2, fill);
    canvas.drawLine(Offset(center.dx + size - 3, center.dy + size - 1), Offset(center.dx + size - 3, center.dy - size), stroke);
    canvas.drawLine(Offset(center.dx + size - 3, center.dy - size), Offset(center.dx + size, center.dy - size + 2), stroke);
  }

  void _drawIconMoon(Canvas canvas, Offset center, double radius) {
    final fill = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;
    final cut = Paint()
      ..color = Colors.black
      ..style = PaintingStyle.fill;
    canvas.drawCircle(center, radius, fill);
    canvas.drawCircle(Offset(center.dx + (radius / 2), center.dy - (radius / 3)), radius - 1, cut);
  }

  @override
  bool shouldRepaint(covariant _NoteCardPainter oldDelegate) {
    return text != oldDelegate.text ||
        fontSize != oldDelegate.fontSize ||
        border != oldDelegate.border ||
        flowerAccent != oldDelegate.flowerAccent ||
        icons.join(',') != oldDelegate.icons.join(',');
  }
}