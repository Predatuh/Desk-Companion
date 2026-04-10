import 'dart:math' as math;
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';

import '../models/companion_image_payload.dart';
import '../models/companion_scene.dart';
import '../models/companion_visual_model.dart';
import '../utils/display_bitmap_codec.dart';

Future<CompanionImagePayload> renderCompanionFacePreviewPayload({
  required CompanionVisualModel visualModel,
  required CompanionScene scene,
  required String personality,
  required String petMode,
  required bool referencePose,
  String? expression,
  required String hair,
  required String ears,
  required String mustache,
  required String glasses,
  required String headwear,
  required String piercing,
  required int hairSize,
  required int mustacheSize,
  required int hairWidth,
  required int hairHeight,
  required int hairThickness,
  required int hairOffsetX,
  required int hairOffsetY,
  required int eyeOffsetX,
  required int eyeOffsetY,
  required int mouthOffsetX,
  required int mouthOffsetY,
  required int mustacheWidth,
  required int mustacheHeight,
  required int mustacheThickness,
  required int mustacheOffsetX,
  required int mustacheOffsetY,
  int stickFigureScale = 100,
  int stickFigureSpacing = 100,
  int stickFigureEnergy = 55,
  double? animationProgress,
  String name = 'studio_scene',
  Color? eyeColor,
  Color? faceColor,
  Color? accentColor,
  Color? bodyColor,
}) async {
  final progress = referencePose
      ? 0.0
      : animationProgress ?? _sceneSnapshotProgress(visualModel, scene);
  final painter = switch (visualModel) {
    CompanionVisualModel.classic => _CompanionFacePainter(
        personality: personality,
        petMode: petMode,
        referencePose: referencePose,
        showScreenBoundary: false,
        expression: expression,
        hair: hair,
        ears: ears,
        mustache: mustache,
        glasses: glasses,
        headwear: headwear,
        piercing: piercing,
        hairSize: hairSize,
        mustacheSize: mustacheSize,
        hairWidth: hairWidth,
        hairHeight: hairHeight,
        hairThickness: hairThickness,
        hairOffsetX: hairOffsetX,
        hairOffsetY: hairOffsetY,
        eyeOffsetX: eyeOffsetX,
        eyeOffsetY: eyeOffsetY,
        mouthOffsetX: mouthOffsetX,
        mouthOffsetY: mouthOffsetY,
        mustacheWidth: mustacheWidth,
        mustacheHeight: mustacheHeight,
        mustacheThickness: mustacheThickness,
        mustacheOffsetX: mustacheOffsetX,
        mustacheOffsetY: mustacheOffsetY,
        eyeColor: eyeColor,
        faceColor: faceColor,
        accentColor: accentColor,
        bodyColor: bodyColor,
      ),
    CompanionVisualModel.stickFigure => _StickFigurePainter(
        scene: scene,
        personality: personality,
        petMode: petMode,
        referencePose: referencePose,
        showScreenBoundary: false,
        expression: expression,
        stickFigureScale: stickFigureScale,
        stickFigureSpacing: stickFigureSpacing,
        stickFigureEnergy: stickFigureEnergy,
        animationProgress: progress,
        eyeColor: eyeColor,
        faceColor: faceColor,
        accentColor: accentColor,
        bodyColor: bodyColor,
      ),
    CompanionVisualModel.robot => _RobotPainter(
        scene: scene,
        showScreenBoundary: false,
        expression: expression,
        animationProgress: progress,
        eyeColor: eyeColor,
        faceColor: faceColor,
        accentColor: accentColor,
        bodyColor: bodyColor,
      ),
  };

  final recorder = ui.PictureRecorder();
  final canvas = Canvas(recorder);
  canvas.drawRect(
    const Rect.fromLTWH(0, 0, 320, 240),
    Paint()
      ..color = Colors.black
      ..style = PaintingStyle.fill,
  );
  painter.paint(canvas, const Size(320, 240));

  final image = await recorder.endRecording().toImage(320, 240);
  final byteData = await image.toByteData(format: ui.ImageByteFormat.rawRgba);
  if (byteData == null) {
    throw const FormatException('Could not render studio preview.');
  }

  return DisplayBitmapCodec.fromRgbaBytes(
    rgbaBytes: Uint8List.fromList(byteData.buffer.asUint8List()),
    sourceWidth: 320,
    sourceHeight: 240,
    name: name,
    threshold: 128,
    invert: true,
  );
}

double _sceneSnapshotProgress(
  CompanionVisualModel visualModel,
  CompanionScene scene,
) {
  return switch (visualModel) {
    CompanionVisualModel.classic => 0.0,
    CompanionVisualModel.stickFigure => switch (scene) {
        CompanionScene.holdHands => 0.64,
        CompanionScene.hug => 0.48,
        CompanionScene.kiss => 0.56,
        CompanionScene.shyLeanIn => 0.52,
        CompanionScene.wave => 0.2,
        CompanionScene.bow => 0.5,
        CompanionScene.none => 0.0,
      },
    CompanionVisualModel.robot => switch (scene) {
        CompanionScene.holdHands => 0.56,
        CompanionScene.kiss => 0.52,
        CompanionScene.shyLeanIn => 0.5,
        CompanionScene.hug => 0.46,
        CompanionScene.wave => 0.2,
        CompanionScene.bow => 0.5,
        CompanionScene.none => 0.0,
      },
  };
}

class CompanionFacePreview extends StatefulWidget {
  const CompanionFacePreview({
    super.key,
    required this.visualModel,
    required this.scene,
    required this.personality,
    required this.petMode,
    this.referencePose = false,
    this.showScreenBoundary = false,
    this.expression,
    required this.hair,
    required this.ears,
    required this.mustache,
    required this.glasses,
    required this.headwear,
    required this.piercing,
    required this.hairSize,
    required this.mustacheSize,
    required this.hairWidth,
    required this.hairHeight,
    required this.hairThickness,
    required this.hairOffsetX,
    required this.hairOffsetY,
    required this.eyeOffsetX,
    required this.eyeOffsetY,
    required this.mouthOffsetX,
    required this.mouthOffsetY,
    required this.mustacheWidth,
    required this.mustacheHeight,
    required this.mustacheThickness,
    required this.mustacheOffsetX,
    required this.mustacheOffsetY,
    this.stickFigureScale = 100,
    this.stickFigureSpacing = 100,
    this.stickFigureEnergy = 55,
    this.eyeColor,
    this.faceColor,
    this.accentColor,
    this.bodyColor,
  });

  final CompanionVisualModel visualModel;
  final CompanionScene scene;
  final String personality;
  final String petMode;
  final bool referencePose;
  final bool showScreenBoundary;
  final String? expression;
  final String hair;
  final String ears;
  final String mustache;
  final String glasses;
  final String headwear;
  final String piercing;
  final int hairSize;
  final int mustacheSize;
  final int hairWidth;
  final int hairHeight;
  final int hairThickness;
  final int hairOffsetX;
  final int hairOffsetY;
  final int eyeOffsetX;
  final int eyeOffsetY;
  final int mouthOffsetX;
  final int mouthOffsetY;
  final int mustacheWidth;
  final int mustacheHeight;
  final int mustacheThickness;
  final int mustacheOffsetX;
  final int mustacheOffsetY;
  final int stickFigureScale;
  final int stickFigureSpacing;
  final int stickFigureEnergy;
  final Color? eyeColor;
  final Color? faceColor;
  final Color? accentColor;
  final Color? bodyColor;

  @override
  State<CompanionFacePreview> createState() => _CompanionFacePreviewState();
}

class _CompanionFacePreviewState extends State<CompanionFacePreview>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      vsync: this,
      duration: _resolveDuration(),
    )..repeat();
  }

  @override
  void didUpdateWidget(covariant CompanionFacePreview oldWidget) {
    super.didUpdateWidget(oldWidget);
    final nextDuration = _resolveDuration();
    if (_controller.duration != nextDuration) {
      _controller
        ..duration = nextDuration
        ..repeat();
    }
  }

  Duration _resolveDuration() {
    if (widget.referencePose) {
      return const Duration(milliseconds: 2400);
    }

    return switch (widget.visualModel) {
      CompanionVisualModel.classic => const Duration(milliseconds: 2400),
      CompanionVisualModel.stickFigure => switch (widget.scene) {
          CompanionScene.holdHands => const Duration(milliseconds: 7200),
          CompanionScene.hug || CompanionScene.kiss || CompanionScene.shyLeanIn =>
            const Duration(milliseconds: 5200),
          CompanionScene.wave || CompanionScene.bow =>
            const Duration(milliseconds: 3600),
          CompanionScene.none => const Duration(milliseconds: 3000),
        },
      CompanionVisualModel.robot => const Duration(milliseconds: 4200),
    };
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: _controller,
      builder: (context, _) {
        final animationProgress = widget.referencePose ? 0.0 : _controller.value;
        final painter = switch (widget.visualModel) {
          CompanionVisualModel.classic => _CompanionFacePainter(
              personality: widget.personality,
              petMode: widget.petMode,
              referencePose: widget.referencePose,
              showScreenBoundary: widget.showScreenBoundary,
              expression: widget.expression,
              hair: widget.hair,
              ears: widget.ears,
              mustache: widget.mustache,
              glasses: widget.glasses,
              headwear: widget.headwear,
              piercing: widget.piercing,
              hairSize: widget.hairSize,
              mustacheSize: widget.mustacheSize,
              hairWidth: widget.hairWidth,
              hairHeight: widget.hairHeight,
              hairThickness: widget.hairThickness,
              hairOffsetX: widget.hairOffsetX,
              hairOffsetY: widget.hairOffsetY,
              eyeOffsetX: widget.eyeOffsetX,
              eyeOffsetY: widget.eyeOffsetY,
              mouthOffsetX: widget.mouthOffsetX,
              mouthOffsetY: widget.mouthOffsetY,
              mustacheWidth: widget.mustacheWidth,
              mustacheHeight: widget.mustacheHeight,
              mustacheThickness: widget.mustacheThickness,
              mustacheOffsetX: widget.mustacheOffsetX,
              mustacheOffsetY: widget.mustacheOffsetY,
              animationProgress: animationProgress,
              eyeColor: widget.eyeColor,
              faceColor: widget.faceColor,
              accentColor: widget.accentColor,
              bodyColor: widget.bodyColor,
            ),
          CompanionVisualModel.stickFigure => _StickFigurePainter(
              scene: widget.scene,
              personality: widget.personality,
              petMode: widget.petMode,
              referencePose: widget.referencePose,
              showScreenBoundary: widget.showScreenBoundary,
              expression: widget.expression,
              stickFigureScale: widget.stickFigureScale,
              stickFigureSpacing: widget.stickFigureSpacing,
              stickFigureEnergy: widget.stickFigureEnergy,
              animationProgress: animationProgress,
              eyeColor: widget.eyeColor,
              faceColor: widget.faceColor,
              accentColor: widget.accentColor,
              bodyColor: widget.bodyColor,
            ),
          CompanionVisualModel.robot => _RobotPainter(
              scene: widget.scene,
              showScreenBoundary: widget.showScreenBoundary,
              expression: widget.expression,
              animationProgress: animationProgress,
              eyeColor: widget.eyeColor,
              faceColor: widget.faceColor,
              accentColor: widget.accentColor,
              bodyColor: widget.bodyColor,
            ),
        };

        return RepaintBoundary(
          child: AspectRatio(
            aspectRatio: 2,
            child: CustomPaint(
              painter: painter,
            ),
          ),
        );
      },
    );
  }
}

class _StickFigurePainter extends CustomPainter {
  const _StickFigurePainter({
    required this.scene,
    required this.personality,
    required this.petMode,
    required this.referencePose,
    required this.showScreenBoundary,
    required this.expression,
    required this.stickFigureScale,
    required this.stickFigureSpacing,
    required this.stickFigureEnergy,
    required this.animationProgress,
    this.eyeColor,
    this.faceColor,
    this.accentColor,
    this.bodyColor,
  });

  final CompanionScene scene;
  final String personality;
  final String petMode;
  final bool referencePose;
  final bool showScreenBoundary;
  final String? expression;
  final int stickFigureScale;
  final int stickFigureSpacing;
  final int stickFigureEnergy;
  final double animationProgress;
  final Color? eyeColor;
  final Color? faceColor;
  final Color? accentColor;
  final Color? bodyColor;

  static const _visibleScreenRect = Rect.fromLTWH(2, 2, 316, 236);

