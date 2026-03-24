import 'dart:typed_data';

import 'package:flutter/material.dart';

enum NoteBorderStyle {
  classic,
  stitched,
  squiggle,
  scallop,
  flowers,
  hearts,
}

enum NoteSticker {
  heart,
  flower,
  star,
  sparkle,
  music,
  moon,
  mail,
  coffee,
  dog,
}

extension NoteBorderStyleLabel on NoteBorderStyle {
  String get label => switch (this) {
        NoteBorderStyle.classic => 'Classic',
        NoteBorderStyle.stitched => 'Stitched',
        NoteBorderStyle.squiggle => 'Squiggle',
        NoteBorderStyle.scallop => 'Scallop',
        NoteBorderStyle.flowers => 'Rose',
        NoteBorderStyle.hearts => 'Hearts',
      };
}

extension NoteStickerLabel on NoteSticker {
  String get label => switch (this) {
        NoteSticker.heart => 'Heart',
    NoteSticker.flower => 'Rose',
        NoteSticker.star => 'Star',
        NoteSticker.sparkle => 'Sparkle',
        NoteSticker.music => 'Music',
        NoteSticker.moon => 'Moon',
        NoteSticker.mail => 'Mail',
        NoteSticker.coffee => 'Coffee',
        NoteSticker.dog => 'Dog',
      };
}

class NoteCardPreview extends StatelessWidget {
  const NoteCardPreview({
    super.key,
    required this.text,
    required this.fontSize,
    required this.borderStyle,
    required this.stickers,
    this.customFrameBytes,
  });

  final String text;
  final int fontSize;
  final NoteBorderStyle borderStyle;
  final Set<NoteSticker> stickers;
  final Uint8List? customFrameBytes;

