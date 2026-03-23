import 'dart:typed_data';

class CompanionImagePayload {
  const CompanionImagePayload({
    required this.name,
    required this.bitmap,
    required this.previewPng,
  });

  final String name;
  final Uint8List bitmap;
  final Uint8List previewPng;

  int get byteLength => bitmap.length;
}