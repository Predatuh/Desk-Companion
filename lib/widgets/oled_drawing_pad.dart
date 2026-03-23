import 'dart:typed_data';
import 'dart:math' as math;

import 'package:flutter/material.dart';

typedef OledPixelCallback = void Function(int x, int y);

class OledDrawingPad extends StatefulWidget {
  const OledDrawingPad({
    super.key,
    required this.bitmap,
    required this.onPixel,
    this.showGrid = true,
    this.enabled = true,
  });

  final Uint8List bitmap;
  final OledPixelCallback onPixel;
  final bool showGrid;
  final bool enabled;

  @override
  State<OledDrawingPad> createState() => _OledDrawingPadState();
}

class _OledDrawingPadState extends State<OledDrawingPad> {
  Offset? _lastGridPoint;

  Offset _toGridPoint(Offset localPosition, Size size) {
    return Offset(
      ((localPosition.dx / size.width) * 128).floor().clamp(0, 127).toDouble(),
      ((localPosition.dy / size.height) * 64).floor().clamp(0, 63).toDouble(),
    );
  }

  void _emitGridPoint(Offset point) {
    widget.onPixel(point.dx.toInt(), point.dy.toInt());
  }

  void _forward(Offset localPosition, Size size) {
    final current = _toGridPoint(localPosition, size);
    final previous = _lastGridPoint;
    if (previous == null) {
      _emitGridPoint(current);
      _lastGridPoint = current;
      return;
    }

    final deltaX = current.dx - previous.dx;
    final deltaY = current.dy - previous.dy;
    final steps = math.max(deltaX.abs(), deltaY.abs()).round();

    if (steps == 0) {
      _emitGridPoint(current);
      _lastGridPoint = current;
      return;
    }

    for (var step = 1; step <= steps; step++) {
      final progress = step / steps;
      final point = Offset(
        (previous.dx + (deltaX * progress)).roundToDouble(),
        (previous.dy + (deltaY * progress)).roundToDouble(),
      );
      _emitGridPoint(point);
    }

    _lastGridPoint = current;
  }

  void _resetStroke() {
    _lastGridPoint = null;
  }

  @override
  Widget build(BuildContext context) {
    return AspectRatio(
      aspectRatio: 2,
      child: LayoutBuilder(
        builder: (context, constraints) {
          final size = Size(constraints.maxWidth, constraints.maxHeight);

          return Stack(
            fit: StackFit.expand,
            children: [
              SizedBox.expand(
                child: GestureDetector(
                  behavior: HitTestBehavior.opaque,
                  onTapDown: widget.enabled
                      ? (details) {
                          _resetStroke();
                          _forward(details.localPosition, size);
                          _resetStroke();
                        }
                      : null,
                  onPanStart: widget.enabled
                      ? (details) {
                          _resetStroke();
                          _forward(details.localPosition, size);
                        }
                      : null,
                  onPanUpdate: widget.enabled
                      ? (details) => _forward(details.localPosition, size)
                      : null,
                  onPanEnd: widget.enabled ? (_) => _resetStroke() : null,
                  onPanCancel: widget.enabled ? _resetStroke : null,
                  child: CustomPaint(
                    painter: _OledDrawingPadPainter(
                      bitmap: widget.bitmap,
                      showGrid: widget.showGrid,
                    ),
                  ),
                ),
              ),
              if (!widget.enabled)
                Positioned.fill(
                  child: IgnorePointer(
                    child: DecoratedBox(
                      decoration: BoxDecoration(
                        color: Colors.black.withValues(alpha: 0.24),
                        borderRadius: BorderRadius.circular(18),
                      ),
                      child: const Center(
                        child: Text(
                          'Turn on Draw mode to sketch',
                          style: TextStyle(
                            color: Colors.white,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                      ),
                    ),
                  ),
                ),
            ],
          );
        },
      ),
    );
  }
}

class _OledDrawingPadPainter extends CustomPainter {
  const _OledDrawingPadPainter({
    required this.bitmap,
    required this.showGrid,
  });

  final Uint8List bitmap;
  final bool showGrid;

  @override
  void paint(Canvas canvas, Size size) {
    final background = Paint()..color = const Color(0xFF161210);
    canvas.drawRRect(
      RRect.fromRectAndRadius(Offset.zero & size, const Radius.circular(18)),
      background,
    );

    final pixelWidth = size.width / 128;
    final pixelHeight = size.height / 64;
    final pixelPaint = Paint()..color = Colors.white;

    for (var y = 0; y < 64; y++) {
      for (var x = 0; x < 128; x++) {
        final byteIndex = y * 16 + (x >> 3);
        final bitMask = 1 << (7 - (x & 7));
        if ((bitmap[byteIndex] & bitMask) == 0) {
          continue;
        }

        canvas.drawRect(
          Rect.fromLTWH(
            x * pixelWidth,
            y * pixelHeight,
            pixelWidth + 0.1,
            pixelHeight + 0.1,
          ),
          pixelPaint,
        );
      }
    }

    if (showGrid) {
      final gridPaint = Paint()
        ..color = Colors.white.withValues(alpha: 0.08)
        ..strokeWidth = 1;
      for (var x = 0; x <= 128; x += 8) {
        final dx = x * pixelWidth;
        canvas.drawLine(Offset(dx, 0), Offset(dx, size.height), gridPaint);
      }
      for (var y = 0; y <= 64; y += 8) {
        final dy = y * pixelHeight;
        canvas.drawLine(Offset(0, dy), Offset(size.width, dy), gridPaint);
      }
    }

    final borderPaint = Paint()
      ..color = Colors.white.withValues(alpha: 0.18)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.5;
    canvas.drawRRect(
      RRect.fromRectAndRadius(Offset.zero & size, const Radius.circular(18)),
      borderPaint,
    );
  }

  @override
  bool shouldRepaint(covariant _OledDrawingPadPainter oldDelegate) {
    return oldDelegate.bitmap != bitmap || oldDelegate.showGrid != showGrid;
  }
}