  @override
  void paint(Canvas canvas, Size size) {
    canvas.save();
    canvas.scale(size.width / 320, size.height / 240);

    final resolvedFaceColor = faceColor ?? Colors.white;

    final stroke = Paint()
      ..color = resolvedFaceColor
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round
      ..isAntiAlias = true;
    final fill = Paint()
      ..color = resolvedFaceColor
      ..style = PaintingStyle.fill
      ..isAntiAlias = true;
    final mask = Paint()
      ..color = Colors.black.withValues(alpha: 0.78)
      ..style = PaintingStyle.fill
      ..isAntiAlias = true;
    final boundary = Paint()
      ..color = const Color(0xFF9B9B9B)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2
      ..isAntiAlias = true;

    canvas.drawRRect(
      RRect.fromRectAndRadius(
        const Rect.fromLTWH(0, 0, 319, 239),
        const Radius.circular(18),
      ),
      stroke,
    );

    final normalizedExpression = expression?.trim();
    final sceneToRender = referencePose ? CompanionScene.none : scene;
    final scale = stickFigureScale.clamp(70, 150) / 100;
    final spacing = 45 * (stickFigureSpacing.clamp(70, 150) / 100);
    final energy = stickFigureEnergy.clamp(0, 100) / 100;
    final isSleepy = !referencePose &&
        (normalizedExpression == 'sleepy' ||
            (normalizedExpression == null &&
                (petMode == 'nap' || personality == 'sleepy')));
    final isExcited = !referencePose &&
        (normalizedExpression == 'excited' ||
            normalizedExpression == 'laugh' ||
            normalizedExpression == 'heart' ||
            petMode == 'party' ||
            petMode == 'play' ||
            personality == 'playful');
    final isAffectionate = !referencePose &&
        (normalizedExpression == 'kiss' ||
            normalizedExpression == 'love' ||
            normalizedExpression == 'heart' ||
            petMode == 'cuddle' ||
            personality == 'cuddly');
    final isThinking = !referencePose && normalizedExpression == 'thinking';
    final isSad = !referencePose && normalizedExpression == 'sad';
    final isSurprised = !referencePose && normalizedExpression == 'surprised';
    final isLooking = !referencePose && normalizedExpression == 'look_around';
    final isOff = !referencePose && normalizedExpression == null && petMode == 'off';

    switch (sceneToRender) {
      case CompanionScene.holdHands:
        _paintHoldHandsScene(canvas, stroke, fill, scale, spacing, energy);
      case CompanionScene.hug:
        _paintDuoMoment(
          canvas,
          stroke,
          fill,
          scale: scale,
          spacing: spacing * 0.82,
          energy: energy,
          scene: sceneToRender,
          showHeart: false,
        );
      case CompanionScene.kiss:
        _paintDuoMoment(
          canvas,
          stroke,
          fill,
          scale: scale,
          spacing: spacing * 0.76,
          energy: energy,
          scene: sceneToRender,
          showHeart: true,
        );
      case CompanionScene.shyLeanIn:
        _paintDuoMoment(
          canvas,
          stroke,
          fill,
          scale: scale,
          spacing: spacing * 0.9,
          energy: energy,
          scene: sceneToRender,
          showHeart: true,
        );
      case CompanionScene.wave:
      case CompanionScene.bow:
      case CompanionScene.none:
        _paintSoloMoment(
          canvas,
          stroke,
          fill,
          scale: scale,
          energy: energy,
          scene: sceneToRender,
          isOff: isOff,
          isSleepy: isSleepy,
          isExcited: isExcited,
          isAffectionate: isAffectionate,
          isThinking: isThinking,
          isSad: isSad,
          isSurprised: isSurprised,
          isLooking: isLooking,
        );
    }

    if (showScreenBoundary) {
      canvas.drawRect(Rect.fromLTWH(0, 0, 320, _visibleScreenRect.top), mask);
      canvas.drawRect(Rect.fromLTWH(0, _visibleScreenRect.bottom, 320, 240 - _visibleScreenRect.bottom), mask);
      canvas.drawRect(Rect.fromLTWH(0, _visibleScreenRect.top, _visibleScreenRect.left, _visibleScreenRect.height), mask);
      canvas.drawRect(Rect.fromLTWH(_visibleScreenRect.right, _visibleScreenRect.top, 320 - _visibleScreenRect.right, _visibleScreenRect.height), mask);
      canvas.drawRect(_visibleScreenRect, boundary);
    }

    canvas.restore();
  }

  void _paintSoloMoment(
    Canvas canvas,
    Paint stroke,
    Paint fill, {
    required double scale,
    required double energy,
    required CompanionScene scene,
    required bool isOff,
    required bool isSleepy,
    required bool isExcited,
    required bool isAffectionate,
    required bool isThinking,
    required bool isSad,
    required bool isSurprised,
    required bool isLooking,
  }) {
    final bob = math.sin(animationProgress * math.pi * 2) * (isSleepy ? 1.5 : 4.0);
    final waveLift = scene == CompanionScene.wave
        ? (math.sin(animationProgress * math.pi * 4) * (21 + energy * 12)).abs()
        : 0.0;
    final bowLean = scene == CompanionScene.bow
        ? _lerp(0, 19.5, (math.sin(animationProgress * math.pi * 2) * 0.5) + 0.5)
        : 0.0;
    final stride = math.sin(animationProgress * math.pi * 8) * (scene == CompanionScene.wave ? 4.5 : 1.5);

    _drawFigure(
      canvas,
      stroke,
      fill,
      headCenter: Offset(160 + bowLean * 0.35, 68 + bob + bowLean * 0.2),
      scale: scale,
      lean: bowLean,
      leftArmVector: Offset(-36 * scale, scene == CompanionScene.bow ? 6 : 0),
      rightArmVector: Offset(36 * scale, -waveLift),
      leftLegVector: Offset(-21 * scale + stride, 30 * scale),
      rightLegVector: Offset(21 * scale - stride, 30 * scale),
      facingRight: true,
      isOff: isOff,
      isSleepy: isSleepy,
      isExcited: isExcited,
      isAffectionate: isAffectionate,
      isThinking: isThinking,
      isSad: isSad,
      isSurprised: isSurprised,
      isLooking: isLooking,
    );

    if (scene == CompanionScene.wave) {
      _drawSpark(canvas, stroke, const Offset(205, 49), 9);
    }
  }

  void _paintDuoMoment(
    Canvas canvas,
    Paint stroke,
    Paint fill, {
    required double scale,
    required double spacing,
    required double energy,
    required CompanionScene scene,
    required bool showHeart,
  }) {
    final leftCenter = 160 - spacing / 2;
    final rightCenter = 160 + spacing / 2;
    final sway = math.sin(animationProgress * math.pi * 2) * (3 + energy * 1.8);
    final stride = math.sin(animationProgress * math.pi * 6) * 3.3;
    final lean = scene == CompanionScene.kiss
        ? 10.8
        : scene == CompanionScene.hug
            ? 7.8
            : 5.4;

    _drawFigure(
      canvas,
      stroke,
      fill,
      headCenter: Offset(leftCenter + lean, 68 + sway * 0.3),
      scale: scale,
      lean: lean,
      leftArmVector: Offset(-33 * scale, 6),
      rightArmVector: Offset(21 * scale, scene == CompanionScene.hug ? 6 : 12),
      leftLegVector: Offset(-21 * scale + stride, 30 * scale),
      rightLegVector: Offset(21 * scale - stride, 30 * scale),
      facingRight: true,
      isAffectionate: true,
    );
    _drawFigure(
      canvas,
      stroke,
      fill,
      headCenter: Offset(rightCenter - lean, 68 - sway * 0.3),
      scale: scale,
      lean: -lean,
      leftArmVector: Offset(-21 * scale, scene == CompanionScene.hug ? 6 : 12),
      rightArmVector: Offset(33 * scale, 6),
      leftLegVector: Offset(-21 * scale - stride, 30 * scale),
      rightLegVector: Offset(21 * scale + stride, 30 * scale),
      facingRight: false,
      isAffectionate: true,
    );

    if (showHeart) {
      final heartY = scene == CompanionScene.kiss
          ? 53 - math.sin(animationProgress * math.pi * 2) * 6
          : 64 - math.sin(animationProgress * math.pi * 2) * 3.6;
      _drawHeart(canvas, fill, Offset(160, heartY), scene == CompanionScene.kiss ? 12 : 9);
    }
  }

  void _paintHoldHandsScene(
    Canvas canvas,
    Paint stroke,
    Paint fill,
    double scale,
    double spacing,
    double energy,
  ) {
    final approach = Curves.easeOutCubic.transform(_segment(animationProgress, 0.0, 0.46));
    final reach = Curves.easeInOut.transform(_segment(animationProgress, 0.34, 0.6));
    final hold = Curves.easeInOut.transform(_segment(animationProgress, 0.58, 0.74));
    final away = Curves.easeIn.transform(_segment(animationProgress, 0.74, 1.0));
    final visibility = 1 - Curves.easeIn.transform(_segment(animationProgress, 0.9, 1.0));
    if (visibility <= 0.03) {
      return;
    }

    final sceneStroke = Paint()
      ..color = Colors.white.withValues(alpha: visibility)
      ..style = PaintingStyle.stroke
      ..strokeWidth = currentScaleStrokeWidth(scale, away)
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round
      ..isAntiAlias = true;
    final sceneFill = Paint()
      ..color = Colors.white.withValues(alpha: visibility)
      ..style = PaintingStyle.fill
      ..isAntiAlias = true;
    final holdHalfGap = (spacing * 0.62).clamp(30.0, 40.0);
    final meetingLeft = 160.0 - holdHalfGap;
    final meetingRight = 160.0 + holdHalfGap;
    final awayHalfGap = _lerp(holdHalfGap, 3.6, away);
    final awayLeft = 160.0 - awayHalfGap;
    final awayRight = 160.0 + awayHalfGap;
    final walkInLeft = _lerp(-40, meetingLeft, approach);
    final walkInRight = _lerp(360, meetingRight, approach);
    final pairedLeft = _lerp(meetingLeft, awayLeft, away);
    final pairedRight = _lerp(meetingRight, awayRight, away);
    final leftX = away > 0 ? pairedLeft : walkInLeft;
    final rightX = away > 0 ? pairedRight : walkInRight;
    final currentScale = scale * _lerp(1.0, 0.06, away);
    final baseY = _lerp(68, 15, away);
    final walkPhase = animationProgress * math.pi * 10;
    final stride = math.sin(walkPhase) * _lerp(7.2 + energy * 4.8, 0.6, away);
    final bobAmount = _lerp(3.6 + energy * 1.8, 0.36, away);
    final leftY = baseY + math.sin(walkPhase) * bobAmount;
    final rightY = baseY + math.sin(walkPhase + math.pi) * bobAmount * 0.82;
    final leftShoulder = Offset(leftX, leftY + 45 * currentScale);
    final rightShoulder = Offset(rightX, rightY + 45 * currentScale);
    final relaxedInnerLeftArm = Offset(24.6 * currentScale, 7.2 + math.sin(walkPhase) * 2.4);
    final relaxedInnerRightArm = Offset(-24.6 * currentScale, 7.2 - math.sin(walkPhase) * 2.4);
    final outerLeftArm = Offset(-30.6 * currentScale, _lerp(7.2, 3.0, away));
    final outerRightArm = Offset(30.6 * currentScale, _lerp(7.2, 3.0, away));
    final handJoin = Offset(
      (leftShoulder.dx + rightShoulder.dx) / 2,
      ((leftShoulder.dy + rightShoulder.dy) / 2) + _lerp(7.8, 1.2, hold) + _lerp(0.0, 2.4, away),
    );
    final leftInnerArm = Offset.lerp(relaxedInnerLeftArm, handJoin - leftShoulder, reach)!;
    final rightInnerArm = Offset.lerp(relaxedInnerRightArm, handJoin - rightShoulder, reach)!;

    _drawFigure(
      canvas,
      sceneStroke,
      sceneFill,
      headCenter: Offset(leftX, leftY),
      scale: currentScale,
      lean: _lerp(0.0, 3.45, reach) - away * 0.66,
      leftArmVector: outerLeftArm,
      rightArmVector: leftInnerArm,
      leftLegVector: Offset(-21 * currentScale + stride, 30 * currentScale),
      rightLegVector: Offset(21 * currentScale - stride, 30 * currentScale),
      facingRight: true,
      rightArmBend: 6.3 * currentScale,
      leftArmBend: -3.6 * currentScale,
      leftLegBend: -3.0 * currentScale,
      rightLegBend: 2.7 * currentScale,
    );
    _drawFigure(
      canvas,
      sceneStroke,
      sceneFill,
      headCenter: Offset(rightX, rightY),
      scale: currentScale,
      lean: _lerp(0.0, -3.45, reach) + away * 0.66,
      leftArmVector: rightInnerArm,
      rightArmVector: outerRightArm,
      leftLegVector: Offset(-21 * currentScale - stride, 30 * currentScale),
      rightLegVector: Offset(21 * currentScale + stride, 30 * currentScale),
      facingRight: false,
      leftArmBend: -6.3 * currentScale,
      rightArmBend: 3.6 * currentScale,
      leftLegBend: -2.7 * currentScale,
      rightLegBend: 3.0 * currentScale,
    );

    if (reach > 0.72) {
      final claspSize = math.max(1.35, 2.85 * currentScale) * visibility;
      canvas.drawCircle(handJoin, claspSize, sceneFill);
    }

    if (reach > 0 || away > 0) {
      final heartRise = baseY - _lerp(0, 10.5, hold) - _lerp(0, 33, away);
      final heartSize = _lerp(0.0, 11.4, math.max(reach, hold)) * visibility;
      if (heartSize > 0.75) {
        _drawHeart(canvas, sceneFill, Offset(160, heartRise), heartSize);
      }
    }
  }

