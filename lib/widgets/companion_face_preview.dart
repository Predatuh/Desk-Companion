import 'dart:math' as math;

import 'package:flutter/material.dart';

class CompanionFacePreview extends StatelessWidget {
  const CompanionFacePreview({
    super.key,
    required this.personality,
    required this.petMode,
    required this.hair,
    required this.ears,
    required this.mustache,
    required this.glasses,
    required this.headwear,
    required this.piercing,
    required this.hairSize,
    required this.mustacheSize,
  });

  final String personality;
  final String petMode;
  final String hair;
  final String ears;
  final String mustache;
  final String glasses;
  final String headwear;
  final String piercing;
  final int hairSize;
  final int mustacheSize;

  @override
  Widget build(BuildContext context) {
    return AspectRatio(
      aspectRatio: 2,
      child: CustomPaint(
        painter: _CompanionFacePainter(
          personality: personality,
          petMode: petMode,
          hair: hair,
          ears: ears,
          mustache: mustache,
          glasses: glasses,
          headwear: headwear,
          piercing: piercing,
          hairSize: hairSize,
          mustacheSize: mustacheSize,
        ),
      ),
    );
  }
}

class _CompanionFacePainter extends CustomPainter {
  const _CompanionFacePainter({
    required this.personality,
    required this.petMode,
    required this.hair,
    required this.ears,
    required this.mustache,
    required this.glasses,
    required this.headwear,
    required this.piercing,
    required this.hairSize,
    required this.mustacheSize,
  });

  final String personality;
  final String petMode;
  final String hair;
  final String ears;
  final String mustache;
  final String glasses;
  final String headwear;
  final String piercing;
  final int hairSize;
  final int mustacheSize;

  @override
  void paint(Canvas canvas, Size size) {
    canvas.save();
    canvas.scale(size.width / 128, size.height / 64);

    final stroke = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.2;
    final fill = Paint()
      ..color = Colors.white
      ..style = PaintingStyle.fill;
    final cut = Paint()
      ..color = Colors.black
      ..style = PaintingStyle.fill;

    canvas.drawRRect(
      RRect.fromRectAndRadius(
        const Rect.fromLTWH(0.5, 0.5, 127, 63),
        const Radius.circular(10),
      ),
      stroke,
    );

    const leftX = 38.0;
    const rightX = 90.0;
    const eyeY = 31.0;
    const mouthY = 47.0;

    final isOff = petMode == 'off';
    final isSleepy = petMode == 'nap' || personality == 'sleepy';
    final isCuddly = petMode == 'cuddle' || personality == 'cuddly';
    final isPlayful = petMode == 'play' || personality == 'playful';
    final isParty = petMode == 'party';

    if (isOff) {
      _drawEye(canvas, fill, cut, leftX, eyeY, 22, 14, 5, 0, 0);
      _drawEye(canvas, fill, cut, rightX, eyeY, 22, 14, 5, 0, 0);
      canvas.drawLine(const Offset(57, mouthY), const Offset(71, mouthY), stroke);
    } else if (isSleepy) {
      _drawBlinkEye(canvas, fill, leftX, eyeY, 24, 4, 4);
      _drawBlinkEye(canvas, fill, rightX, eyeY, 24, 4, 4);
      canvas.drawLine(const Offset(56, mouthY), const Offset(72, mouthY), stroke);
      _drawZ(canvas, stroke, const Offset(100, 23), 1.0);
      _drawZ(canvas, stroke, const Offset(108, 18), 0.8);
    } else if (isCuddly) {
      _drawEye(canvas, fill, cut, leftX, eyeY, 24, 16, 5, 0, 0);
      _drawEye(canvas, fill, cut, rightX, eyeY, 24, 16, 5, 0, 0);
      _drawSmile(canvas, stroke, 64, mouthY - 1, 24);
      _drawHeart(canvas, fill, const Offset(64, 55), 3);
    } else if (isPlayful) {
      _drawEye(canvas, fill, cut, leftX, eyeY, 26, 18, 5, 3, 0);
      _drawEye(canvas, fill, cut, rightX, eyeY, 26, 18, 5, 3, 0);
      _drawSmile(canvas, stroke, 64, mouthY - 2, 26);
      canvas.drawCircle(const Offset(18, 49), 2, fill);
    } else if (isParty) {
      _drawEye(canvas, fill, cut, leftX, eyeY, 26, 18, 5, 2, 0);
      _drawEye(canvas, fill, cut, rightX, eyeY, 26, 18, 5, -2, 0);
      _drawSmile(canvas, stroke, 64, mouthY - 2, 28);
      _drawStar(canvas, stroke, const Offset(16, 15), 3);
      _drawStar(canvas, stroke, const Offset(112, 17), 3);
    } else {
      _drawEye(canvas, fill, cut, leftX, eyeY, 24, 16, 5, 2, 1);
      _drawEye(canvas, fill, cut, rightX, eyeY, 24, 16, 5, -1, -1);
      _drawOvalMouth(canvas, stroke, 64, mouthY, 5, 4);
    }

    _drawAccessories(canvas, stroke, fill, leftX, rightX, eyeY, mouthY);
    canvas.restore();
  }