  @override
  Widget build(BuildContext context) {
    final textStyle = TextStyle(
      color: Colors.white,
      fontFamily: 'monospace',
      fontSize: switch (fontSize) {
        3 => 12,
        2 => 10,
        _ => 8,
      },
      fontWeight: FontWeight.w600,
      height: 1.15,
    );

    final stickerList = stickers.toList(growable: false);
    final topStickers = stickerList.take(4).toList(growable: false);
    final bottomStickers = stickerList.skip(4).take(4).toList(growable: false);

    return SizedBox(
      width: 128,
      height: 64,
      child: DecoratedBox(
        decoration: const BoxDecoration(color: Colors.black),
        child: Stack(
          fit: StackFit.expand,
          children: [
            CustomPaint(painter: _NoteBorderPainter(borderStyle)),
            if (customFrameBytes != null)
              Positioned.fill(
                child: IgnorePointer(
                  child: ColorFiltered(
                    colorFilter: const ColorFilter.mode(Colors.white, BlendMode.srcIn),
                    child: Image.memory(
                      customFrameBytes!,
                      fit: BoxFit.cover,
                      filterQuality: FilterQuality.none,
                    ),
                  ),
                ),
              ),
            Padding(
              padding: const EdgeInsets.fromLTRB(10, 8, 10, 8),
              child: Column(
                children: [
                  if (topStickers.isNotEmpty) _StickerRow(stickers: topStickers),
                  Expanded(
                    child: Center(
                      child: Text(
                        text.trim().isEmpty ? 'Your note here' : text.trim(),
                        textAlign: TextAlign.center,
                        style: textStyle,
                        maxLines: switch (fontSize) {
                          3 => 3,
                          2 => 4,
                          _ => 5,
                        },
                        overflow: TextOverflow.clip,
                      ),
                    ),
                  ),
                  if (bottomStickers.isNotEmpty) _StickerRow(stickers: bottomStickers),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _StickerRow extends StatelessWidget {
  const _StickerRow({required this.stickers});

  final List<NoteSticker> stickers;

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        for (var i = 0; i < stickers.length; i++) ...[
          _StickerGlyph(sticker: stickers[i]),
          if (i != stickers.length - 1) const SizedBox(width: 4),
        ],
      ],
    );
  }
}

class _StickerGlyph extends StatelessWidget {
  const _StickerGlyph({required this.sticker});

  final NoteSticker sticker;

  @override
  Widget build(BuildContext context) {
    if (sticker == NoteSticker.dog) {
      return const SizedBox(
        width: 12,
        height: 12,
        child: CustomPaint(painter: _AussieDogFacePainter()),
      );
    }

    if (sticker == NoteSticker.flower) {
      return const SizedBox(
        width: 12,
        height: 12,
        child: CustomPaint(painter: _RoseStickerPainter()),
      );
    }

    final icon = switch (sticker) {
      NoteSticker.heart => Icons.favorite,
      NoteSticker.flower => Icons.local_florist,
      NoteSticker.star => Icons.star,
      NoteSticker.sparkle => Icons.auto_awesome,
      NoteSticker.music => Icons.music_note,
      NoteSticker.moon => Icons.dark_mode,
      NoteSticker.mail => Icons.mail,
      NoteSticker.coffee => Icons.coffee,
      NoteSticker.dog => Icons.pets,
    };

    return Icon(icon, size: 12, color: Colors.white);
  }
}

class _NoteBorderPainter extends CustomPainter {
  const _NoteBorderPainter(this.style);

  final NoteBorderStyle style;

  @override
  void paint(Canvas canvas, Size size) {
    final stroke = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.4;
    final fill = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;
    final rect = Rect.fromLTWH(1, 1, size.width - 2, size.height - 2);
    final rounded = RRect.fromRectAndRadius(rect, const Radius.circular(8));

    canvas.drawRRect(rounded, stroke);

    switch (style) {
      case NoteBorderStyle.classic:
        break;
      case NoteBorderStyle.stitched:
        for (double x = 8; x < size.width - 8; x += 6) {
          canvas.drawLine(Offset(x, 4), Offset(x + 2, 4), stroke);
          canvas.drawLine(Offset(x, size.height - 4), Offset(x + 2, size.height - 4), stroke);
        }
        for (double y = 8; y < size.height - 8; y += 6) {
          canvas.drawLine(Offset(4, y), Offset(4, y + 2), stroke);
          canvas.drawLine(Offset(size.width - 4, y), Offset(size.width - 4, y + 2), stroke);
        }
        break;
      case NoteBorderStyle.squiggle:
        _drawSquiggle(canvas, size, stroke);
        break;
      case NoteBorderStyle.scallop:
        _drawScallops(canvas, size, stroke);
        break;
      case NoteBorderStyle.flowers:
        _drawRoses(canvas, size, stroke, fill);
        break;
      case NoteBorderStyle.hearts:
        _drawHearts(canvas, size, fill);
        break;
    }
  }

  void _drawSquiggle(Canvas canvas, Size size, Paint paint) {
    final path = Path();
    path.moveTo(8, 4);
    for (double x = 8; x <= size.width - 8; x += 8) {
      path.quadraticBezierTo(x + 2, 2, x + 4, 4);
      path.quadraticBezierTo(x + 6, 6, x + 8, 4);
    }
    canvas.drawPath(path, paint);

    final bottom = Path();
    bottom.moveTo(8, size.height - 4);
    for (double x = 8; x <= size.width - 8; x += 8) {
      bottom.quadraticBezierTo(x + 2, size.height - 6, x + 4, size.height - 4);
      bottom.quadraticBezierTo(x + 6, size.height - 2, x + 8, size.height - 4);
    }
    canvas.drawPath(bottom, paint);
  }

  void _drawScallops(Canvas canvas, Size size, Paint paint) {
    for (double x = 8; x < size.width - 8; x += 8) {
      canvas.drawArc(Rect.fromCircle(center: Offset(x + 4, 4), radius: 4), 3.14, 3.14, false, paint);
      canvas.drawArc(Rect.fromCircle(center: Offset(x + 4, size.height - 4), radius: 4), 0, 3.14, false, paint);
    }
  }

  void _drawRoses(Canvas canvas, Size size, Paint stroke, Paint fill) {
    for (final center in [
      const Offset(10, 10),
      Offset(size.width - 10, 10),
      Offset(10, size.height - 10),
      Offset(size.width - 10, size.height - 10),
    ]) {
      final rose = Path()
        ..moveTo(center.dx - 2.8, center.dy + 1.6)
        ..quadraticBezierTo(center.dx - 4.4, center.dy - 0.8, center.dx - 1.6, center.dy - 2.8)
        ..quadraticBezierTo(center.dx, center.dy - 4.2, center.dx + 1.9, center.dy - 2.4)
        ..quadraticBezierTo(center.dx + 4.5, center.dy - 0.6, center.dx + 2.2, center.dy + 2.2)
        ..quadraticBezierTo(center.dx, center.dy + 4.2, center.dx - 2.8, center.dy + 1.6);
      canvas.drawPath(rose, stroke);
      canvas.drawArc(
        Rect.fromCenter(center: center, width: 4.6, height: 4.6),
        0.2,
        4.4,
        false,
        stroke,
      );
      canvas.drawLine(
        Offset(center.dx + 3.2, center.dy + 2.4),
        Offset(center.dx + 5.5, center.dy + 4.8),
        stroke,
      );
      canvas.drawCircle(Offset(center.dx + 4.6, center.dy + 5.1), 0.5, fill);
    }
  }

  void _drawHearts(Canvas canvas, Size size, Paint fill) {
    for (final center in [
      const Offset(11, 9),
      Offset(size.width - 11, 9),
      Offset(11, size.height - 9),
      Offset(size.width - 11, size.height - 9),
    ]) {
      final path = Path()
        ..moveTo(center.dx, center.dy + 3)
        ..cubicTo(center.dx - 5, center.dy - 1, center.dx - 3, center.dy - 5, center.dx, center.dy - 2)
        ..cubicTo(center.dx + 3, center.dy - 5, center.dx + 5, center.dy - 1, center.dx, center.dy + 3);
      canvas.drawPath(path, fill);
    }
  }

  @override
  bool shouldRepaint(covariant _NoteBorderPainter oldDelegate) => oldDelegate.style != style;
}

class _AussieDogFacePainter extends CustomPainter {
  const _AussieDogFacePainter();

  @override
  void paint(Canvas canvas, Size size) {
    final stroke = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.1
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round;
    final fill = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;

    final face = Path()
      ..moveTo(size.width * 0.18, size.height * 0.38)
      ..lineTo(size.width * 0.28, size.height * 0.12)
      ..lineTo(size.width * 0.42, size.height * 0.28)
      ..quadraticBezierTo(size.width * 0.50, size.height * 0.18, size.width * 0.58, size.height * 0.28)
      ..lineTo(size.width * 0.72, size.height * 0.12)
      ..lineTo(size.width * 0.82, size.height * 0.38)
      ..quadraticBezierTo(size.width * 0.77, size.height * 0.78, size.width * 0.50, size.height * 0.88)
      ..quadraticBezierTo(size.width * 0.23, size.height * 0.78, size.width * 0.18, size.height * 0.38);
    canvas.drawPath(face, stroke);

    canvas.drawLine(
      Offset(size.width * 0.50, size.height * 0.28),
      Offset(size.width * 0.50, size.height * 0.66),
      stroke,
    );
    canvas.drawCircle(Offset(size.width * 0.37, size.height * 0.46), 0.9, fill);
    canvas.drawCircle(Offset(size.width * 0.63, size.height * 0.46), 0.9, fill);
    canvas.drawCircle(Offset(size.width * 0.50, size.height * 0.66), 1.2, fill);
    canvas.drawArc(
      Rect.fromCenter(center: Offset(size.width * 0.44, size.height * 0.73), width: 0.12 * size.width, height: 0.10 * size.height),
      0.2,
      2.2,
      false,
      stroke,
    );
    canvas.drawArc(
      Rect.fromCenter(center: Offset(size.width * 0.56, size.height * 0.73), width: 0.12 * size.width, height: 0.10 * size.height),
      0.7,
      2.2,
      false,
      stroke,
    );
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class _RoseStickerPainter extends CustomPainter {
  const _RoseStickerPainter();

  @override
  void paint(Canvas canvas, Size size) {
    final stroke = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.0
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round;

    final bloomCenter = Offset(size.width * 0.38, size.height * 0.38);
    final bloom = Path()
      ..moveTo(bloomCenter.dx - 2.8, bloomCenter.dy + 1.6)
      ..quadraticBezierTo(bloomCenter.dx - 4.4, bloomCenter.dy - 0.8, bloomCenter.dx - 1.6, bloomCenter.dy - 2.8)
      ..quadraticBezierTo(bloomCenter.dx, bloomCenter.dy - 4.2, bloomCenter.dx + 1.9, bloomCenter.dy - 2.4)
      ..quadraticBezierTo(bloomCenter.dx + 4.5, bloomCenter.dy - 0.6, bloomCenter.dx + 2.2, bloomCenter.dy + 2.2)
      ..quadraticBezierTo(bloomCenter.dx, bloomCenter.dy + 4.2, bloomCenter.dx - 2.8, bloomCenter.dy + 1.6);
    canvas.drawPath(bloom, stroke);
    canvas.drawArc(
      Rect.fromCenter(center: bloomCenter, width: 4.4, height: 4.4),
      0.2,
      4.6,
      false,
      stroke,
    );

    canvas.drawLine(
      Offset(size.width * 0.58, size.height * 0.52),
      Offset(size.width * 0.85, size.height * 0.82),
      stroke,
    );
    final leaf = Path()
      ..moveTo(size.width * 0.64, size.height * 0.62)
      ..quadraticBezierTo(size.width * 0.76, size.height * 0.56, size.width * 0.76, size.height * 0.70)
      ..quadraticBezierTo(size.width * 0.69, size.height * 0.72, size.width * 0.64, size.height * 0.62);
    canvas.drawPath(leaf, stroke);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}