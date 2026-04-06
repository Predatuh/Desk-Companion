import 'dart:typed_data';

import 'package:image/image.dart' as img;

import '../models/companion_image_payload.dart';

class DisplayBitmapCodec {
  DisplayBitmapCodec._();

  static const int width = 320;
  static const int height = 240;
  static const int byteLength = width * height ~/ 8;
  static const int colorByteLength = width * height * 2; // RGB565

  /// Encode an image into a full-color RGB565 buffer for the TFT display.
  static CompanionImagePayload encodeColorImage({
    required Uint8List sourceBytes,
    required String name,
  }) {
    final decoded = img.decodeImage(sourceBytes);
    if (decoded == null) {
      throw const FormatException('Unsupported image file.');
    }

    final fitted = _fitToCanvasColor(decoded);
    final rgb565 = Uint8List(colorByteLength);

    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        final pixel = fitted.getPixel(x, y);
        final r = pixel.r.toInt();
        final g = pixel.g.toInt();
        final b = pixel.b.toInt();
        // Pack into RGB565: RRRRRGGGGGGBBBBB (big-endian for ESP32)
        final rgb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        final offset = (y * width + x) * 2;
        rgb565[offset] = (rgb >> 8) & 0xFF;
        rgb565[offset + 1] = rgb & 0xFF;
      }
    }

    // Generate a small preview PNG (use the fitted image directly)
    final previewPng = Uint8List.fromList(img.encodePng(fitted));

    return CompanionImagePayload(
      name: name,
      bitmap: rgb565,
      previewPng: previewPng,
      isColor: true,
    );
  }

  static CompanionImagePayload encodeImage({
    required Uint8List sourceBytes,
    required String name,
    int threshold = 148,
    bool invert = false,
  }) {
    final decoded = img.decodeImage(sourceBytes);
    if (decoded == null) {
      throw const FormatException('Unsupported image file.');
    }

    final fitted = _fitToCanvas(decoded);
    final bitmap = Uint8List(byteLength);
    final preview = img.Image(width: width, height: height);

    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        final pixel = fitted.getPixel(x, y);
        final luminance = img.getLuminance(pixel);
        var on = luminance < threshold;
        if (invert) {
          on = !on;
        }

        final byteIndex = y * (width ~/ 8) + (x >> 3);
        if (on) {
          bitmap[byteIndex] |= 1 << (7 - (x & 7));
          preview.setPixelRgb(x, y, 255, 255, 255);
        } else {
          preview.setPixelRgb(x, y, 0, 0, 0);
        }
      }
    }

    return CompanionImagePayload(
      name: name,
      bitmap: bitmap,
      previewPng: Uint8List.fromList(img.encodePng(preview)),
    );
  }

  static CompanionImagePayload fromBitmap({
    required Uint8List bitmap,
    String name = 'bitmap',
  }) {
    if (bitmap.length != byteLength) {
      throw ArgumentError('Bitmap must be exactly $byteLength bytes for ${width}x$height TFT output.');
    }

    return CompanionImagePayload(
      name: name,
      bitmap: Uint8List.fromList(bitmap),
      previewPng: Uint8List.fromList(img.encodePng(_previewFromBitmap(bitmap))),
    );
  }

  static CompanionImagePayload fromRgbaBytes({
    required Uint8List rgbaBytes,
    required int sourceWidth,
    required int sourceHeight,
    required String name,
    int threshold = 148,
    bool invert = false,
  }) {
    if (sourceWidth != width || sourceHeight != height) {
      throw ArgumentError(
        'RGBA source must be exactly ${width}x$height for TFT output.',
      );
    }

    final bitmap = Uint8List(byteLength);
    final preview = img.Image(width: width, height: height);

    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        final pixelIndex = (y * sourceWidth + x) * 4;
        final red = rgbaBytes[pixelIndex];
        final green = rgbaBytes[pixelIndex + 1];
        final blue = rgbaBytes[pixelIndex + 2];
        final luminance = ((red * 299) + (green * 587) + (blue * 114)) ~/ 1000;

        var on = luminance < threshold;
        if (invert) {
          on = !on;
        }

        final byteIndex = y * (width ~/ 8) + (x >> 3);
        if (on) {
          bitmap[byteIndex] |= 1 << (7 - (x & 7));
          preview.setPixelRgb(x, y, 255, 255, 255);
        } else {
          preview.setPixelRgb(x, y, 0, 0, 0);
        }
      }
    }

    return CompanionImagePayload(
      name: name,
      bitmap: bitmap,
      previewPng: Uint8List.fromList(img.encodePng(preview)),
    );
  }

  static img.Image _fitToCanvas(img.Image source) {
    final scale = _min(width / source.width, height / source.height);
    final resized = img.copyResize(
      source,
      width: (source.width * scale).round().clamp(1, width),
      height: (source.height * scale).round().clamp(1, height),
      interpolation: img.Interpolation.average,
    );

    final grayscale = img.grayscale(resized);
    final canvas = img.Image(width: width, height: height);
    img.fill(canvas, color: img.ColorRgb8(255, 255, 255));

    final offsetX = ((width - grayscale.width) / 2).floor();
    final offsetY = ((height - grayscale.height) / 2).floor();
    img.compositeImage(canvas, grayscale, dstX: offsetX, dstY: offsetY);
    return canvas;
  }

  static img.Image _fitToCanvasColor(img.Image source) {
    final scale = _min(width / source.width, height / source.height);
    final resized = img.copyResize(
      source,
      width: (source.width * scale).round().clamp(1, width),
      height: (source.height * scale).round().clamp(1, height),
      interpolation: img.Interpolation.average,
    );

    final canvas = img.Image(width: width, height: height);
    img.fill(canvas, color: img.ColorRgb8(0, 0, 0));

    final offsetX = ((width - resized.width) / 2).floor();
    final offsetY = ((height - resized.height) / 2).floor();
    img.compositeImage(canvas, resized, dstX: offsetX, dstY: offsetY);
    return canvas;
  }

  static img.Image _previewFromBitmap(Uint8List bitmap) {
    final preview = img.Image(width: width, height: height);
    for (var y = 0; y < height; y++) {
      for (var x = 0; x < width; x++) {
        final byteIndex = y * (width ~/ 8) + (x >> 3);
        final bitMask = 1 << (7 - (x & 7));
        final on = (bitmap[byteIndex] & bitMask) != 0;
        if (on) {
          preview.setPixelRgb(x, y, 255, 255, 255);
        } else {
          preview.setPixelRgb(x, y, 0, 0, 0);
        }
      }
    }
    return preview;
  }

  static double _min(double a, double b) => a < b ? a : b;
}