  void _drawFigure(
    Canvas canvas,
    Paint stroke,
    Paint fill, {
    required Offset headCenter,
    required double scale,
    required double lean,
    required Offset leftArmVector,
    required Offset rightArmVector,
    required Offset leftLegVector,
    required Offset rightLegVector,
    required bool facingRight,
    double? leftArmBend,
    double? rightArmBend,
    double? leftLegBend,
    double? rightLegBend,
    bool isOff = false,
    bool isSleepy = false,
    bool isExcited = false,
    bool isAffectionate = false,
    bool isThinking = false,
    bool isSad = false,
    bool isSurprised = false,
    bool isLooking = false,
  }) {
    final direction = facingRight ? 1.0 : -1.0;
    final headRadius = (isSurprised ? 24.0 : 21.0) * scale;
    final eyeOffsetX = math.max(5.7, 9.0 * scale);
    final eyeRadius = math.max(2.25, 3.6 * scale);
    final mouthOffsetY = math.max(8.1, 18.0 * scale);
    final mouthWidth = math.max(15.0, 30.0 * scale);
    final mouthHeight = math.max(9.6, 18.0 * scale);
    final mouthSadWidth = math.max(12.6, 24.0 * scale);
    final mouthSadHeight = math.max(8.4, 15.0 * scale);
    final faceLineHalf = math.max(6.6, 9.0 * scale);
    final thoughtBubbleNear = math.max(3.0, 3.9 * scale);
    final thoughtBubbleFar = math.max(4.8, 6.0 * scale);
    final emotionOffsetX = headRadius + math.max(13.5, 16.5 * scale);
    final emotionOffsetY = math.max(4.5, 6.0 * scale);
    final torsoTop = Offset(headCenter.dx, headCenter.dy + 30 * scale);
    final torsoBottom = Offset(headCenter.dx + lean * 0.4, headCenter.dy + 78 * scale);
    final shoulder = Offset(headCenter.dx, headCenter.dy + 45 * scale);
    final leftArmEnd = shoulder + leftArmVector;
    final rightArmEnd = shoulder + rightArmVector;
    final leftLegEnd = torsoBottom + leftLegVector;
    final rightLegEnd = torsoBottom + rightLegVector;
    final resolvedLeftArmBend = leftArmBend ?? (facingRight ? -2.7 : 2.7) * scale;
    final resolvedRightArmBend = rightArmBend ?? (facingRight ? 2.7 : -2.7) * scale;
    final resolvedLeftLegBend = leftLegBend ?? (-2.55 * scale);
    final resolvedRightLegBend = rightLegBend ?? (2.55 * scale);

    canvas.drawCircle(headCenter, headRadius, stroke);
    canvas.drawLine(torsoTop, torsoBottom, stroke);
    _drawBentLimb(canvas, stroke, shoulder, leftArmEnd, bend: resolvedLeftArmBend);
    _drawBentLimb(canvas, stroke, shoulder, rightArmEnd, bend: resolvedRightArmBend);
    _drawBentLimb(canvas, stroke, torsoBottom, leftLegEnd, bend: resolvedLeftLegBend);
    _drawBentLimb(canvas, stroke, torsoBottom, rightLegEnd, bend: resolvedRightLegBend);

    if (isOff) {
      canvas.drawLine(
        Offset(headCenter.dx - eyeOffsetX, headCenter.dy - eyeRadius),
        Offset(headCenter.dx - eyeOffsetX + eyeRadius * 1.7, headCenter.dy + eyeRadius),
        stroke,
      );
      canvas.drawLine(
        Offset(headCenter.dx - eyeOffsetX, headCenter.dy + eyeRadius),
        Offset(headCenter.dx - eyeOffsetX + eyeRadius * 1.7, headCenter.dy - eyeRadius),
        stroke,
      );
      canvas.drawLine(
        Offset(headCenter.dx + eyeOffsetX - eyeRadius * 1.7, headCenter.dy - eyeRadius),
        Offset(headCenter.dx + eyeOffsetX, headCenter.dy + eyeRadius),
        stroke,
      );
      canvas.drawLine(
        Offset(headCenter.dx + eyeOffsetX - eyeRadius * 1.7, headCenter.dy + eyeRadius),
        Offset(headCenter.dx + eyeOffsetX, headCenter.dy - eyeRadius),
        stroke,
      );
      canvas.drawLine(
        Offset(headCenter.dx - faceLineHalf - 1, headCenter.dy + mouthOffsetY),
        Offset(headCenter.dx + faceLineHalf + 1, headCenter.dy + mouthOffsetY),
        stroke,
      );
      return;
    }

    if (isSleepy) {
      canvas.drawLine(
        Offset(headCenter.dx - eyeOffsetX - eyeRadius, headCenter.dy),
        Offset(headCenter.dx - eyeOffsetX + eyeRadius, headCenter.dy),
        stroke,
      );
      canvas.drawLine(
        Offset(headCenter.dx + eyeOffsetX - eyeRadius, headCenter.dy),
        Offset(headCenter.dx + eyeOffsetX + eyeRadius, headCenter.dy),
        stroke,
      );
      canvas.drawLine(
        Offset(headCenter.dx - faceLineHalf, headCenter.dy + mouthOffsetY),
        Offset(headCenter.dx + faceLineHalf, headCenter.dy + mouthOffsetY),
        stroke,
      );
      if (headCenter.dx >= 160) {
        _drawZ(canvas, stroke, Offset(headCenter.dx + emotionOffsetX + 1, headCenter.dy - emotionOffsetY - 1), math.max(0.65, 0.9 * scale));
      }
      return;
    }

    final gazeOffset = isLooking ? 6.0 * direction : 0.0;
    canvas.drawCircle(Offset(headCenter.dx - eyeOffsetX + gazeOffset, headCenter.dy), eyeRadius, fill);
    canvas.drawCircle(Offset(headCenter.dx + eyeOffsetX + gazeOffset, headCenter.dy), eyeRadius, fill);

    if (isSad) {
      _drawArcMouth(
        canvas,
        stroke,
        Rect.fromCenter(
          center: Offset(headCenter.dx, headCenter.dy + mouthOffsetY),
          width: mouthSadWidth,
          height: mouthSadHeight,
        ),
        math.pi,
        math.pi,
      );
    } else if (isSurprised) {
      canvas.drawCircle(Offset(headCenter.dx, headCenter.dy + mouthOffsetY), math.max(3.3, 5.7 * scale), stroke);
    } else if (isThinking) {
      canvas.drawLine(
        Offset(headCenter.dx - faceLineHalf, headCenter.dy + mouthOffsetY),
        Offset(headCenter.dx + faceLineHalf, headCenter.dy + mouthOffsetY),
        stroke,
      );
      canvas.drawCircle(Offset(headCenter.dx + emotionOffsetX, headCenter.dy - emotionOffsetY), thoughtBubbleNear, stroke);
      canvas.drawCircle(Offset(headCenter.dx + emotionOffsetX + 12 * scale, headCenter.dy - emotionOffsetY - 9 * scale), thoughtBubbleFar, stroke);
    } else {
      _drawArcMouth(
        canvas,
        stroke,
        Rect.fromCenter(
          center: Offset(headCenter.dx, headCenter.dy + mouthOffsetY),
          width: mouthWidth,
          height: mouthHeight,
        ),
        0.15,
        math.pi - 0.3,
      );
    }

    if (isAffectionate && headCenter.dx > 150) {
      _drawHeart(canvas, fill, Offset(headCenter.dx + emotionOffsetX + 3, headCenter.dy - emotionOffsetY * 0.5), math.max(6.6, 10.5 * scale));
    } else if (isExcited) {
      _drawSpark(canvas, stroke, Offset(headCenter.dx - emotionOffsetX, headCenter.dy - emotionOffsetY), math.max(5.4, 9 * scale));
      _drawSpark(canvas, stroke, Offset(headCenter.dx + emotionOffsetX, headCenter.dy - emotionOffsetY - 3), math.max(5.4, 9 * scale));
    }
  }

  double _segment(double value, double start, double end) {
    if (end <= start) {
      return 0;
    }
    return ((value - start) / (end - start)).clamp(0.0, 1.0);
  }

  double currentScaleStrokeWidth(double scale, double away) {
    final fadedScale = scale * _lerp(1.0, 0.06, away);
    return fadedScale < 0.22 ? 1.2 : 2.0;
  }

  double _lerp(double start, double end, double t) {
    return start + (end - start) * t;
  }

  void _drawBentLimb(
    Canvas canvas,
    Paint stroke,
    Offset start,
    Offset end, {
    required double bend,
  }) {
    final vector = end - start;
    final length = vector.distance;
    if (length < 0.01 || bend.abs() < 0.05) {
      canvas.drawLine(start, end, stroke);
      return;
    }

    final normal = Offset(-vector.dy / length, vector.dx / length);
    final joint = Offset(
      start.dx + vector.dx * 0.52 + normal.dx * bend,
      start.dy + vector.dy * 0.52 + normal.dy * bend,
    );
    canvas.drawLine(start, joint, stroke);
    canvas.drawLine(joint, end, stroke);
  }

  void _drawArcMouth(Canvas canvas, Paint stroke, Rect rect, double startAngle, double sweepAngle) {
    canvas.drawArc(rect, startAngle, sweepAngle, false, stroke);
  }

  void _drawHeart(Canvas canvas, Paint fill, Offset center, double size) {
    final path = Path()
      ..moveTo(center.dx, center.dy + size)
      ..cubicTo(center.dx - size * 1.3, center.dy + size * 0.3, center.dx - size * 1.2, center.dy - size * 0.9, center.dx, center.dy - size * 0.15)
      ..cubicTo(center.dx + size * 1.2, center.dy - size * 0.9, center.dx + size * 1.3, center.dy + size * 0.3, center.dx, center.dy + size);
    canvas.drawPath(path, fill);
  }

  void _drawSpark(Canvas canvas, Paint stroke, Offset center, double size) {
    canvas.drawLine(Offset(center.dx - size, center.dy), Offset(center.dx + size, center.dy), stroke);
    canvas.drawLine(Offset(center.dx, center.dy - size), Offset(center.dx, center.dy + size), stroke);
  }

  void _drawZ(Canvas canvas, Paint stroke, Offset origin, double scale) {
    final width = 12.0 * scale;
    final height = 12.0 * scale;
    canvas.drawLine(origin, Offset(origin.dx + width, origin.dy), stroke);
    canvas.drawLine(Offset(origin.dx + width, origin.dy), Offset(origin.dx, origin.dy + height), stroke);
    canvas.drawLine(Offset(origin.dx, origin.dy + height), Offset(origin.dx + width, origin.dy + height), stroke);
  }

  @override
  bool shouldRepaint(covariant _StickFigurePainter oldDelegate) {
    return scene != oldDelegate.scene ||
        stickFigureScale != oldDelegate.stickFigureScale ||
        stickFigureSpacing != oldDelegate.stickFigureSpacing ||
        stickFigureEnergy != oldDelegate.stickFigureEnergy ||
        animationProgress != oldDelegate.animationProgress ||
        personality != oldDelegate.personality ||
        petMode != oldDelegate.petMode ||
        referencePose != oldDelegate.referencePose ||
        showScreenBoundary != oldDelegate.showScreenBoundary ||
        expression != oldDelegate.expression;
  }
}

class _RobotPainter extends CustomPainter {
  const _RobotPainter({
    required this.scene,
    required this.showScreenBoundary,
    required this.expression,
    required this.animationProgress,
    this.eyeColor,
    this.faceColor,
    this.accentColor,
    this.bodyColor,
  });

  final CompanionScene scene;
  final bool showScreenBoundary;
  final String? expression;
  final double animationProgress;
  final Color? eyeColor;
  final Color? faceColor;
  final Color? accentColor;
  final Color? bodyColor;

  static const _visibleScreenRect = Rect.fromLTWH(2, 2, 316, 236);