  int _clampPercent(int value) => value.clamp(70, 170);

  double _scaleValue(num base, int percent) => base * _clampPercent(percent) / 100.0;

  void _drawEye(
    Canvas canvas,
    Paint fill,
    Paint cut,
    double cx,
    double cy,
    double w,
    double h,
    double r,
    double pupilDx,
    double pupilDy,
  ) {
    final clampedH = h < 3 ? 3.0 : h;
    canvas.drawRRect(
      RRect.fromRectAndRadius(
        Rect.fromCenter(center: Offset(cx, cy), width: w, height: clampedH),
        Radius.circular(r),
      ),
      fill,
    );
    if (clampedH >= 10) {
      canvas.drawCircle(Offset(cx + pupilDx, cy + pupilDy), 5, cut);
    } else if (clampedH >= 6) {
      canvas.drawCircle(Offset(cx + pupilDx, cy + pupilDy), 3, cut);
    }
  }

  void _drawBlinkEye(Canvas canvas, Paint fill, double cx, double cy, double w, double h, double r) {
    canvas.drawRRect(
      RRect.fromRectAndRadius(
        Rect.fromCenter(center: Offset(cx, cy), width: w, height: h < 3 ? 3 : h),
        Radius.circular(r),
      ),
      fill,
    );
  }

  void _drawSmile(Canvas canvas, Paint stroke, double cx, double cy, double w) {
    final hw = w / 2;
    for (var t = 0; t < 3; t++) {
      canvas.drawLine(Offset(cx - hw, cy + t), Offset(cx - hw / 3, cy + 7 + t), stroke);
      canvas.drawLine(Offset(cx - hw / 3, cy + 7 + t), Offset(cx + hw / 3, cy + 7 + t), stroke);
      canvas.drawLine(Offset(cx + hw / 3, cy + 7 + t), Offset(cx + hw, cy + t), stroke);
    }
  }

