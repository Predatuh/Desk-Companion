import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/painting.dart';
import 'package:image/image.dart' as img;

import '../models/companion_image_payload.dart';
import 'display_bitmap_codec.dart';

class NoteCardBitmapRenderer {
  NoteCardBitmapRenderer._();

  static Future<CompanionImagePayload> render({
    required Uint8List previewPng,
    required String text,
    required int fontSize,
    required int topStickerCount,
    required int bottomStickerCount,
  }) async {
    final baseImage = img.decodeImage(previewPng);
    if (baseImage == null) {
      throw const FormatException('Could not decode note preview image.');
    }

    final noteText = text.trim().isEmpty ? 'Your note here' : text.trim();
    const left = 10;
    const top = 15;
    const width = 300;
    const height = 210;
    final textTopInset = topStickerCount > 0 ? 37 : 0;
    final textBottomInset = bottomStickerCount > 0 ? 37 : 0;
    final contentTop = top + textTopInset;
    final contentHeight = height - textTopInset - textBottomInset;

    img.fillRect(
      baseImage,
      x1: left,
      y1: contentTop,
      x2: left + width - 1,
      y2: contentTop + contentHeight - 1,
      color: img.ColorRgb8(0, 0, 0),
    );

    final overlayPng = await _renderTextOverlayPng(
      text: noteText,
      fontSize: fontSize,
      left: left,
      contentTop: contentTop,
      width: width,
      contentHeight: contentHeight,
    );
    final overlayImage = img.decodeImage(overlayPng);
    if (overlayImage != null) {
      // Hard-key composite: any overlay pixel with alpha > 20 becomes
      // pure white in the base image. This prevents antialiased thin
      // text from being killed by the display threshold.
      for (var y = 0; y < overlayImage.height && y < baseImage.height; y++) {
        for (var x = 0; x < overlayImage.width && x < baseImage.width; x++) {
          final p = overlayImage.getPixel(x, y);
          if (p.a > 20) {
            baseImage.setPixelRgb(x, y, 255, 255, 255);
          }
        }
      }
    }

    return DisplayBitmapCodec.encodeImage(
      sourceBytes: Uint8List.fromList(img.encodePng(baseImage)),
      name: 'custom_note_card',
      threshold: 128,
      invert: true,
    );
  }

  static Future<Uint8List> _renderTextOverlayPng({
    required String text,
    required int fontSize,
    required int left,
    required int contentTop,
    required int width,
    required int contentHeight,
  }) async {
    const scale = 4.0;
    final recorder = ui.PictureRecorder();
    final canvas = Canvas(recorder);
    canvas.scale(scale);

    final textPainter = TextPainter(
      text: TextSpan(text: text, style: _textStyle(fontSize)),
      textAlign: TextAlign.center,
      textDirection: TextDirection.ltr,
      maxLines: switch (fontSize) {
        >= 5 => 3,
        4 => 6,
        3 => 10,
        2 => 14,
        _ => 18,
      },
      strutStyle: StrutStyle(
        forceStrutHeight: true,
        fontSize: _fontPixels(fontSize),
        height: 0.98,
      ),
    )..layout(maxWidth: width.toDouble());

    final dx = left + ((width - textPainter.width) / 2);
    final dy = contentTop + ((contentHeight - textPainter.height) / 2);
    textPainter.paint(
      canvas,
      Offset(dx, dy < contentTop ? contentTop.toDouble() : dy),
    );

    final image = await recorder.endRecording().toImage(
          (DisplayBitmapCodec.width * scale).round(),
          (DisplayBitmapCodec.height * scale).round(),
        );
    final byteData = await image.toByteData(format: ui.ImageByteFormat.png);
    if (byteData == null) {
      throw const FormatException('Could not render note text overlay.');
    }

    final highRes = img.decodeImage(byteData.buffer.asUint8List());
    if (highRes == null) {
      throw const FormatException('Could not decode note text overlay.');
    }

    final downsampled = img.copyResize(
      highRes,
      width: DisplayBitmapCodec.width,
      height: DisplayBitmapCodec.height,
      interpolation: img.Interpolation.average,
    );
    return Uint8List.fromList(img.encodePng(downsampled));
  }

  static TextStyle _textStyle(int fontSize) {
    return TextStyle(
      color: const Color(0xFFFFFFFF),
      fontSize: _fontPixels(fontSize),
      fontWeight: switch (fontSize) {
        >= 5 => FontWeight.w700,
        4 => FontWeight.w600,
        3 => FontWeight.w500,
        _ => FontWeight.w400,
      },
      height: 0.98,
      letterSpacing: switch (fontSize) {
        >= 5 => -0.5,
        4 => -0.2,
        3 => -0.1,
        2 => -0.05,
        _ => 0.0,
      },
    );
  }

  static double _fontPixels(int fontSize) {
    return switch (fontSize) {
      >= 6 => 38,
      5 => 28,
      4 => 18,
      3 => 12.5,
      2 => 9.75,
      _ => 8.2,
    };
  }
}