  @override
  void paint(Canvas canvas, Size size) {
    canvas.save();
    canvas.scale(size.width / 320, size.height / 240);

    final resolvedFaceColor = faceColor ?? Colors.white;

    final stroke = Paint()
      ..color = resolvedFaceColor
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round
      ..isAntiAlias = true;
    final fill = Paint()
      ..color = resolvedFaceColor
      ..style = PaintingStyle.fill
      ..isAntiAlias = true;
    final mask = Paint()
      ..color = Colors.black.withValues(alpha: 0.78)
      ..style = PaintingStyle.fill
      ..isAntiAlias = true;
    final boundary = Paint()
      ..color = const Color(0xFF9B9B9B)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2
      ..isAntiAlias = true;

    canvas.drawRRect(
      RRect.fromRectAndRadius(
        const Rect.fromLTWH(0, 0, 319, 239),
        const Radius.circular(18),
      ),
      stroke,
    );

    // Smooth easing for organic motion
    final eased = Curves.easeInOutSine.transform(animationProgress);
    final pulse = (math.sin(eased * math.pi * 2) + 1) / 2;
    if (scene.isDuo) {
      _drawRobot(canvas, stroke, fill, centerX: 120, pulse: pulse, facingRight: true, scene: scene);
      _drawRobot(canvas, stroke, fill, centerX: 200, pulse: 1 - pulse, facingRight: false, scene: scene);
      if (scene == CompanionScene.kiss || scene == CompanionScene.shyLeanIn) {
        _drawHeart(canvas, fill, Offset(160, 56 - pulse * 6), scene == CompanionScene.kiss ? 12 : 9);
      }
    } else {
      _drawRobot(canvas, stroke, fill, centerX: 160, pulse: pulse, facingRight: true, scene: scene);
    }

    if (showScreenBoundary) {
      canvas.drawRect(Rect.fromLTWH(0, 0, 320, _visibleScreenRect.top), mask);
      canvas.drawRect(Rect.fromLTWH(0, _visibleScreenRect.bottom, 320, 240 - _visibleScreenRect.bottom), mask);
      canvas.drawRect(Rect.fromLTWH(0, _visibleScreenRect.top, _visibleScreenRect.left, _visibleScreenRect.height), mask);
      canvas.drawRect(Rect.fromLTWH(_visibleScreenRect.right, _visibleScreenRect.top, 320 - _visibleScreenRect.right, _visibleScreenRect.height), mask);
      canvas.drawRect(_visibleScreenRect, boundary);
    }

    canvas.restore();
  }

  void _drawRobot(
    Canvas canvas,
    Paint stroke,
    Paint fill, {
    required double centerX,
    required double pulse,
    required bool facingRight,
    required CompanionScene scene,
  }) {
    final direction = facingRight ? 1.0 : -1.0;
    final eased = Curves.easeInOutSine.transform(animationProgress);
    final hover = math.sin(eased * math.pi * 2 + (facingRight ? 0 : math.pi / 3)) * 4.2;
    final head = Rect.fromCenter(center: Offset(centerX, 68 + hover), width: 48, height: 39);
    final torso = Rect.fromCenter(center: Offset(centerX, 116 + hover * 0.5), width: 36, height: 42);
    final antennaTop = Offset(centerX, head.top - 15 - pulse * 4.5);
    final shoulderY = torso.top + 6;
    final handReach = scene == CompanionScene.holdHands ? 18 : scene == CompanionScene.wave ? 24 : 12;

    canvas.drawRRect(RRect.fromRectAndRadius(head, const Radius.circular(9)), stroke);
    canvas.drawRRect(RRect.fromRectAndRadius(torso, const Radius.circular(9)), stroke);
    canvas.drawLine(Offset(centerX, head.top), antennaTop, stroke);
    canvas.drawCircle(antennaTop, 4.5 + pulse * 2.4, stroke);
    canvas.drawLine(Offset(centerX - 12, torso.bottom), Offset(centerX - 18, torso.bottom + 27), stroke);
    canvas.drawLine(Offset(centerX + 12, torso.bottom), Offset(centerX + 18, torso.bottom + 27), stroke);
    canvas.drawLine(Offset(centerX - 18, shoulderY), Offset(centerX - 33, shoulderY + 12), stroke);
    canvas.drawLine(
      Offset(centerX + 18, shoulderY),
      Offset(centerX + 18 + handReach * direction, shoulderY - (scene == CompanionScene.wave ? 21 * pulse : 3)),
      stroke,
    );

    final eyeWidth = expression == 'angry' ? 16.5 : 12.0 + pulse * 2.4;
    canvas.drawRect(Rect.fromCenter(center: Offset(centerX - 12, head.center.dy), width: eyeWidth, height: 4.8), fill);
    canvas.drawRect(Rect.fromCenter(center: Offset(centerX + 12, head.center.dy), width: eyeWidth, height: 4.8), fill);
    canvas.drawLine(Offset(centerX - 12, head.center.dy + 12), Offset(centerX + 12, head.center.dy + 12), stroke);
  }

  void _drawHeart(Canvas canvas, Paint fill, Offset center, double size) {
    final path = Path()
      ..moveTo(center.dx, center.dy + size)
      ..cubicTo(center.dx - size * 1.3, center.dy + size * 0.3, center.dx - size * 1.2, center.dy - size * 0.9, center.dx, center.dy - size * 0.15)
      ..cubicTo(center.dx + size * 1.2, center.dy - size * 0.9, center.dx + size * 1.3, center.dy + size * 0.3, center.dx, center.dy + size);
    canvas.drawPath(path, fill);
  }

  @override
  bool shouldRepaint(covariant _RobotPainter oldDelegate) {
    return scene != oldDelegate.scene ||
        showScreenBoundary != oldDelegate.showScreenBoundary ||
        expression != oldDelegate.expression ||
        animationProgress != oldDelegate.animationProgress;
  }
}

class _CompanionFacePainter extends CustomPainter {
  const _CompanionFacePainter({
    required this.personality,
    required this.petMode,
    required this.referencePose,
    required this.showScreenBoundary,
    required this.expression,
    required this.hair,
    required this.ears,
    required this.mustache,
    required this.glasses,
    required this.headwear,
    required this.piercing,
    required this.hairSize,
    required this.mustacheSize,
    required this.hairWidth,
    required this.hairHeight,
    required this.hairThickness,
    required this.hairOffsetX,
    required this.hairOffsetY,
    required this.eyeOffsetX,
    required this.eyeOffsetY,
    required this.mouthOffsetX,
    required this.mouthOffsetY,
    required this.mustacheWidth,
    required this.mustacheHeight,
    required this.mustacheThickness,
    required this.mustacheOffsetX,
    required this.mustacheOffsetY,
    this.animationProgress = 0.0,
    this.eyeColor,
    this.faceColor,
    this.accentColor,
    this.bodyColor,
  });

  final String personality;
  final String petMode;
  final bool referencePose;
  final bool showScreenBoundary;
  final String? expression;
  final String hair;
  final String ears;
  final String mustache;
  final String glasses;
  final String headwear;
  final String piercing;
  final int hairSize;
  final int mustacheSize;
  final int hairWidth;
  final int hairHeight;
  final int hairThickness;
  final int hairOffsetX;
  final int hairOffsetY;
  final int eyeOffsetX;
  final int eyeOffsetY;
  final int mouthOffsetX;
  final int mouthOffsetY;
  final int mustacheWidth;
  final int mustacheHeight;
  final int mustacheThickness;
  final int mustacheOffsetX;
  final int mustacheOffsetY;
  final double animationProgress;
  final Color? eyeColor;
  final Color? faceColor;
  final Color? accentColor;
  final Color? bodyColor;

  Color get _resolvedAccentColor => accentColor ?? const Color(0xFF00BFFF);

  static const _visibleScreenRect = Rect.fromLTWH(2, 2, 316, 236);

  @override
  void paint(Canvas canvas, Size size) {
    canvas.save();
    canvas.scale(size.width / 320, size.height / 240);

    final resolvedFaceColor = faceColor ?? Colors.white;
    final resolvedEyeColor = eyeColor ?? Colors.white;

    final stroke = Paint()
      ..color = resolvedFaceColor
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round
      ..isAntiAlias = true;
    final fill = Paint()
      ..color = resolvedEyeColor
      ..style = PaintingStyle.fill
      ..isAntiAlias = true;
    final cut = Paint()
      ..color = Colors.black
      ..style = PaintingStyle.fill
      ..isAntiAlias = true;

    // --- animation helpers ---
    final double t = animationProgress; // 0..1 repeating
    final double breathY = math.sin(t * 2 * math.pi) * 2; // gentle vertical bob
    // Blink: eyes close briefly around t≈0.75 (a ~6% window)
    final bool blinking = (t > 0.72 && t < 0.78);
    // Pupil drift: slow figure-8 orbit
    final double driftDx = math.sin(t * 2 * math.pi) * 3;
    final double driftDy = math.sin(t * 4 * math.pi) * 1.5;

    canvas.drawRRect(
      RRect.fromRectAndRadius(
        const Rect.fromLTWH(0, 0, 319, 239),
        const Radius.circular(18),
      ),
      stroke,
    );

    final idleLeftX = 70.0 + _clampOffset(eyeOffsetX) * 2.0;
    final idleRightX = 170.0 + _clampOffset(eyeOffsetX) * 2.0;
    final idleEyeY = 100.0 + _clampOffset(eyeOffsetY) * 2.0 + breathY;
    final idleMouthCenterX = 160.0 + _clampOffset(mouthOffsetX) * 2.0;
    final idleMouthY = 150.0 + _clampOffset(mouthOffsetY) * 2.0 + breathY;
    final expressionLeftX = 68.0 + _clampOffset(eyeOffsetX) * 2.0;
    final expressionRightX = 172.0 + _clampOffset(eyeOffsetX) * 2.0;
    final expressionEyeY = 85.0 + _clampOffset(eyeOffsetY) * 2.0;
    final expressionMouthCenterX = 160.0 + _clampOffset(mouthOffsetX) * 2.0;
    final expressionMouthY = 140.0 + _clampOffset(mouthOffsetY) * 2.0;

    final normalizedExpression = expression?.trim();

    // Accessories use fixed base positions so they don't move with eye/mouth offsets.
    // Hair/mustache still move independently via their own offset controls.
    final accessoryBaseLeftX = normalizedExpression != null ? 68.0 : 70.0;
    final accessoryBaseRightX = normalizedExpression != null ? 172.0 : 170.0;
    final accessoryBaseEyeY = normalizedExpression != null ? 85.0 : (100.0 + breathY);
    final accessoryBaseMouthY = normalizedExpression != null ? 140.0 : (150.0 + breathY);
    final isOff = !referencePose && normalizedExpression == null && petMode == 'off';
    final isSleepy = !referencePose && normalizedExpression == null && (petMode == 'nap' || personality == 'sleepy');
    final isCuddly = !referencePose && normalizedExpression == null && (petMode == 'cuddle' || personality == 'cuddly');
    final isPlayful = !referencePose && normalizedExpression == null && (petMode == 'play' || personality == 'playful');
    final isParty = !referencePose && normalizedExpression == null && petMode == 'party';

    if (referencePose) {
      _drawEye(canvas, fill, cut, idleLeftX, idleEyeY, 44, 30, 9, 0, 0);
      _drawEye(canvas, fill, cut, idleRightX, idleEyeY, 44, 30, 9, 0, 0);
      _drawOvalMouth(canvas, stroke, idleMouthCenterX, idleMouthY, 9, 7);
    } else if (normalizedExpression != null) {
      _drawExpressionPreview(
        canvas,
        stroke,
        fill,
        cut,
        normalizedExpression,
        eyeY: expressionEyeY + breathY,
        mouthY: expressionMouthY + breathY,
        leftX: expressionLeftX,
        rightX: expressionRightX,
        mouthCenterX: expressionMouthCenterX,
        t: t,
        blinking: blinking,
        driftDx: driftDx,
        driftDy: driftDy,
      );
    } else if (isOff) {
      if (blinking) {
        _drawBlinkEye(canvas, fill, idleLeftX, idleEyeY, 40, 6, 7);
        _drawBlinkEye(canvas, fill, idleRightX, idleEyeY, 40, 6, 7);
      } else {
        _drawEye(canvas, fill, cut, idleLeftX, idleEyeY, 40, 26, 9, driftDx, driftDy);
        _drawEye(canvas, fill, cut, idleRightX, idleEyeY, 40, 26, 9, driftDx, driftDy);
      }
      canvas.drawLine(Offset(idleMouthCenterX - 17, idleMouthY), Offset(idleMouthCenterX + 18, idleMouthY), stroke);
    } else if (isSleepy) {
      // Sleepy Z floats up with animation
      final zOffset = t * 12;
      _drawBlinkEye(canvas, fill, idleLeftX, idleEyeY, 44, 8, 7);
      _drawBlinkEye(canvas, fill, idleRightX, idleEyeY, 44, 8, 7);
      canvas.drawLine(Offset(idleMouthCenterX - 20, idleMouthY), Offset(idleMouthCenterX + 20, idleMouthY), stroke);
      _drawZ(canvas, stroke, Offset(200, 86 - zOffset), 2.5);
      _drawZ(canvas, stroke, Offset(216, 68 - zOffset), 2.0);
    } else if (isCuddly) {
      if (blinking) {
        _drawBlinkEye(canvas, fill, idleLeftX, idleEyeY, 44, 6, 7);
        _drawBlinkEye(canvas, fill, idleRightX, idleEyeY, 44, 6, 7);
      } else {
        _drawEye(canvas, fill, cut, idleLeftX, idleEyeY, 44, 30, 9, driftDx, driftDy);
        _drawEye(canvas, fill, cut, idleRightX, idleEyeY, 44, 30, 9, driftDx, driftDy);
      }
      _drawSmile(canvas, stroke, idleMouthCenterX, idleMouthY - 2, 44);
      // Heart pulses gently
      final heartScale = 6.0 + math.sin(t * 2 * math.pi) * 1.5;
      _drawHeart(canvas, fill, Offset(idleMouthCenterX, 206 + breathY), heartScale);
    } else if (isPlayful) {
      if (blinking) {
        _drawBlinkEye(canvas, fill, idleLeftX, idleEyeY, 48, 6, 7);
        _drawBlinkEye(canvas, fill, idleRightX, idleEyeY, 48, 6, 7);
      } else {
        _drawEye(canvas, fill, cut, idleLeftX, idleEyeY, 48, 34, 9, 6 + driftDx, driftDy);
        _drawEye(canvas, fill, cut, idleRightX, idleEyeY, 48, 34, 9, 6 + driftDx, driftDy);
      }
      _drawSmile(canvas, stroke, idleMouthCenterX, idleMouthY - 4, 48);
      // Ball bounces
      final bounceY = 184 - (math.sin(t * math.pi).abs()) * 20;
      canvas.drawCircle(Offset(45, bounceY), 4, fill);
    } else if (isParty) {
      if (blinking) {
        _drawBlinkEye(canvas, fill, idleLeftX, idleEyeY, 48, 6, 7);
        _drawBlinkEye(canvas, fill, idleRightX, idleEyeY, 48, 6, 7);
      } else {
        _drawEye(canvas, fill, cut, idleLeftX, idleEyeY, 48, 34, 9, 4 + driftDx, driftDy);
        _drawEye(canvas, fill, cut, idleRightX, idleEyeY, 48, 34, 9, -4 + driftDx, driftDy);
      }
      _drawSmile(canvas, stroke, idleMouthCenterX, idleMouthY - 4, 52);
      // Stars rotate around their positions
      final starR = 18.0;
      _drawStar(canvas, stroke, Offset(40 + math.cos(t * 2 * math.pi) * starR, 56 + math.sin(t * 2 * math.pi) * starR), 6);
      _drawStar(canvas, stroke, Offset(280 + math.cos(t * 2 * math.pi + math.pi) * starR, 64 + math.sin(t * 2 * math.pi + math.pi) * starR), 6);
    } else {
      // Default curious
      if (blinking) {
        _drawBlinkEye(canvas, fill, idleLeftX, idleEyeY, 44, 6, 7);
        _drawBlinkEye(canvas, fill, idleRightX, idleEyeY, 44, 6, 7);
      } else {
        _drawEye(canvas, fill, cut, idleLeftX, idleEyeY, 44, 30, 9, 4 + driftDx, 2 + driftDy);
        _drawEye(canvas, fill, cut, idleRightX, idleEyeY, 44, 30, 9, -2 + driftDx, -2 + driftDy);
      }
      _drawOvalMouth(canvas, stroke, idleMouthCenterX, idleMouthY, 9, 7);
    }

    _drawAccessories(
      canvas,
      stroke,
      fill,
      accessoryBaseLeftX,
      accessoryBaseRightX,
      accessoryBaseEyeY,
      accessoryBaseMouthY,
    );

    if (showScreenBoundary) {
      _drawScreenBoundary(canvas);
    }

    canvas.restore();
  }