  void _drawOvalMouth(Canvas canvas, Paint stroke, double cx, double cy, double rw, double rh) {
    for (var t = 0; t < 2; t++) {
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          Rect.fromCenter(center: Offset(cx, cy + t), width: rw * 2, height: rh * 2),
          Radius.circular(rh),
        ),
        stroke,
      );
    }
  }

  void _drawHeart(Canvas canvas, Paint fill, Offset center, double s) {
    canvas.drawCircle(Offset(center.dx - s, center.dy - s / 3), s, fill);
    canvas.drawCircle(Offset(center.dx + s, center.dy - s / 3), s, fill);
    final path = Path()
      ..moveTo(center.dx - s * 2, center.dy - s / 3 + 1)
      ..lineTo(center.dx + s * 2, center.dy - s / 3 + 1)
      ..lineTo(center.dx, center.dy + s * 2)
      ..close();
    canvas.drawPath(path, fill);
  }

  void _drawStar(Canvas canvas, Paint stroke, Offset center, double radius) {
    for (var i = 0; i < 6; i++) {
      final angle = i * math.pi / 3;
      final point = Offset(
        center.dx + math.cos(angle) * radius,
        center.dy + math.sin(angle) * radius,
      );
      canvas.drawLine(center, point, stroke);
    }
  }

  void _drawZ(Canvas canvas, Paint stroke, Offset origin, double scale) {
    final width = 5 * scale;
    final height = 5 * scale;
    canvas.drawLine(origin, Offset(origin.dx + width, origin.dy), stroke);
    canvas.drawLine(Offset(origin.dx + width, origin.dy), Offset(origin.dx, origin.dy + height), stroke);
    canvas.drawLine(Offset(origin.dx, origin.dy + height), Offset(origin.dx + width, origin.dy + height), stroke);
  }

  void _drawAccessories(Canvas canvas, Paint stroke, Paint fill, double leftX, double rightX, double eyeY, double mouthY) {
    final faceCenterX = (leftX + rightX) / 2;
    final scaledHair = _clampPercent(hairSize);
    final scaledMustache = _clampPercent(mustacheSize);

    if (ears == 'cat') {
      _triangle(canvas, stroke, Offset(leftX - 16, eyeY - 18), Offset(leftX - 8, eyeY - 30), Offset(leftX + 2, eyeY - 18));
      _triangle(canvas, stroke, Offset(rightX - 2, eyeY - 18), Offset(rightX + 8, eyeY - 30), Offset(rightX + 16, eyeY - 18));
    } else if (ears == 'bear') {
      canvas.drawCircle(Offset(leftX - 12, eyeY - 20), 6, stroke);
      canvas.drawCircle(Offset(rightX + 12, eyeY - 20), 6, stroke);
    } else if (ears == 'bunny') {
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(leftX - 15, eyeY - 34, 8, 16), const Radius.circular(4)),
        stroke,
      );
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(rightX + 7, eyeY - 34, 8, 16), const Radius.circular(4)),
        stroke,
      );
    }

    if (hair == 'tuft') {
      final lift = _scaleValue(12, scaledHair);
      final spread = _scaleValue(5, scaledHair);
      canvas.drawLine(Offset(faceCenterX - spread, eyeY - 12), Offset(faceCenterX, eyeY - 12 - lift), stroke);
      canvas.drawLine(Offset(faceCenterX, eyeY - 12 - lift), Offset(faceCenterX + spread, eyeY - 12), stroke);
      canvas.drawLine(Offset(faceCenterX, eyeY - 12 - lift), Offset(faceCenterX + 1, eyeY - 8), stroke);
    } else if (hair == 'bangs') {
      final topY = eyeY - 12 - _scaleValue(3, scaledHair);
      canvas.drawLine(Offset(leftX - 18, topY), Offset(rightX + 18, topY), stroke);
      for (double x = leftX - 14; x <= rightX + 14; x += 8) {
        canvas.drawLine(Offset(x, topY + 1), Offset(x + _scaleValue(3, scaledHair), eyeY - 7), stroke);
      }
    } else if (hair == 'spiky') {
      for (double x = leftX - 14; x <= rightX + 14; x += 10) {
        final peak = eyeY - 10 - _scaleValue(9, scaledHair);
        canvas.drawLine(Offset(x, eyeY - 10), Offset(x + 4, peak), stroke);
        canvas.drawLine(Offset(x + 4, peak), Offset(x + 8, eyeY - 10), stroke);
      }
    } else if (hair == 'swoop') {
      final topY = eyeY - 12 - _scaleValue(6, scaledHair);
      canvas.drawLine(Offset(leftX - 10, eyeY - 9), Offset(faceCenterX + 12, topY), stroke);
      canvas.drawLine(Offset(faceCenterX + 12, topY), Offset(rightX + 18, eyeY - 5), stroke);
      canvas.drawLine(Offset(leftX - 6, eyeY - 10), Offset(faceCenterX + 4, topY + 2), stroke);
    } else if (hair == 'bob') {
      final topY = eyeY - 11 - _scaleValue(4, scaledHair);
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          Rect.fromLTWH(leftX - 18, topY, (rightX - leftX) + 36, 10 + _scaleValue(4, scaledHair)),
          const Radius.circular(5),
        ),
        stroke,
      );
      canvas.drawLine(Offset(leftX - 18, eyeY - 1), Offset(leftX - 12, eyeY + 7), stroke);
      canvas.drawLine(Offset(rightX + 18, eyeY - 1), Offset(rightX + 12, eyeY + 7), stroke);
    } else if (hair == 'messy') {
      for (double x = leftX - 10; x <= rightX + 12; x += 9) {
        final peak = eyeY - 8 - _scaleValue(7, scaledHair) + (((x ~/ 9) % 2 == 0) ? 0 : 3);
        canvas.drawLine(Offset(x, eyeY - 8), Offset(x + 3, peak), stroke);
        canvas.drawLine(Offset(x + 3, peak), Offset(x + 6, eyeY - 9), stroke);
      }
    }

    if (headwear == 'bow') {
      _triangle(canvas, stroke, Offset(faceCenterX - 4, eyeY - 24), Offset(faceCenterX - 16, eyeY - 18), Offset(faceCenterX - 8, eyeY - 12));
      _triangle(canvas, stroke, Offset(faceCenterX + 4, eyeY - 24), Offset(faceCenterX + 16, eyeY - 18), Offset(faceCenterX + 8, eyeY - 12));
      canvas.drawCircle(Offset(faceCenterX, eyeY - 18), 2, stroke);
    } else if (headwear == 'beanie') {
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(faceCenterX - 24, eyeY - 28, 48, 12), const Radius.circular(5)),
        stroke,
      );
      canvas.drawLine(Offset(faceCenterX - 20, eyeY - 15), Offset(faceCenterX + 20, eyeY - 15), stroke);
      canvas.drawCircle(Offset(faceCenterX, eyeY - 30), 3, stroke);
    } else if (headwear == 'crown') {
      final crown = Path()
        ..moveTo(faceCenterX - 20, eyeY - 18)
        ..lineTo(faceCenterX - 12, eyeY - 30)
        ..lineTo(faceCenterX - 2, eyeY - 18)
        ..lineTo(faceCenterX + 6, eyeY - 32)
        ..lineTo(faceCenterX + 14, eyeY - 18)
        ..lineTo(faceCenterX + 20, eyeY - 28);
      canvas.drawPath(crown, stroke);
      canvas.drawLine(Offset(faceCenterX - 20, eyeY - 18), Offset(faceCenterX + 20, eyeY - 18), stroke);
    }

    if (glasses == 'round') {
      canvas.drawCircle(Offset(leftX, eyeY), 12, stroke);
      canvas.drawCircle(Offset(rightX, eyeY), 12, stroke);
      canvas.drawLine(Offset(leftX + 12, eyeY), Offset(rightX - 12, eyeY), stroke);
    } else if (glasses == 'square') {
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(leftX - 14, eyeY - 11, 28, 22), const Radius.circular(4)),
        stroke,
      );
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(rightX - 14, eyeY - 11, 28, 22), const Radius.circular(4)),
        stroke,
      );
      canvas.drawLine(Offset(leftX + 14, eyeY), Offset(rightX - 14, eyeY), stroke);
    } else if (glasses == 'visor') {
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(leftX - 18, eyeY - 12, (rightX - leftX) + 36, 20), const Radius.circular(6)),
        stroke,
      );
    }

    if (mustache == 'classic') {
      final wing = _scaleValue(12, scaledMustache);
      final inner = _scaleValue(4, scaledMustache);
      canvas.drawLine(Offset(faceCenterX - wing, mouthY - 4), Offset(faceCenterX - inner, mouthY - 1), stroke);
      canvas.drawLine(Offset(faceCenterX - wing, mouthY - 3), Offset(faceCenterX - inner, mouthY + 1), stroke);
      canvas.drawLine(Offset(faceCenterX + inner, mouthY - 1), Offset(faceCenterX + wing, mouthY - 4), stroke);
      canvas.drawLine(Offset(faceCenterX + inner, mouthY + 1), Offset(faceCenterX + wing, mouthY - 3), stroke);
    } else if (mustache == 'curled') {
      final wing = _scaleValue(12, scaledMustache);
      canvas.drawLine(Offset(faceCenterX - wing, mouthY - 2), Offset(faceCenterX - 2, mouthY - 1), stroke);
      canvas.drawLine(Offset(faceCenterX + 2, mouthY - 1), Offset(faceCenterX + wing, mouthY - 2), stroke);
      canvas.drawCircle(Offset(faceCenterX - wing - 2, mouthY - 3), 2, stroke);
      canvas.drawCircle(Offset(faceCenterX + wing + 2, mouthY - 3), 2, stroke);
    } else if (mustache == 'handlebar') {
      final wing = _scaleValue(14, scaledMustache);
      canvas.drawLine(Offset(faceCenterX - wing, mouthY - 1), Offset(faceCenterX - 2, mouthY), stroke);
      canvas.drawLine(Offset(faceCenterX + 2, mouthY), Offset(faceCenterX + wing, mouthY - 1), stroke);
      canvas.drawLine(Offset(faceCenterX - wing, mouthY - 1), Offset(faceCenterX - wing - 4, mouthY - 5), stroke);
      canvas.drawLine(Offset(faceCenterX + wing, mouthY - 1), Offset(faceCenterX + wing + 4, mouthY - 5), stroke);
    } else if (mustache == 'walrus') {
      final width = _scaleValue(14, scaledMustache);
      final height = _scaleValue(4, scaledMustache);
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(faceCenterX - width, mouthY - 6, width * 2, height + 2), const Radius.circular(3)),
        fill,
      );
      canvas.drawRect(Rect.fromLTWH(faceCenterX - 2, mouthY - 3, 4, height + 4), fill);
    } else if (mustache == 'pencil') {
      final width = _scaleValue(13, scaledMustache);
      canvas.drawLine(Offset(faceCenterX - width, mouthY - 2), Offset(faceCenterX + width, mouthY - 2), stroke);
      canvas.drawLine(Offset(faceCenterX - width + 2, mouthY - 1), Offset(faceCenterX + width - 2, mouthY - 1), stroke);
    } else if (mustache == 'imperial') {
      final wing = _scaleValue(12, scaledMustache);
      canvas.drawLine(Offset(faceCenterX - wing, mouthY - 2), Offset(faceCenterX - 1, mouthY - 1), stroke);
      canvas.drawLine(Offset(faceCenterX + 1, mouthY - 1), Offset(faceCenterX + wing, mouthY - 2), stroke);
      canvas.drawLine(Offset(faceCenterX - wing, mouthY - 2), Offset(faceCenterX - wing - 2, mouthY - 9), stroke);
      canvas.drawLine(Offset(faceCenterX + wing, mouthY - 2), Offset(faceCenterX + wing + 2, mouthY - 9), stroke);
    }

    if (piercing == 'brow') {
      canvas.drawLine(Offset(rightX + 6, eyeY - 14), Offset(rightX + 14, eyeY - 12), stroke);
    } else if (piercing == 'nose') {
      canvas.drawCircle(Offset(faceCenterX + 4, mouthY - 10), 2, stroke);
    } else if (piercing == 'lip') {
      canvas.drawCircle(Offset(faceCenterX + 8, mouthY + 2), 2, stroke);
    }
  }

  void _triangle(Canvas canvas, Paint stroke, Offset a, Offset b, Offset c) {
    final path = Path()
      ..moveTo(a.dx, a.dy)
      ..lineTo(b.dx, b.dy)
      ..lineTo(c.dx, c.dy)
      ..close();
    canvas.drawPath(path, stroke);
  }

  @override
  bool shouldRepaint(covariant _CompanionFacePainter oldDelegate) {
    return personality != oldDelegate.personality ||
        petMode != oldDelegate.petMode ||
        hair != oldDelegate.hair ||
        ears != oldDelegate.ears ||
        mustache != oldDelegate.mustache ||
        glasses != oldDelegate.glasses ||
        headwear != oldDelegate.headwear ||
        piercing != oldDelegate.piercing ||
        hairSize != oldDelegate.hairSize ||
        mustacheSize != oldDelegate.mustacheSize;
  }
}