  int _clampPercent(int value) => value.clamp(10, 400);

  int _clampOffset(int value) => value.clamp(-60, 60);

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
    if (clampedH >= 18) {
      canvas.drawCircle(Offset(cx + pupilDx, cy + pupilDy), 9, cut);
    } else if (clampedH >= 11) {
      canvas.drawCircle(Offset(cx + pupilDx, cy + pupilDy), 5, cut);
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
    for (var t = 0; t < 5; t++) {
      canvas.drawLine(Offset(cx - hw, cy + t), Offset(cx - hw / 3, cy + 13 + t), stroke);
      canvas.drawLine(Offset(cx - hw / 3, cy + 13 + t), Offset(cx + hw / 3, cy + 13 + t), stroke);
      canvas.drawLine(Offset(cx + hw / 3, cy + 13 + t), Offset(cx + hw, cy + t), stroke);
    }
  }

  void _drawOvalMouth(Canvas canvas, Paint stroke, double cx, double cy, double rw, double rh) {
    for (var t = 0; t < 4; t++) {
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
      ..moveTo(center.dx - s * 2, center.dy - s / 3 + 2)
      ..lineTo(center.dx + s * 2, center.dy - s / 3 + 2)
      ..lineTo(center.dx, center.dy + s * 2)
      ..close();
    canvas.drawPath(path, fill);
  }

  void _drawHappyArc(Canvas canvas, Paint stroke, double cx, double cy, double width) {
    final hw = width / 2;
    for (var t = 0; t < 5; t++) {
      canvas.drawLine(Offset(cx - hw, cy + 10 + t), Offset(cx, cy - 10 + t), stroke);
      canvas.drawLine(Offset(cx, cy - 10 + t), Offset(cx + hw, cy + 10 + t), stroke);
    }
  }

  void _drawSadArc(Canvas canvas, Paint stroke, double cx, double cy, double width) {
    final hw = width / 2;
    for (var t = 0; t < 5; t++) {
      canvas.drawLine(Offset(cx - hw, cy - 6 + t), Offset(cx, cy + 10 + t), stroke);
      canvas.drawLine(Offset(cx, cy + 10 + t), Offset(cx + hw, cy - 6 + t), stroke);
    }
  }

  void _drawKissLips(Canvas canvas, Paint fill, Paint cut, double cx, double cy) {
    canvas.drawCircle(Offset(cx, cy), 11, fill);
    canvas.drawCircle(Offset(cx, cy), 6, cut);
  }

  void _drawTear(Canvas canvas, Paint fill, Offset center, double size) {
    canvas.drawCircle(Offset(center.dx, center.dy + size), size, fill);
    final path = Path()
      ..moveTo(center.dx - size, center.dy + size)
      ..lineTo(center.dx + size, center.dy + size)
      ..lineTo(center.dx, center.dy)
      ..close();
    canvas.drawPath(path, fill);
  }

  void _drawBrow(Canvas canvas, Paint stroke, Offset start, Offset end) {
    for (var t = 0; t < 5; t++) {
      canvas.drawLine(start.translate(0, t.toDouble()), end.translate(0, t.toDouble()), stroke);
    }
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
    final width = 12 * scale;
    final height = 12 * scale;
    canvas.drawLine(origin, Offset(origin.dx + width, origin.dy), stroke);
    canvas.drawLine(Offset(origin.dx + width, origin.dy), Offset(origin.dx, origin.dy + height), stroke);
    canvas.drawLine(Offset(origin.dx, origin.dy + height), Offset(origin.dx + width, origin.dy + height), stroke);
  }

  void _drawAccessories(Canvas canvas, Paint stroke, Paint fill, double leftX, double rightX, double eyeY, double mouthY) {
    final faceCenterX = (leftX + rightX) / 2;
    final scaledHair = _clampPercent(hairSize);
    final scaledMustache = _clampPercent(mustacheSize);
    final scaledHairWidth = _clampPercent(hairWidth);
    final scaledHairHeight = _clampPercent(hairHeight);
    final scaledHairThickness = _clampPercent(hairThickness);
    final hairStroke = 1 + ((2 * scaledHairThickness) / 100).floor();
    final hairCenterX = faceCenterX + _clampOffset(hairOffsetX);
    final hairCenterY = eyeY + _clampOffset(hairOffsetY);
    final scaledMustacheWidth = _clampPercent(mustacheWidth);
    final scaledMustacheHeight = _clampPercent(mustacheHeight);
    final scaledMustacheThickness = _clampPercent(mustacheThickness);
    final mustacheStroke = 1 + ((2 * scaledMustacheThickness) / 100).floor();
    final mustacheCenterX = faceCenterX + _clampOffset(mustacheOffsetX);
    final mustacheCenterY = mouthY + _clampOffset(mustacheOffsetY);

    if (ears == 'cat') {
      _triangle(canvas, stroke, Offset(leftX - 40, eyeY - 34), Offset(leftX - 20, eyeY - 56), Offset(leftX + 5, eyeY - 34));
      _triangle(canvas, stroke, Offset(rightX - 5, eyeY - 34), Offset(rightX + 20, eyeY - 56), Offset(rightX + 40, eyeY - 34));
    } else if (ears == 'bear') {
      canvas.drawCircle(Offset(leftX - 30, eyeY - 38), 11, stroke);
      canvas.drawCircle(Offset(rightX + 30, eyeY - 38), 11, stroke);
    } else if (ears == 'bunny') {
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(leftX - 38, eyeY - 64, 15, 30), const Radius.circular(7)),
        stroke,
      );
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(rightX + 18, eyeY - 64, 15, 30), const Radius.circular(7)),
        stroke,
      );
    }

    if (hair == 'tuft') {
      final lift = _scaleValue(22, _clampPercent((scaledHair * scaledHairHeight / 100).round()));
      final spread = _scaleValue(10, _clampPercent((scaledHair * scaledHairWidth / 100).round()));
      for (var strokeIndex = 0; strokeIndex < hairStroke; strokeIndex++) {
        canvas.drawLine(
          Offset(hairCenterX - spread, hairCenterY - 22 + strokeIndex),
          Offset(hairCenterX, hairCenterY - 22 - lift + strokeIndex),
          stroke,
        );
        canvas.drawLine(
          Offset(hairCenterX, hairCenterY - 22 - lift + strokeIndex),
          Offset(hairCenterX + spread, hairCenterY - 22 + strokeIndex),
          stroke,
        );
        canvas.drawLine(
          Offset(hairCenterX, hairCenterY - 22 - lift + strokeIndex),
          Offset(hairCenterX + 2, hairCenterY - 15 + strokeIndex),
          stroke,
        );
      }
    } else if (hair == 'bangs') {
      final topY = hairCenterY - 22 - _scaleValue(6, _clampPercent((scaledHair * scaledHairHeight / 100).round()));
      final leftEdge = hairCenterX - _scaleValue(45, scaledHairWidth);
      final rightEdge = hairCenterX + _scaleValue(45, scaledHairWidth);
      for (var strokeIndex = 0; strokeIndex < hairStroke; strokeIndex++) {
        canvas.drawLine(Offset(leftEdge, topY + strokeIndex), Offset(rightEdge, topY + strokeIndex), stroke);
      }
      for (double x = leftX - 35; x <= rightX + 35; x += 20) {
        final shiftedX = hairCenterX + (x - faceCenterX);
        for (var strokeIndex = 0; strokeIndex < hairStroke; strokeIndex++) {
          canvas.drawLine(
            Offset(shiftedX, topY + 2 + strokeIndex),
            Offset(shiftedX + _scaleValue(6, scaledHairWidth), hairCenterY - 13 + strokeIndex),
            stroke,
          );
        }
      }
    } else if (hair == 'spiky') {
      for (double x = leftX - 35; x <= rightX + 35; x += 25) {
        final shiftedX = hairCenterX + (x - faceCenterX);
        final peak = hairCenterY - 19 - _scaleValue(17, _clampPercent((scaledHair * scaledHairHeight / 100).round()));
        for (var strokeIndex = 0; strokeIndex < hairStroke; strokeIndex++) {
          canvas.drawLine(
            Offset(shiftedX, hairCenterY - 19 + strokeIndex),
            Offset(shiftedX + _scaleValue(10, scaledHairWidth), peak + strokeIndex),
            stroke,
          );
          canvas.drawLine(
            Offset(shiftedX + _scaleValue(10, scaledHairWidth), peak + strokeIndex),
            Offset(shiftedX + _scaleValue(20, scaledHairWidth), hairCenterY - 19 + strokeIndex),
            stroke,
          );
        }
      }
    } else if (hair == 'swoop') {
      final topY = hairCenterY - 22 - _scaleValue(11, _clampPercent((scaledHair * scaledHairHeight / 100).round()));
      for (var strokeIndex = 0; strokeIndex < hairStroke; strokeIndex++) {
        canvas.drawLine(
          Offset(hairCenterX - _scaleValue(45, scaledHairWidth), hairCenterY - 17 + strokeIndex),
          Offset(hairCenterX + _scaleValue(30, scaledHairWidth), topY + strokeIndex),
          stroke,
        );
        canvas.drawLine(
          Offset(hairCenterX + _scaleValue(30, scaledHairWidth), topY + strokeIndex),
          Offset(hairCenterX + _scaleValue(70, scaledHairWidth), hairCenterY - 9 + strokeIndex),
          stroke,
        );
        canvas.drawLine(
          Offset(hairCenterX - _scaleValue(25, scaledHairWidth), hairCenterY - 19 + strokeIndex),
          Offset(hairCenterX + _scaleValue(10, scaledHairWidth), topY + 4 + strokeIndex),
          stroke,
        );
      }
    } else if (hair == 'bob') {
      final topY = hairCenterY - 21 - _scaleValue(8, _clampPercent((scaledHair * scaledHairHeight / 100).round()));
      final width = _scaleValue((rightX - leftX) + 90, scaledHairWidth);
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          Rect.fromLTWH(hairCenterX - width / 2, topY, width, 19 + _scaleValue(8, scaledHairHeight)),
          const Radius.circular(9),
        ),
        stroke,
      );
      for (var strokeIndex = 0; strokeIndex < hairStroke; strokeIndex++) {
        canvas.drawLine(
          Offset(hairCenterX - width / 2, hairCenterY - 2 + strokeIndex),
          Offset(hairCenterX - width / 2 + _scaleValue(15, scaledHairWidth), hairCenterY + 13 + strokeIndex),
          stroke,
        );
        canvas.drawLine(
          Offset(hairCenterX + width / 2, hairCenterY - 2 + strokeIndex),
          Offset(hairCenterX + width / 2 - _scaleValue(15, scaledHairWidth), hairCenterY + 13 + strokeIndex),
          stroke,
        );
      }
    } else if (hair == 'messy') {
      for (double x = leftX - 25; x <= rightX + 30; x += 22) {
        final shiftedX = hairCenterX + (x - faceCenterX);
        final peak = hairCenterY - 15 - _scaleValue(13, _clampPercent((scaledHair * scaledHairHeight / 100).round())) + (((x ~/ 22) % 2 == 0) ? 0 : 6);
        for (var strokeIndex = 0; strokeIndex < hairStroke; strokeIndex++) {
          canvas.drawLine(
            Offset(shiftedX, hairCenterY - 15 + strokeIndex),
            Offset(shiftedX + _scaleValue(6, scaledHairWidth), peak + strokeIndex),
            stroke,
          );
          canvas.drawLine(
            Offset(shiftedX + _scaleValue(6, scaledHairWidth), peak + strokeIndex),
            Offset(shiftedX + _scaleValue(15, scaledHairWidth), hairCenterY - 17 + strokeIndex),
            stroke,
          );
        }
      }
    } else if (hair == 'ponytail') {
      final topY = hairCenterY - 22 - _scaleValue(6, _clampPercent((scaledHair * scaledHairHeight / 100).round()));
      final leftEdge = hairCenterX - _scaleValue(45, scaledHairWidth);
      final rightEdge = hairCenterX + _scaleValue(45, scaledHairWidth);
      for (var strokeIndex = 0; strokeIndex < hairStroke; strokeIndex++) {
        canvas.drawLine(Offset(leftEdge, topY + strokeIndex), Offset(rightEdge, topY + strokeIndex), stroke);
      }
      // Ponytail tail going right and down
      for (var strokeIndex = 0; strokeIndex < hairStroke; strokeIndex++) {
        canvas.drawLine(Offset(rightEdge, topY + strokeIndex), Offset(rightEdge + _scaleValue(20, scaledHairWidth), topY + 10 + strokeIndex), stroke);
        canvas.drawLine(Offset(rightEdge + _scaleValue(20, scaledHairWidth), topY + 10 + strokeIndex), Offset(rightEdge + _scaleValue(15, scaledHairWidth), topY + 35 + strokeIndex), stroke);
      }
      // Band
      canvas.drawCircle(Offset(rightEdge, topY + 2), 3, stroke);
    } else if (hair == 'curly') {
      for (double angle = 0; angle < 5; angle++) {
        final offsetAngle = angle * 1.1;
        final cx = hairCenterX - _scaleValue(30, scaledHairWidth) + angle * _scaleValue(15, scaledHairWidth);
        final cy = hairCenterY - 22 - _scaleValue(6, _clampPercent((scaledHair * scaledHairHeight / 100).round())) + (angle.toInt().isOdd ? 3.0 : 0.0);
        canvas.drawCircle(Offset(cx, cy), _scaleValue(8, scaledHairHeight) + offsetAngle, stroke);
      }
    } else if (hair == 'pigtails') {
      final topY = hairCenterY - 20;
      final leftPigX = hairCenterX - _scaleValue(55, scaledHairWidth);
      final rightPigX = hairCenterX + _scaleValue(55, scaledHairWidth);
      // bands
      canvas.drawCircle(Offset(leftPigX + 10, topY - 6), 4, stroke);
      canvas.drawCircle(Offset(rightPigX - 10, topY - 6), 4, stroke);
      // pigtails hanging down
      for (var strokeIndex = 0; strokeIndex < hairStroke; strokeIndex++) {
        canvas.drawLine(Offset(leftPigX + 10, topY - 2 + strokeIndex), Offset(leftPigX, topY + _scaleValue(25, scaledHairHeight) + strokeIndex), stroke);
        canvas.drawLine(Offset(rightPigX - 10, topY - 2 + strokeIndex), Offset(rightPigX, topY + _scaleValue(25, scaledHairHeight) + strokeIndex), stroke);
      }
    } else if (hair == 'mohawk') {
      final height = _scaleValue(28, _clampPercent((scaledHair * scaledHairHeight / 100).round()));
      for (double x = hairCenterX - _scaleValue(15, scaledHairWidth); x <= hairCenterX + _scaleValue(15, scaledHairWidth); x += 8) {
        for (var strokeIndex = 0; strokeIndex < hairStroke; strokeIndex++) {
          canvas.drawLine(
            Offset(x, hairCenterY - 18 + strokeIndex),
            Offset(x + 2, hairCenterY - 18 - height + strokeIndex),
            stroke,
          );
        }
      }
    }

    if (headwear == 'bow') {
      _triangle(canvas, stroke, Offset(faceCenterX - 10, eyeY - 45), Offset(faceCenterX - 40, eyeY - 34), Offset(faceCenterX - 20, eyeY - 22));
      _triangle(canvas, stroke, Offset(faceCenterX + 10, eyeY - 45), Offset(faceCenterX + 40, eyeY - 34), Offset(faceCenterX + 20, eyeY - 22));
      canvas.drawCircle(Offset(faceCenterX, eyeY - 34), 4, stroke);
    } else if (headwear == 'beanie') {
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(faceCenterX - 60, eyeY - 52, 120, 22), const Radius.circular(9)),
        stroke,
      );
      canvas.drawLine(Offset(faceCenterX - 50, eyeY - 28), Offset(faceCenterX + 50, eyeY - 28), stroke);
      canvas.drawCircle(Offset(faceCenterX, eyeY - 56), 6, stroke);
    } else if (headwear == 'crown') {
      final crown = Path()
        ..moveTo(faceCenterX - 50, eyeY - 34)
        ..lineTo(faceCenterX - 30, eyeY - 56)
        ..lineTo(faceCenterX - 5, eyeY - 34)
        ..lineTo(faceCenterX + 15, eyeY - 60)
        ..lineTo(faceCenterX + 35, eyeY - 34)
        ..lineTo(faceCenterX + 50, eyeY - 52);
      canvas.drawPath(crown, stroke);
      canvas.drawLine(Offset(faceCenterX - 50, eyeY - 34), Offset(faceCenterX + 50, eyeY - 34), stroke);
    } else if (headwear == 'top_hat') {
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(faceCenterX - 30, eyeY - 80, 60, 48), const Radius.circular(4)),
        stroke,
      );
      canvas.drawLine(Offset(faceCenterX - 50, eyeY - 32), Offset(faceCenterX + 50, eyeY - 32), stroke);
    } else if (headwear == 'halo') {
      canvas.drawOval(
        Rect.fromCenter(center: Offset(faceCenterX, eyeY - 52), width: 80, height: 16),
        stroke,
      );
    } else if (headwear == 'flower_crown') {
      for (var i = 0; i < 5; i++) {
        final fx = faceCenterX - 40 + i * 20.0;
        final fy = eyeY - 38 + (i.isOdd ? 4.0 : 0.0);
        canvas.drawCircle(Offset(fx, fy), 6, stroke);
        canvas.drawCircle(Offset(fx, fy), 2, fill);
      }
    } else if (headwear == 'beret') {
      canvas.drawOval(
        Rect.fromCenter(center: Offset(faceCenterX - 10, eyeY - 42), width: 90, height: 28),
        stroke,
      );
      canvas.drawCircle(Offset(faceCenterX - 10, eyeY - 56), 4, fill);
    }

    if (glasses == 'round') {
      canvas.drawCircle(Offset(leftX, eyeY), 22, stroke);
      canvas.drawCircle(Offset(rightX, eyeY), 22, stroke);
      canvas.drawLine(Offset(leftX + 22, eyeY), Offset(rightX - 22, eyeY), stroke);
    } else if (glasses == 'square') {
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(leftX - 26, eyeY - 20, 52, 40), const Radius.circular(7)),
        stroke,
      );
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(rightX - 26, eyeY - 20, 52, 40), const Radius.circular(7)),
        stroke,
      );
      canvas.drawLine(Offset(leftX + 26, eyeY), Offset(rightX - 26, eyeY), stroke);
    } else if (glasses == 'visor') {
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(leftX - 45, eyeY - 22, (rightX - leftX) + 90, 38), const Radius.circular(11)),
        stroke,
      );
    }

    if (mustache == 'classic') {
      final wing = _scaleValue(22, _clampPercent((scaledMustache * scaledMustacheWidth / 100).round()));
      final inner = _scaleValue(8, _clampPercent((scaledMustache * scaledMustacheWidth / 100).round()));
      final rise = _scaleValue(8, _clampPercent((scaledMustache * scaledMustacheHeight / 100).round()));
      for (var strokeIndex = 0; strokeIndex < mustacheStroke; strokeIndex++) {
        canvas.drawLine(Offset(mustacheCenterX - wing, mustacheCenterY - rise + strokeIndex), Offset(mustacheCenterX - inner, mustacheCenterY - 2 + strokeIndex), stroke);
        canvas.drawLine(Offset(mustacheCenterX - wing, mustacheCenterY - rise + 2 + strokeIndex), Offset(mustacheCenterX - inner, mustacheCenterY + 2 + strokeIndex), stroke);
        canvas.drawLine(Offset(mustacheCenterX + inner, mustacheCenterY - 2 + strokeIndex), Offset(mustacheCenterX + wing, mustacheCenterY - rise + strokeIndex), stroke);
        canvas.drawLine(Offset(mustacheCenterX + inner, mustacheCenterY + 2 + strokeIndex), Offset(mustacheCenterX + wing, mustacheCenterY - rise + 2 + strokeIndex), stroke);
      }
    } else if (mustache == 'curled') {
      final wing = _scaleValue(22, _clampPercent((scaledMustache * scaledMustacheWidth / 100).round()));
      final rise = _scaleValue(4, _clampPercent((scaledMustache * scaledMustacheHeight / 100).round()));
      for (var strokeIndex = 0; strokeIndex < mustacheStroke; strokeIndex++) {
        canvas.drawLine(Offset(mustacheCenterX - wing, mustacheCenterY - rise + strokeIndex), Offset(mustacheCenterX - 4, mustacheCenterY - 2 + strokeIndex), stroke);
        canvas.drawLine(Offset(mustacheCenterX + 4, mustacheCenterY - 2 + strokeIndex), Offset(mustacheCenterX + wing, mustacheCenterY - rise + strokeIndex), stroke);
      }
      canvas.drawCircle(Offset(mustacheCenterX - wing - 4, mustacheCenterY - rise - 2), 4, stroke);
      canvas.drawCircle(Offset(mustacheCenterX + wing + 4, mustacheCenterY - rise - 2), 4, stroke);
    } else if (mustache == 'handlebar') {
      final wing = _scaleValue(26, _clampPercent((scaledMustache * scaledMustacheWidth / 100).round()));
      final curl = _scaleValue(10, _clampPercent((scaledMustache * scaledMustacheHeight / 100).round()));
      final curlWidth = _scaleValue(8, scaledMustacheWidth);
      for (var strokeIndex = 0; strokeIndex < mustacheStroke; strokeIndex++) {
        canvas.drawLine(Offset(mustacheCenterX - wing, mustacheCenterY - 2 + strokeIndex), Offset(mustacheCenterX - 4, mustacheCenterY + strokeIndex), stroke);
        canvas.drawLine(Offset(mustacheCenterX + 4, mustacheCenterY + strokeIndex), Offset(mustacheCenterX + wing, mustacheCenterY - 2 + strokeIndex), stroke);
        canvas.drawLine(Offset(mustacheCenterX - wing, mustacheCenterY - 2 + strokeIndex), Offset(mustacheCenterX - wing - curlWidth, mustacheCenterY - curl + strokeIndex), stroke);
        canvas.drawLine(Offset(mustacheCenterX + wing, mustacheCenterY - 2 + strokeIndex), Offset(mustacheCenterX + wing + curlWidth, mustacheCenterY - curl + strokeIndex), stroke);
      }
    } else if (mustache == 'walrus') {
      final width = _scaleValue(26, _clampPercent((scaledMustache * scaledMustacheWidth / 100).round()));
      final height = _scaleValue(8, _clampPercent((scaledMustache * scaledMustacheHeight / 100).round()));
      final bridgeWidth = _scaleValue(8, scaledMustacheThickness);
      canvas.drawRRect(
        RRect.fromRectAndRadius(Rect.fromLTWH(mustacheCenterX - width, mustacheCenterY - 11, width * 2, height + 4), const Radius.circular(5)),
        fill,
      );
      canvas.drawRect(Rect.fromLTWH(mustacheCenterX - bridgeWidth / 2, mustacheCenterY - 6, bridgeWidth, height + 8), fill);
    } else if (mustache == 'pencil') {
      final width = _scaleValue(24, _clampPercent((scaledMustache * scaledMustacheWidth / 100).round()));
      final inset = _scaleValue(4, scaledMustacheThickness);
      for (var strokeIndex = 0; strokeIndex < mustacheStroke; strokeIndex++) {
        canvas.drawLine(Offset(mustacheCenterX - width, mustacheCenterY - 4 + strokeIndex), Offset(mustacheCenterX + width, mustacheCenterY - 4 + strokeIndex), stroke);
        canvas.drawLine(Offset(mustacheCenterX - width + inset, mustacheCenterY - 2 + strokeIndex), Offset(mustacheCenterX + width - inset, mustacheCenterY - 2 + strokeIndex), stroke);
      }
    } else if (mustache == 'imperial') {
      final wing = _scaleValue(22, _clampPercent((scaledMustache * scaledMustacheWidth / 100).round()));
      final rise = _scaleValue(17, _clampPercent((scaledMustache * scaledMustacheHeight / 100).round()));
      final flare = _scaleValue(4, scaledMustacheWidth);
      for (var strokeIndex = 0; strokeIndex < mustacheStroke; strokeIndex++) {
        canvas.drawLine(Offset(mustacheCenterX - wing, mustacheCenterY - 4 + strokeIndex), Offset(mustacheCenterX - 2, mustacheCenterY - 2 + strokeIndex), stroke);
        canvas.drawLine(Offset(mustacheCenterX + 2, mustacheCenterY - 2 + strokeIndex), Offset(mustacheCenterX + wing, mustacheCenterY - 4 + strokeIndex), stroke);
        canvas.drawLine(Offset(mustacheCenterX - wing, mustacheCenterY - 4 + strokeIndex), Offset(mustacheCenterX - wing - flare, mustacheCenterY - rise + strokeIndex), stroke);
        canvas.drawLine(Offset(mustacheCenterX + wing, mustacheCenterY - 4 + strokeIndex), Offset(mustacheCenterX + wing + flare, mustacheCenterY - rise + strokeIndex), stroke);
      }
    } else if (mustache == 'goatee') {
      final gWidth = _scaleValue(14, _clampPercent((scaledMustache * scaledMustacheWidth / 100).round()));
      final gHeight = _scaleValue(20, _clampPercent((scaledMustache * scaledMustacheHeight / 100).round()));
      final path = Path()
        ..moveTo(mustacheCenterX - gWidth, mustacheCenterY + 2)
        ..quadraticBezierTo(mustacheCenterX, mustacheCenterY + gHeight + 6, mustacheCenterX + gWidth, mustacheCenterY + 2);
      for (var strokeIndex = 0; strokeIndex < mustacheStroke; strokeIndex++) {
        canvas.drawPath(path.shift(Offset(0, strokeIndex.toDouble())), stroke);
      }
    } else if (mustache == 'soul_patch') {
      final pWidth = _scaleValue(6, _clampPercent((scaledMustache * scaledMustacheWidth / 100).round()));
      final pHeight = _scaleValue(10, _clampPercent((scaledMustache * scaledMustacheHeight / 100).round()));
      canvas.drawRRect(
        RRect.fromRectAndRadius(
          Rect.fromCenter(center: Offset(mustacheCenterX, mustacheCenterY + 6), width: pWidth.toDouble(), height: pHeight.toDouble()),
          const Radius.circular(3),
        ),
        stroke,
      );
    }

    if (piercing == 'brow') {
      canvas.drawLine(Offset(rightX + 15, eyeY - 26), Offset(rightX + 35, eyeY - 22), stroke);
    } else if (piercing == 'nose') {
      canvas.drawCircle(Offset(faceCenterX + 8, mouthY - 19), 4, stroke);
    } else if (piercing == 'lip') {
      canvas.drawCircle(Offset(faceCenterX + 15, mouthY + 4), 4, stroke);
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

  void _drawExpressionPreview(
    Canvas canvas,
    Paint stroke,
    Paint fill,
    Paint cut,
    String expression, {
    required double eyeY,
    required double mouthY,
    double leftX = 68.0,
    double rightX = 172.0,
    double mouthCenterX = 160.0,
    double t = 0.0,
    bool blinking = false,
    double driftDx = 0.0,
    double driftDy = 0.0,
  }
  ) {
    const eyeWidth = 52.0;
    const eyeHeight = 40.0;
    const eyeRadius = 13.0;
    final eyeShift = eyeY - 85.0;
    final mouthShift = mouthY - 140.0;
    final eyeDx = leftX - 68.0;
    final mouthDx = mouthCenterX - 160.0;

    // Helper: open eye with blink + drift support
    void eye(double cx, double cy, double w, double h, double r, double pdx, double pdy) {
      if (blinking) {
        _drawBlinkEye(canvas, fill, cx, cy, w, 6, r);
      } else {
        _drawEye(canvas, fill, cut, cx, cy, w, h, r, pdx + driftDx, pdy + driftDy);
      }
    }

    switch (expression) {
      case 'heart':
        final pulse = 19.0 + math.sin(t * 2 * math.pi) * 2;
        _drawHeart(canvas, fill, Offset(mouthCenterX, 112 + eyeShift), pulse);
        break;
      case 'love':
        eye(leftX, eyeY, eyeWidth, eyeHeight, eyeRadius, 0, 0);
        eye(rightX, eyeY, eyeWidth, eyeHeight, eyeRadius, 0, 0);
        _drawHeart(canvas, fill, Offset(leftX, eyeY), 8);
        _drawHeart(canvas, fill, Offset(rightX, eyeY), 8);
        _drawSmile(canvas, stroke, mouthCenterX, mouthY - 4, 52);
        _drawHeart(canvas, fill, Offset(60 + eyeDx, 82 + eyeShift), 6);
        _drawHeart(canvas, fill, Offset(260 + eyeDx, 68 + eyeShift), 4);
        break;
      case 'surprised':
        eye(leftX, eyeY, eyeWidth + 8, eyeHeight + 15, eyeRadius + 4, 0, 0);
        eye(rightX, eyeY, eyeWidth + 8, eyeHeight + 15, eyeRadius + 4, 0, 0);
        _drawBrow(canvas, stroke, Offset(55 + eyeDx, 26 + eyeShift), Offset(125 + eyeDx, 26 + eyeShift));
        _drawBrow(canvas, stroke, Offset(195 + eyeDx, 26 + eyeShift), Offset(265 + eyeDx, 26 + eyeShift));
        _drawOvalMouth(canvas, stroke, mouthCenterX, mouthY, 11, 9);
        break;
      case 'angry':
        eye(leftX, eyeY + 4, eyeWidth, eyeHeight - 4, eyeRadius, 0, 4);
        eye(rightX, eyeY + 4, eyeWidth, eyeHeight - 4, eyeRadius, 0, 4);
        canvas.drawRect(Rect.fromLTWH(55 + eyeDx, 56 + eyeShift, 52, 8), cut);
        canvas.drawRect(Rect.fromLTWH(195 + eyeDx, 56 + eyeShift, 52, 8), cut);
        _drawBrow(canvas, stroke, Offset(55 + eyeDx, 30 + eyeShift), Offset(110 + eyeDx, 68 + eyeShift));
        _drawBrow(canvas, stroke, Offset(265 + eyeDx, 30 + eyeShift), Offset(210 + eyeDx, 68 + eyeShift));
        for (var line = 0; line < 5; line++) {
          canvas.drawLine(Offset(135 + mouthDx, mouthY + line), Offset(185 + mouthDx, mouthY + line), stroke);
        }
        break;
      case 'sad':
        eye(leftX, eyeY + 6, eyeWidth, eyeHeight - 8, eyeRadius, 0, 8);
        eye(rightX, eyeY + 6, eyeWidth, eyeHeight - 8, eyeRadius, 0, 8);
        _drawBrow(canvas, stroke, Offset(55 + eyeDx, 68 + eyeShift), Offset(115 + eyeDx, 30 + eyeShift));
        _drawBrow(canvas, stroke, Offset(265 + eyeDx, 68 + eyeShift), Offset(205 + eyeDx, 30 + eyeShift));
        _drawSadArc(canvas, stroke, mouthCenterX, mouthY - 4, 30);
        _drawTear(canvas, fill, Offset(130 + eyeDx, 135 + eyeShift), 6);
        break;
      case 'sleepy':
        _drawBlinkEye(canvas, fill, leftX, eyeY, eyeWidth, 9, eyeRadius);
        _drawBlinkEye(canvas, fill, rightX, eyeY, eyeWidth, 9, eyeRadius);
        canvas.drawLine(Offset(140 + mouthDx, mouthY), Offset(180 + mouthDx, mouthY), stroke);
        final zOff = t * 12;
        _drawZ(canvas, stroke, Offset(250 + eyeDx, 45 + eyeShift - zOff), 1.0);
        _drawZ(canvas, stroke, Offset(270 + eyeDx, 68 + eyeShift - zOff), 0.8);
        break;
      case 'thinking':
        eye(leftX, eyeY, eyeWidth - 11, 22, eyeRadius, 9, 0);
        eye(rightX, eyeY, eyeWidth, eyeHeight, eyeRadius, 9, 0);
        final bobble = math.sin(t * 2 * math.pi) * 3;
        canvas.drawCircle(Offset(270 + eyeDx, 143 + eyeShift + bobble), 4, fill);
        canvas.drawCircle(Offset(285 + eyeDx, 105 + eyeShift + bobble), 6, fill);
        canvas.drawCircle(Offset(295 + eyeDx, 60 + eyeShift + bobble), 8, fill);
        canvas.drawLine(Offset(140 + mouthDx, mouthY), Offset(175 + mouthDx, mouthY - 4), stroke);
        canvas.drawLine(Offset(140 + mouthDx, mouthY + 2), Offset(175 + mouthDx, mouthY - 2), stroke);
        break;
      case 'smile':
        _drawHappyArc(canvas, stroke, leftX, eyeY, eyeWidth);
        _drawHappyArc(canvas, stroke, rightX, eyeY, eyeWidth);
        _drawSmile(canvas, stroke, mouthCenterX, mouthY - 4, 44);
        break;
      case 'confused':
        eye(leftX, eyeY - 4, eyeWidth, eyeHeight + 4, eyeRadius, -4, 0);
        eye(rightX, eyeY + 6, eyeWidth - 11, eyeHeight - 11, eyeRadius - 4, 4, 0);
        _drawBrow(canvas, stroke, Offset(55 + eyeDx, 34 + eyeShift), Offset(115 + eyeDx, 49 + eyeShift));
        _drawBrow(canvas, stroke, Offset(210 + eyeDx, 56 + eyeShift), Offset(265 + eyeDx, 41 + eyeShift));
        for (var line = 0; line < 5; line++) {
          canvas.drawLine(Offset(130 + mouthDx, mouthY + 8 + line), Offset(190 + mouthDx, mouthY - 4 + line), stroke);
        }
        break;
      case 'look_around':
        final lookDx = math.sin(t * 2 * math.pi) * 9;
        if (blinking) {
          _drawBlinkEye(canvas, fill, leftX, eyeY, eyeWidth, 6, eyeRadius);
          _drawBlinkEye(canvas, fill, rightX, eyeY, eyeWidth, 6, eyeRadius);
        } else {
          _drawEye(canvas, fill, cut, leftX, eyeY, eyeWidth, eyeHeight, eyeRadius, lookDx, -2);
          _drawEye(canvas, fill, cut, rightX, eyeY, eyeWidth, eyeHeight, eyeRadius, lookDx, -2);
        }
        canvas.drawLine(Offset(143 + mouthDx, mouthY), Offset(178 + mouthDx, mouthY), stroke);
        canvas.drawLine(Offset(143 + mouthDx, mouthY + 2), Offset(178 + mouthDx, mouthY + 2), stroke);
        break;
      case 'kiss':
        _drawBlinkEye(canvas, fill, leftX, eyeY, eyeWidth, 8, eyeRadius);
        eye(rightX, eyeY, eyeWidth, eyeHeight, eyeRadius, 0, 0);
        _drawKissLips(canvas, fill, cut, mouthCenterX, mouthY);
        final hFloat = math.sin(t * 3 * math.pi) * 6;
        _drawHeart(canvas, fill, Offset(135 + mouthDx, 105 + eyeShift - hFloat.abs()), 6);
        _drawHeart(canvas, fill, Offset(200 + mouthDx, 75 + eyeShift - hFloat.abs() * 0.6), 4);
        break;
      case 'wink':
        for (var line = 0; line < 5; line++) {
          canvas.drawLine(Offset(leftX - eyeWidth / 2, eyeY + line), Offset(leftX + eyeWidth / 2, eyeY + line), stroke);
        }
        eye(rightX, eyeY, eyeWidth, eyeHeight, eyeRadius, 0, 0);
        _drawSmile(canvas, stroke, mouthCenterX, mouthY - 4, 38);
        break;
      case 'laugh':
        _drawHappyArc(canvas, stroke, leftX, eyeY, eyeWidth);
        _drawHappyArc(canvas, stroke, rightX, eyeY, eyeWidth);
        final laughBounce = math.sin(t * 4 * math.pi).abs() * 4;
        canvas.drawRRect(
          RRect.fromRectAndRadius(Rect.fromLTWH(135 + mouthDx, 126 + mouthShift - laughBounce, 38, 22 + laughBounce), const Radius.circular(7)),
          fill,
        );
        canvas.drawRRect(
          RRect.fromRectAndRadius(Rect.fromLTWH(140 + mouthDx, 130 + mouthShift - laughBounce, 30, 15 + laughBounce * 0.6), const Radius.circular(5)),
          cut,
        );
        break;
      case 'star_eyes':
        eye(leftX, eyeY, eyeWidth, eyeHeight, eyeRadius, 0, 0);
        eye(rightX, eyeY, eyeWidth, eyeHeight, eyeRadius, 0, 0);
        final starSpin = t * 2 * math.pi;
        _drawStar(canvas, stroke, Offset(leftX, eyeY), 9 + math.sin(starSpin) * 2);
        _drawStar(canvas, stroke, Offset(rightX, eyeY), 9 + math.sin(starSpin) * 2);
        _drawSmile(canvas, stroke, mouthCenterX, mouthY - 4, 44);
        break;
      case 'excited':
        eye(leftX, eyeY - 6, eyeWidth + 8, eyeHeight + 8, eyeRadius, 0, 0);
        eye(rightX, eyeY - 6, eyeWidth + 8, eyeHeight + 8, eyeRadius, 0, 0);
        _drawBrow(canvas, stroke, Offset(55 + eyeDx, eyeShift), Offset(125 + eyeDx, eyeShift));
        _drawBrow(canvas, stroke, Offset(195 + eyeDx, eyeShift), Offset(265 + eyeDx, eyeShift));
        _drawSmile(canvas, stroke, mouthCenterX, mouthY - 8, 52);
        break;
      case 'tongue':
        for (var line = 0; line < 5; line++) {
          canvas.drawLine(Offset(leftX - eyeWidth / 2, eyeY + line), Offset(leftX + eyeWidth / 2, eyeY + line), stroke);
        }
        eye(rightX, eyeY, eyeWidth, eyeHeight, eyeRadius, 0, 0);
        for (var line = 0; line < 4; line++) {
          canvas.drawLine(Offset(135 + mouthDx, mouthY - 4 + line), Offset(160 + mouthDx, mouthY + 4 + line), stroke);
          canvas.drawLine(Offset(160 + mouthDx, mouthY + 4 + line), Offset(185 + mouthDx, mouthY - 4 + line), stroke);
        }
        final tongueWiggle = math.sin(t * 4 * math.pi) * 2;
        canvas.drawRRect(
          RRect.fromRectAndRadius(Rect.fromLTWH(145 + mouthDx + tongueWiggle, 146 + mouthShift, 22, 15), const Radius.circular(7)),
          fill,
        );
        canvas.drawCircle(Offset(160 + mouthDx + tongueWiggle, 156 + mouthShift), 4, cut);
        break;
      case 'grateful':
        _drawHappyArc(canvas, stroke, leftX, eyeY, eyeWidth);
        _drawHappyArc(canvas, stroke, rightX, eyeY, eyeWidth);
        final blushPulse = 6.0 + math.sin(t * 2 * math.pi) * 1.5;
        canvas.drawCircle(Offset(leftX + 24, eyeY + 16), blushPulse, Paint()..color = _resolvedAccentColor);
        canvas.drawCircle(Offset(rightX - 24, eyeY + 16), blushPulse, Paint()..color = _resolvedAccentColor);
        _drawSmile(canvas, stroke, mouthCenterX, mouthY - 4, 48);
        break;
      case 'crying':
        eye(leftX, eyeY + 6, eyeWidth, eyeHeight - 9, eyeRadius, 0, 7);
        eye(rightX, eyeY + 6, eyeWidth, eyeHeight - 9, eyeRadius, 0, 7);
        _drawBrow(canvas, stroke, Offset(leftX - 26, eyeY - 11 + eyeShift), Offset(leftX + 18, eyeY - 30 + eyeShift));
        _drawBrow(canvas, stroke, Offset(rightX + 26, eyeY - 11 + eyeShift), Offset(rightX - 18, eyeY - 30 + eyeShift));
        _drawSadArc(canvas, stroke, mouthCenterX, mouthY - 4, 34);
        final tearDrop = (t * 30) % 20;
        _drawTear(canvas, fill, Offset(leftX + eyeWidth / 2 + 3, eyeY + 30 + tearDrop), 4);
        _drawTear(canvas, fill, Offset(rightX + eyeWidth / 2 + 3, eyeY + 36 + tearDrop * 0.7), 4);
        break;
      case 'blushing':
        eye(leftX, eyeY, eyeWidth, eyeHeight - 4, eyeRadius, -5, 3);
        eye(rightX, eyeY, eyeWidth, eyeHeight - 4, eyeRadius, -5, 3);
        final blushR = 10.0 + math.sin(t * 2 * math.pi) * 2;
        canvas.drawCircle(Offset(leftX + 28, eyeY + 18), blushR, Paint()..color = _resolvedAccentColor);
        canvas.drawCircle(Offset(rightX - 28, eyeY + 18), blushR, Paint()..color = _resolvedAccentColor);
        for (var line = 0; line < 3; line++) {
          canvas.drawLine(Offset(150 + mouthDx, mouthY + line), Offset(170 + mouthDx, mouthY + line), stroke);
        }
        break;
      case 'nervous':
        final jitter = math.sin(t * 12 * math.pi) * 1.5;
        eye(leftX + jitter, eyeY, eyeWidth + 4, eyeHeight + 4, eyeRadius, 4, 0);
        eye(rightX + jitter, eyeY, eyeWidth + 4, eyeHeight + 4, eyeRadius, 4, 0);
        _drawBrow(canvas, stroke, Offset(leftX - 26, eyeY - 32 + eyeShift), Offset(leftX + 26, eyeY - 30 + eyeShift));
        _drawBrow(canvas, stroke, Offset(rightX - 26, eyeY - 30 + eyeShift), Offset(rightX + 26, eyeY - 32 + eyeShift));
        for (var line = 0; line < 3; line++) {
          canvas.drawLine(Offset(144 + mouthDx, mouthY + 2 + line), Offset(176 + mouthDx, mouthY - 2 + line), stroke);
        }
        break;
      case 'proud':
        _drawHappyArc(canvas, stroke, leftX, eyeY - 4, eyeWidth + 4);
        _drawHappyArc(canvas, stroke, rightX, eyeY - 4, eyeWidth + 4);
        _drawSmile(canvas, stroke, mouthCenterX, mouthY - 6, 56);
        final sparkle = math.sin(t * 3 * math.pi).abs();
        _drawStar(canvas, fill, Offset(140 + eyeDx, eyeShift + 18), 5.0 + sparkle * 2);
        _drawStar(canvas, fill, Offset(160 + eyeDx, eyeShift + 12), 7.0 + sparkle * 2);
        _drawStar(canvas, fill, Offset(180 + eyeDx, eyeShift + 18), 5.0 + sparkle * 2);
        break;
      case 'skeptical':
        eye(leftX, eyeY + 4, eyeWidth - 8, eyeHeight - 12, eyeRadius - 2, 4, 4);
        eye(rightX, eyeY, eyeWidth + 4, eyeHeight + 4, eyeRadius + 2, 4, 0);
        _drawBrow(canvas, stroke, Offset(leftX - 22, eyeY - 18 + eyeShift), Offset(leftX + 22, eyeY - 20 + eyeShift));
        final browRaise = math.sin(t * 2 * math.pi) * 3;
        _drawBrow(canvas, stroke, Offset(rightX - 22, eyeY - 36 + eyeShift + browRaise), Offset(rightX + 22, eyeY - 32 + eyeShift + browRaise));
        for (var line = 0; line < 4; line++) {
          canvas.drawLine(Offset(144 + mouthDx, mouthY + 2 + line), Offset(176 + mouthDx, mouthY - 2 + line), stroke);
        }
        break;
      case 'peaceful':
        _drawHappyArc(canvas, stroke, leftX, eyeY + 2, eyeWidth - 4);
        _drawHappyArc(canvas, stroke, rightX, eyeY + 2, eyeWidth - 4);
        for (var line = 0; line < 3; line++) {
          canvas.drawLine(Offset(148 + mouthDx, mouthY + line), Offset(172 + mouthDx, mouthY + line), stroke);
        }
        break;
      case 'determined':
        eye(leftX, eyeY + 2, eyeWidth, eyeHeight - 6, eyeRadius, 0, 2);
        eye(rightX, eyeY + 2, eyeWidth, eyeHeight - 6, eyeRadius, 0, 2);
        _drawBrow(canvas, stroke, Offset(leftX - 22, eyeY - 20 + eyeShift), Offset(leftX + 22, eyeY - 28 + eyeShift));
        _drawBrow(canvas, stroke, Offset(rightX - 22, eyeY - 28 + eyeShift), Offset(rightX + 22, eyeY - 20 + eyeShift));
        for (var line = 0; line < 5; line++) {
          canvas.drawLine(Offset(140 + mouthDx, mouthY + line), Offset(180 + mouthDx, mouthY + line), stroke);
        }
        break;
      case 'happy':
      default:
        eye(leftX, eyeY, eyeWidth, eyeHeight, eyeRadius, 0, 2);
        eye(rightX, eyeY, eyeWidth, eyeHeight, eyeRadius, 0, 2);
        _drawSmile(canvas, stroke, mouthCenterX, mouthY - 6, 44);
        break;
    }

  }

  void _drawScreenBoundary(Canvas canvas) {
    final mask = Paint()
      ..color = Colors.black
      ..style = PaintingStyle.fill
      ..isAntiAlias = true;
    final boundary = Paint()
      ..color = const Color(0xFFFFB6A6)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1
      ..isAntiAlias = true;

    canvas.drawRect(
      Rect.fromLTWH(0, 0, 320, _visibleScreenRect.top),
      mask,
    );
    canvas.drawRect(
      Rect.fromLTWH(0, _visibleScreenRect.bottom, 320, 240 - _visibleScreenRect.bottom),
      mask,
    );
    canvas.drawRect(
      Rect.fromLTWH(0, _visibleScreenRect.top, _visibleScreenRect.left, _visibleScreenRect.height),
      mask,
    );
    canvas.drawRect(
      Rect.fromLTWH(_visibleScreenRect.right, _visibleScreenRect.top, 320 - _visibleScreenRect.right, _visibleScreenRect.height),
      mask,
    );
    canvas.drawRect(_visibleScreenRect, boundary);
  }

  @override
  bool shouldRepaint(covariant _CompanionFacePainter oldDelegate) {
    return personality != oldDelegate.personality ||
        petMode != oldDelegate.petMode ||
        referencePose != oldDelegate.referencePose ||
        showScreenBoundary != oldDelegate.showScreenBoundary ||
        expression != oldDelegate.expression ||
        hair != oldDelegate.hair ||
        ears != oldDelegate.ears ||
        mustache != oldDelegate.mustache ||
        glasses != oldDelegate.glasses ||
        headwear != oldDelegate.headwear ||
        piercing != oldDelegate.piercing ||
        hairSize != oldDelegate.hairSize ||
        mustacheSize != oldDelegate.mustacheSize ||
        hairWidth != oldDelegate.hairWidth ||
        hairHeight != oldDelegate.hairHeight ||
        hairThickness != oldDelegate.hairThickness ||
        hairOffsetX != oldDelegate.hairOffsetX ||
        hairOffsetY != oldDelegate.hairOffsetY ||
        eyeOffsetX != oldDelegate.eyeOffsetX ||
        eyeOffsetY != oldDelegate.eyeOffsetY ||
        mouthOffsetX != oldDelegate.mouthOffsetX ||
        mouthOffsetY != oldDelegate.mouthOffsetY ||
        mustacheWidth != oldDelegate.mustacheWidth ||
        mustacheHeight != oldDelegate.mustacheHeight ||
        mustacheThickness != oldDelegate.mustacheThickness ||
        mustacheOffsetX != oldDelegate.mustacheOffsetX ||
        mustacheOffsetY != oldDelegate.mustacheOffsetY ||
        animationProgress != oldDelegate.animationProgress ||
        eyeColor != oldDelegate.eyeColor ||
        faceColor != oldDelegate.faceColor ||
        accentColor != oldDelegate.accentColor ||
        bodyColor != oldDelegate.bodyColor;
  }
}