import 'dart:async';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';

import '../models/companion_scene.dart';
import '../models/companion_visual_model.dart';
import '../models/companion_image_payload.dart';
import '../providers/desk_companion_controller.dart';
import '../theme/companion_theme.dart';
import '../utils/oled_bitmap_codec.dart';
import '../widgets/companion_face_preview.dart';
import '../widgets/note_card_preview.dart';
import '../widgets/oled_drawing_pad.dart';

const int kMaxNoteCharacters = 80;

enum DeskFlower { rose, sunflower, kingProtea }

extension DeskFlowerExt on DeskFlower {
  String get label => switch (this) {
        DeskFlower.rose => 'Rose',
        DeskFlower.sunflower => 'Sunflower',
        DeskFlower.kingProtea => 'King Protea',
      };

  String get command => switch (this) {
        DeskFlower.rose => 'rose',
        DeskFlower.sunflower => 'sunflower',
        DeskFlower.kingProtea => 'king_protea',
      };

  String get description => switch (this) {
        DeskFlower.rose => 'Petals unfurl outward in a layered spiral bloom.',
        DeskFlower.sunflower => 'Seed spiral glows at the center, petals radiate like rays of sun.',
        DeskFlower.kingProtea => 'Bold spiky bracts fan out around a dense center — South Africa\'s wild queen.',
      };
}

enum DeskExpression {
  happy,
  smile,
  confused,
  lookAround,
  kiss,
  heart,
  sad,
  angry,
  surprised,
  sleepy,
  love,
  thinking,
  wink,
  laugh,
  starEyes,
  excited,
  tongue,
}

extension DeskExpressionLabel on DeskExpression {
  String get label => switch (this) {
        DeskExpression.happy => 'Happy',
        DeskExpression.smile => 'Smiling',
      DeskExpression.confused => 'Confused',
        DeskExpression.lookAround => 'Look around',
        DeskExpression.kiss => 'Blow kisses',
        DeskExpression.heart => 'Big heart',
        DeskExpression.sad => 'Sad',
        DeskExpression.angry => 'Angry',
        DeskExpression.surprised => 'Surprised',
        DeskExpression.sleepy => 'Sleepy',
        DeskExpression.love => 'In love',
        DeskExpression.thinking => 'Thinking',
        DeskExpression.wink => 'Wink',
        DeskExpression.laugh => 'Laughing',
        DeskExpression.starEyes => 'Star eyes',
        DeskExpression.excited => 'Excited',
        DeskExpression.tongue => 'Tongue out',
      };

  String get command => switch (this) {
        DeskExpression.happy => 'happy',
        DeskExpression.smile => 'smile',
      DeskExpression.confused => 'confused',
        DeskExpression.lookAround => 'look_around',
        DeskExpression.kiss => 'kiss',
        DeskExpression.heart => 'heart',
        DeskExpression.sad => 'sad',
        DeskExpression.angry => 'angry',
        DeskExpression.surprised => 'surprised',
        DeskExpression.sleepy => 'sleepy',
        DeskExpression.love => 'love',
        DeskExpression.thinking => 'thinking',
        DeskExpression.wink => 'wink',
        DeskExpression.laugh => 'laugh',
        DeskExpression.starEyes => 'star_eyes',
        DeskExpression.excited => 'excited',
        DeskExpression.tongue => 'tongue',
      };

  String get subtitle => switch (this) {
        DeskExpression.happy => 'Bright open eyes with a cheerful grin.',
        DeskExpression.smile => 'Soft smile with squinted arc eyes.',
      DeskExpression.confused => 'Asymmetric brows and a tilted puzzled mouth.',
        DeskExpression.lookAround => 'Pupils drift left and right like a curious bot.',
        DeskExpression.kiss => 'One wink, hearts float up from the lips.',
        DeskExpression.heart => 'A big pulsing heart that fills the screen.',
        DeskExpression.sad => 'Droopy eyes, frown, a single falling tear.',
        DeskExpression.angry => 'Angled brows, squinting eyes, tight flat mouth.',
        DeskExpression.surprised => 'Huge eyes that grow wide, open-O mouth.',
        DeskExpression.sleepy => 'Half-closed eyes slowly blinking, ZZZ rising.',
        DeskExpression.love => 'Heart-shaped pupils and a giant grin.',
        DeskExpression.thinking => 'One squinted eye, gaze up, thought bubble.',
        DeskExpression.wink => 'One eye closes playfully, slight smile.',
        DeskExpression.laugh => 'Eyes crinkle shut, huge open-mouth laugh.',
        DeskExpression.starEyes => 'Star-shaped pupils shimmer with amazement.',
        DeskExpression.excited => 'Wide bouncing eyes, can\'t stop smiling.',
        DeskExpression.tongue => 'One wink and a cheeky tongue poke.',
      };
}

enum DeskPersonality { playful, cuddly, sleepy, curious }

extension DeskPersonalityExt on DeskPersonality {
  String get label => switch (this) {
        DeskPersonality.playful => 'Playful',
        DeskPersonality.cuddly => 'Cuddly',
        DeskPersonality.sleepy => 'Sleepy',
        DeskPersonality.curious => 'Curious',
      };

  String get command => switch (this) {
        DeskPersonality.playful => 'playful',
        DeskPersonality.cuddly => 'cuddly',
        DeskPersonality.sleepy => 'sleepy',
        DeskPersonality.curious => 'curious',
      };

  String get description => switch (this) {
        DeskPersonality.playful => 'Chases attention, reacts fast, and leans toward excited little scenes.',
        DeskPersonality.cuddly => 'Warm, affectionate, and likely to answer you with hearts and soft faces.',
        DeskPersonality.sleepy => 'Calmer and slower, with heavier eyelids and relaxed reactions.',
        DeskPersonality.curious => 'Watches the room, looks around, and reacts like a tiny thoughtful companion.',
      };
}

enum DeskPetMode { off, hangout, play, cuddle, nap, needy, party }

extension DeskPetModeExt on DeskPetMode {
  String get label => switch (this) {
        DeskPetMode.off => 'Off',
        DeskPetMode.hangout => 'Hangout',
        DeskPetMode.play => 'Play',
        DeskPetMode.cuddle => 'Cuddle',
        DeskPetMode.nap => 'Nap',
        DeskPetMode.needy => 'Needy',
        DeskPetMode.party => 'Party',
      };

  String get command => switch (this) {
      DeskPetMode.off => 'off',
        DeskPetMode.hangout => 'hangout',
        DeskPetMode.play => 'play',
        DeskPetMode.cuddle => 'cuddle',
        DeskPetMode.nap => 'nap',
        DeskPetMode.needy => 'needy',
        DeskPetMode.party => 'party',
      };

  String get description => switch (this) {
        DeskPetMode.off => 'Turns companion behavior off and leaves the face in a simple neutral state.',
        DeskPetMode.hangout => 'Default companion state. Personality can drive the idle behavior here.',
        DeskPetMode.play => 'Persistent playful mode with energetic scenes and more active attention-seeking behavior.',
        DeskPetMode.cuddle => 'Persistent affectionate mode with hearts, kisses, and softer reactions.',
        DeskPetMode.nap => 'Persistent sleepy mode with longer pauses and drowsy expressions.',
        DeskPetMode.needy => 'Persistent attention-seeking mode that acts like it wants you to notice it.',
        DeskPetMode.party => 'Persistent party mode inspired by desk companions that dance, show off, and stay extra excited.',
      };
}

enum DeskHairStyle { none, tuft, bangs, spiky, swoop, bob, messy }

extension DeskHairStyleExt on DeskHairStyle {
  String get label => switch (this) {
        DeskHairStyle.none => 'None',
        DeskHairStyle.tuft => 'Tuft',
        DeskHairStyle.bangs => 'Bangs',
        DeskHairStyle.spiky => 'Spiky',
        DeskHairStyle.swoop => 'Swoop',
        DeskHairStyle.bob => 'Bob',
        DeskHairStyle.messy => 'Messy',
      };

  String get command => switch (this) {
        DeskHairStyle.none => 'none',
        DeskHairStyle.tuft => 'tuft',
        DeskHairStyle.bangs => 'bangs',
        DeskHairStyle.spiky => 'spiky',
        DeskHairStyle.swoop => 'swoop',
        DeskHairStyle.bob => 'bob',
        DeskHairStyle.messy => 'messy',
      };
}

enum DeskEarsStyle { none, cat, bear, bunny }

extension DeskEarsStyleExt on DeskEarsStyle {
  String get label => switch (this) {
        DeskEarsStyle.none => 'None',
        DeskEarsStyle.cat => 'Cat',
        DeskEarsStyle.bear => 'Bear',
        DeskEarsStyle.bunny => 'Bunny',
      };

  String get command => switch (this) {
        DeskEarsStyle.none => 'none',
        DeskEarsStyle.cat => 'cat',
        DeskEarsStyle.bear => 'bear',
        DeskEarsStyle.bunny => 'bunny',
      };
}

enum DeskMustacheStyle {
  none,
  classic,
  curled,
  handlebar,
  walrus,
  pencil,
  imperial,
}

extension DeskMustacheStyleExt on DeskMustacheStyle {
  String get label => switch (this) {
        DeskMustacheStyle.none => 'None',
        DeskMustacheStyle.classic => 'Classic',
        DeskMustacheStyle.curled => 'Curled',
        DeskMustacheStyle.handlebar => 'Handlebar',
        DeskMustacheStyle.walrus => 'Walrus',
        DeskMustacheStyle.pencil => 'Pencil',
        DeskMustacheStyle.imperial => 'Imperial',
      };

  String get command => switch (this) {
        DeskMustacheStyle.none => 'none',
        DeskMustacheStyle.classic => 'classic',
        DeskMustacheStyle.curled => 'curled',
        DeskMustacheStyle.handlebar => 'handlebar',
        DeskMustacheStyle.walrus => 'walrus',
        DeskMustacheStyle.pencil => 'pencil',
        DeskMustacheStyle.imperial => 'imperial',
      };
}

enum DeskGlassesStyle { none, round, square, visor }

extension DeskGlassesStyleExt on DeskGlassesStyle {
  String get label => switch (this) {
        DeskGlassesStyle.none => 'None',
        DeskGlassesStyle.round => 'Round',
        DeskGlassesStyle.square => 'Square',
        DeskGlassesStyle.visor => 'Visor',
      };

  String get command => switch (this) {
        DeskGlassesStyle.none => 'none',
        DeskGlassesStyle.round => 'round',
        DeskGlassesStyle.square => 'square',
        DeskGlassesStyle.visor => 'visor',
      };
}

enum DeskHeadwearStyle { none, bow, beanie, crown }

extension DeskHeadwearStyleExt on DeskHeadwearStyle {
  String get label => switch (this) {
        DeskHeadwearStyle.none => 'None',
        DeskHeadwearStyle.bow => 'Bow',
        DeskHeadwearStyle.beanie => 'Beanie',
        DeskHeadwearStyle.crown => 'Crown',
      };

  String get command => switch (this) {
        DeskHeadwearStyle.none => 'none',
        DeskHeadwearStyle.bow => 'bow',
        DeskHeadwearStyle.beanie => 'beanie',
        DeskHeadwearStyle.crown => 'crown',
      };
}

enum DeskPiercingStyle { none, brow, nose, lip }

extension DeskPiercingStyleExt on DeskPiercingStyle {
  String get label => switch (this) {
        DeskPiercingStyle.none => 'None',
        DeskPiercingStyle.brow => 'Brow',
        DeskPiercingStyle.nose => 'Nose',
        DeskPiercingStyle.lip => 'Lip',
      };

  String get command => switch (this) {
        DeskPiercingStyle.none => 'none',
        DeskPiercingStyle.brow => 'brow',
        DeskPiercingStyle.nose => 'nose',
        DeskPiercingStyle.lip => 'lip',
      };
}

enum DeskCareAction { pet, cheer, comfort, dance, surprise }

extension DeskCareActionExt on DeskCareAction {
  String get label => switch (this) {
        DeskCareAction.pet => 'Pet',
        DeskCareAction.cheer => 'Cheer up',
        DeskCareAction.comfort => 'Comfort',
        DeskCareAction.dance => 'Dance',
        DeskCareAction.surprise => 'Surprise me',
      };

  String get command => switch (this) {
        DeskCareAction.pet => 'pet',
        DeskCareAction.cheer => 'cheer',
        DeskCareAction.comfort => 'comfort',
        DeskCareAction.dance => 'dance',
        DeskCareAction.surprise => 'surprise',
      };

  String get description => switch (this) {
        DeskCareAction.pet => 'Simulates a head pat or affectionate touch response.',
        DeskCareAction.cheer => 'Gives it a happy little boost when it feels flat.',
        DeskCareAction.comfort => 'Settles it down like a reassuring companion moment.',
        DeskCareAction.dance => 'Triggers a high-energy party reaction.',
        DeskCareAction.surprise => 'Lets the companion choose a playful reaction on its own.',
      };
}

enum _StudioWorkspace { studio, send, setup }

extension _StudioWorkspaceExt on _StudioWorkspace {
  String get label => switch (this) {
        _StudioWorkspace.studio => 'Studio',
        _StudioWorkspace.send => 'Send',
        _StudioWorkspace.setup => 'Setup',
      };

  IconData get icon => switch (this) {
        _StudioWorkspace.studio => Icons.auto_awesome,
        _StudioWorkspace.send => Icons.send_rounded,
        _StudioWorkspace.setup => Icons.settings_input_component,
      };
}

class _StudioPreset {
  const _StudioPreset({
    required this.title,
    required this.subtitle,
    required this.visualModel,
    required this.scene,
    required this.expression,
  });

  final String title;
  final String subtitle;
  final CompanionVisualModel visualModel;
  final CompanionScene scene;
  final DeskExpression expression;
}

class DeskCompanionStudioScreen extends StatefulWidget {
  const DeskCompanionStudioScreen({super.key});

  @override
  State<DeskCompanionStudioScreen> createState() =>
      _DeskCompanionStudioScreenState();
}

class _DeskCompanionStudioScreenState extends State<DeskCompanionStudioScreen> {
  final _noteController = TextEditingController();
  final _bannerController = TextEditingController(text: 'miss you already <3');
  double _noteFontSize = 1;
  int _noteBorderStyle = 0;
  final List<String> _noteIcons = [];
  String? _noteFlowerAccent;  // flower accent for note decoration

  DeskFlower _selectedFlower = DeskFlower.rose;

  CompanionImagePayload? _selectedImage;
  Uint8List _drawBitmap = Uint8List(OledBitmapCodec.byteLength);
  bool _invertImage = false;
  bool _showGrid = true;
  bool _drawModeEnabled = false;
  bool _liveDraw = false;
  bool _eraserMode = false;
  double _bannerSpeed = 35;
  double _brushSize = 2;
  _StudioWorkspace _activeWorkspace = _StudioWorkspace.studio;
  DeskExpression _selectedExpression = DeskExpression.happy;
  CompanionScene _selectedScene = CompanionScene.none;
  DeskPersonality _selectedPersonality = DeskPersonality.curious;
  DeskPetMode _selectedPetMode = DeskPetMode.hangout;
  DeskCareAction _selectedCareAction = DeskCareAction.pet;
  CompanionVisualModel _selectedVisualModel = CompanionVisualModel.classic;
  DeskHairStyle _selectedHairStyle = DeskHairStyle.none;
  DeskEarsStyle _selectedEarsStyle = DeskEarsStyle.none;
  DeskMustacheStyle _selectedMustacheStyle = DeskMustacheStyle.none;
  DeskGlassesStyle _selectedGlassesStyle = DeskGlassesStyle.none;
  DeskHeadwearStyle _selectedHeadwearStyle = DeskHeadwearStyle.none;
  DeskPiercingStyle _selectedPiercingStyle = DeskPiercingStyle.none;
  double _selectedHairSize = 100;
  double _selectedHairWidth = 100;
  double _selectedHairHeight = 100;
  double _selectedHairThickness = 100;
  double _selectedHairOffsetX = 0;
  double _selectedHairOffsetY = 0;
  double _selectedEyeOffsetY = 0;
  double _selectedMouthOffsetY = 0;
  double _selectedMustacheSize = 100;
  double _selectedMustacheWidth = 100;
  double _selectedMustacheHeight = 100;
  double _selectedMustacheThickness = 100;
  double _selectedMustacheOffsetX = 0;
  double _selectedMustacheOffsetY = 0;
  double _stickFigureScale = 100;
  double _stickFigureSpacing = 100;
  double _stickFigureEnergy = 55;
  bool _appearancePreviewReferencePose = true;
  bool _behaviorDraftDirty = false;
  bool _appearanceDraftDirty = false;
  Timer? _liveSyncTimer;
  final Stopwatch _scenePlaybackClock = Stopwatch();
  bool _scenePlaybackActive = false;
  int _scenePlaybackToken = 0;

  static const List<_StudioPreset> _studioPresets = [
    _StudioPreset(
      title: 'Sweethearts',
      subtitle: 'Stick duo kiss scene with a soft romantic vibe.',
      visualModel: CompanionVisualModel.stickFigure,
      scene: CompanionScene.kiss,
      expression: DeskExpression.love,
    ),
    _StudioPreset(
      title: 'Besties',
      subtitle: 'Hold-hands duo pose with a bright friendly energy.',
      visualModel: CompanionVisualModel.stickFigure,
      scene: CompanionScene.holdHands,
      expression: DeskExpression.happy,
    ),
    _StudioPreset(
      title: 'Tiny Bow',
      subtitle: 'Solo greeting pose for a neat little intro moment.',
      visualModel: CompanionVisualModel.stickFigure,
      scene: CompanionScene.bow,
      expression: DeskExpression.smile,
    ),
    _StudioPreset(
      title: 'Classic Wink',
      subtitle: 'The firmware-faithful face with a playful expression.',
      visualModel: CompanionVisualModel.classic,
      scene: CompanionScene.none,
      expression: DeskExpression.wink,
    ),
    _StudioPreset(
      title: 'Robot Parade',
      subtitle: 'Animated robot duo with a clean geometric hold-hands pose.',
      visualModel: CompanionVisualModel.robot,
      scene: CompanionScene.holdHands,
      expression: DeskExpression.excited,
    ),
  ];

  @override
  void initState() {
    super.initState();
    _noteController.addListener(_onNoteTextChanged);
  }

  @override
  void dispose() {
    _noteController.removeListener(_onNoteTextChanged);
    _noteController.dispose();
    _bannerController.dispose();
    _liveSyncTimer?.cancel();
    _stopLiveScenePlayback(updateUi: false);
    super.dispose();
  }

  void _onNoteTextChanged() {
    if (mounted) setState(() {});
  }

  void _syncBehaviorDraft(
    DeskCompanionController controller,
    DeskPersonality currentPersonality,
    DeskPetMode currentPetMode,
  ) {
    if (_behaviorDraftDirty) {
      return;
    }
    _selectedPersonality = currentPersonality;
    _selectedPetMode = currentPetMode;
  }

  void _syncAppearanceDraft(DeskCompanionController controller) {
    if (_appearanceDraftDirty) {
      return;
    }
    _selectedVisualModel = companionVisualModelFromCommand(
      controller.companionVisualModel,
    );
    _selectedScene = companionSceneFromCommand(controller.companionScene);
    _selectedHairStyle =
        _hairStyleFromCommand(controller.companionHair) ?? _selectedHairStyle;
    _selectedEarsStyle =
        _earsStyleFromCommand(controller.companionEars) ?? _selectedEarsStyle;
    _selectedMustacheStyle =
        _mustacheStyleFromCommand(controller.companionMustache) ??
            _selectedMustacheStyle;
    _selectedGlassesStyle =
        _glassesStyleFromCommand(controller.companionGlasses) ??
            _selectedGlassesStyle;
    _selectedHeadwearStyle =
        _headwearStyleFromCommand(controller.companionHeadwear) ??
            _selectedHeadwearStyle;
    _selectedPiercingStyle =
        _piercingStyleFromCommand(controller.companionPiercing) ??
            _selectedPiercingStyle;
    _selectedHairSize = controller.companionHairSize.toDouble();
    _selectedHairWidth = controller.companionHairWidth.toDouble();
    _selectedHairHeight = controller.companionHairHeight.toDouble();
    _selectedHairThickness = controller.companionHairThickness.toDouble();
    _selectedHairOffsetX = controller.companionHairOffsetX.toDouble();
    _selectedHairOffsetY = controller.companionHairOffsetY.toDouble();
    _selectedEyeOffsetY = controller.companionEyeOffsetY.toDouble();
    _selectedMouthOffsetY = controller.companionMouthOffsetY.toDouble();
    _selectedMustacheSize = controller.companionMustacheSize.toDouble();
    _selectedMustacheWidth = controller.companionMustacheWidth.toDouble();
    _selectedMustacheHeight = controller.companionMustacheHeight.toDouble();
    _selectedMustacheThickness =
      controller.companionMustacheThickness.toDouble();
    _selectedMustacheOffsetX = controller.companionMustacheOffsetX.toDouble();
    _selectedMustacheOffsetY = controller.companionMustacheOffsetY.toDouble();
    _stickFigureScale = controller.stickFigureScale.toDouble();
    _stickFigureSpacing = controller.stickFigureSpacing.toDouble();
    _stickFigureEnergy = controller.stickFigureEnergy.toDouble();
  }

  void _updateSelectedVisualModel(
    DeskCompanionController controller,
    CompanionVisualModel model,
  ) {
    if (_selectedVisualModel == model) {
      return;
    }

    setState(() {
      _appearanceDraftDirty = true;
      _selectedVisualModel = model;
    });
    _persistStudioPreviewState(controller);
  }

  void _persistStudioPreviewState(DeskCompanionController controller) {
    unawaited(
      controller.updateStudioPreviewSettings(
        visualModel: _selectedVisualModel.command,
        scene: _selectedScene.command,
        stickFigureScale: _stickFigureScale.round(),
        stickFigureSpacing: _stickFigureSpacing.round(),
        stickFigureEnergy: _stickFigureEnergy.round(),
      ),
    );
  }

  void _applyStudioPreset(
    DeskCompanionController controller,
    _StudioPreset preset,
  ) {
    setState(() {
      _appearanceDraftDirty = true;
      _selectedVisualModel = preset.visualModel;
      _selectedScene = preset.scene;
      _selectedExpression = preset.expression;
      if (preset.visualModel == CompanionVisualModel.stickFigure) {
        _appearancePreviewReferencePose = false;
      }
    });
    _persistStudioPreviewState(controller);
  }

  @override
  Widget build(BuildContext context) {
    final controller = context.watch<DeskCompanionController>();
    final currentPersonality =
      _personalityFromCommand(controller.petPersonality) ??
        _selectedPersonality;
    final currentPetMode =
      _petModeFromCommand(controller.activePetMode) ?? _selectedPetMode;
    _syncBehaviorDraft(controller, currentPersonality, currentPetMode);
    _syncAppearanceDraft(controller);

    return Scaffold(
      appBar: AppBar(title: const Text('Companion Studio')),
      body: DecoratedBox(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: [Color(0xFF130C12), CompanionTheme.cream],
          ),
        ),
        child: SafeArea(
          child: SingleChildScrollView(
            physics: _drawModeEnabled
                ? const NeverScrollableScrollPhysics()
                : const BouncingScrollPhysics(),
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 24),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                _buildStudioHeader(context, controller),
                const SizedBox(height: 18),
                _buildWorkspaceSwitcher(context),
                const SizedBox(height: 18),
                if (_activeWorkspace == _StudioWorkspace.setup) ...[
                _SectionCard(
                  title: 'Device link',
                  subtitle: controller.statusMessage,
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Wrap(
                        spacing: 10,
                        runSpacing: 10,
                        children: [
                          _ChipLabel(
                            label: controller.isBleConnected
                                ? 'BLE: ${controller.deviceName}'
                                : 'BLE: disconnected',
                          ),
                          _ChipLabel(
                            label: controller.isBleConnected
                                ? 'Transport: BLE ready'
                                : 'Transport: BLE required',
                          ),
                          _ChipLabel(label: 'Mode: ${controller.mode}'),
                        ],
                      ),
                      const SizedBox(height: 14),
                      Row(
                        children: [
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed: controller.busy
                                  ? null
                                  : controller.isBleConnected
                                      ? null
                                      : () => controller.scanAndConnect(),
                              icon: const Icon(Icons.bluetooth_searching),
                              label: const Text('Connect BLE'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy
                                  ? null
                                  : controller.isBleConnected
                                      ? () => controller.disconnect()
                                      : null,
                              icon: Icon(
                                controller.isBleConnected
                                    ? Icons.link_off
                                    : Icons.refresh,
                              ),
                              label: Text(
                                controller.isBleConnected
                                    ? 'Disconnect'
                                    : 'Refresh status',
                              ),
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Setup',
                  subtitle:
                      'Wi-Fi and relay provisioning are intentionally removed for now. This build only controls the desk companion over BLE.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        controller.isBleConnected
                            ? 'BLE is the only active link in this reset build. You can still send notes, images, expressions, scenes, and behavior changes once the device is paired.'
                            : 'Connect to the desk companion over BLE to use the app. Remote setup, Wi-Fi joins, and relay delivery are intentionally disabled until they are rebuilt cleanly.',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 14),
                      Container(
                        width: double.infinity,
                        padding: const EdgeInsets.all(12),
                        decoration: BoxDecoration(
                          color: CompanionTheme.surface,
                          borderRadius: BorderRadius.circular(18),
                          border: Border.all(color: CompanionTheme.blush),
                        ),
                        child: Text(
                          'Fresh reset: any old Wi-Fi or relay state is being ignored so the transport layer can be re-added later from a clean baseline.',
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                ],
                if (_activeWorkspace == _StudioWorkspace.studio) ...[
                _SectionCard(
                  title: 'Companion behavior',
                  subtitle:
                      'Choose the vibe and reactions here without mirroring the selected mode back on the display.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: [
                          _ChipLabel(label: 'Bond ${controller.bondLevel}%'),
                          _ChipLabel(label: 'Energy ${controller.energyLevel}%'),
                          _ChipLabel(label: 'Boredom ${controller.boredomLevel}%'),
                          _ChipLabel(
                            label: controller.canControlDevice
                                ? 'Linked'
                                : 'Waiting for link',
                          ),
                        ],
                      ),
                      const SizedBox(height: 16),
                      _StudioGroup(
                        title: 'Personality',
                        subtitle: _selectedPersonality.description,
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            _buildStudioDropdown(
                              context,
                              label: 'Personality',
                              value: _selectedPersonality,
                              values: DeskPersonality.values,
                              labelBuilder: (value) => value.label,
                              enabled: !controller.busy,
                              onChanged: (value) => setState(() {
                                _behaviorDraftDirty = true;
                                _selectedPersonality = value;
                              }),
                            ),
                            const SizedBox(height: 12),
                            SizedBox(
                              width: double.infinity,
                              child: ElevatedButton.icon(
                                onPressed: controller.busy ||
                                        !controller.canControlDevice
                                    ? null
                                    : () => _sendPersonality(controller),
                                icon: const Icon(Icons.pets_outlined),
                                label: const Text('Apply personality'),
                              ),
                            ),
                          ],
                        ),
                      ),
                      const SizedBox(height: 14),
                      _StudioGroup(
                        title: 'Mode',
                        subtitle: _selectedPetMode.description,
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            _buildStudioDropdown(
                              context,
                              label: 'Mode',
                              value: _selectedPetMode,
                              values: DeskPetMode.values,
                              labelBuilder: (value) => value.label,
                              enabled: !controller.busy,
                              onChanged: (value) => setState(() {
                                _behaviorDraftDirty = true;
                                _selectedPetMode = value;
                              }),
                            ),
                            const SizedBox(height: 12),
                            SizedBox(
                              width: double.infinity,
                              child: OutlinedButton.icon(
                                onPressed: controller.busy ||
                                        !controller.canControlDevice
                                    ? null
                                    : () => _triggerPetMode(controller),
                                icon: const Icon(Icons.auto_awesome_outlined),
                                label: const Text('Apply mode'),
                              ),
                            ),
                          ],
                        ),
                      ),
                      const SizedBox(height: 14),
                      _StudioGroup(
                        title: 'Care',
                        subtitle: _selectedCareAction.description,
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            _buildStudioDropdown(
                              context,
                              label: 'Care action',
                              value: _selectedCareAction,
                              values: DeskCareAction.values,
                              labelBuilder: (value) => value.label,
                              enabled: !controller.busy,
                              onChanged: (value) => setState(() => _selectedCareAction = value),
                            ),
                            const SizedBox(height: 12),
                            SizedBox(
                              width: double.infinity,
                              child: ElevatedButton.icon(
                                onPressed: controller.busy ||
                                        !controller.canControlDevice
                                    ? null
                                    : () => _sendCareAction(controller),
                                icon: const Icon(Icons.favorite_outline),
                                label: const Text('Send care action'),
                              ),
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _buildStyleStudioSection(
                  context,
                  controller,
                  currentPersonality,
                  currentPetMode,
                ),
                const SizedBox(height: 16),
                _buildExpressionStudioSection(
                  context,
                  controller,
                  currentPersonality,
                  currentPetMode,
                ),
                const SizedBox(height: 16),
                ],
                if (_activeWorkspace == _StudioWorkspace.send) ...[
                _buildStickyNoteSection(context, controller),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Banner',
                  subtitle: 'Scrolling marquee for short, cute status lines.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      TextField(
                        controller: _bannerController,
                        decoration:
                            const InputDecoration(labelText: 'Banner text'),
                      ),
                      const SizedBox(height: 10),
                      Text(
                        'Scroll speed: ${_bannerSpeed.round()} px/s',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      Slider(
                        min: 10,
                        max: 80,
                        value: _bannerSpeed,
                        onChanged: (value) =>
                            setState(() => _bannerSpeed = value),
                      ),
                      SizedBox(
                        width: double.infinity,
                        child: ElevatedButton.icon(
                          onPressed: controller.busy || !controller.canControlDevice
                              ? null
                              : () => _sendBanner(controller),
                          icon: const Icon(Icons.view_carousel_outlined),
                          label: const Text('Start banner'),
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Flowers',
                  subtitle:
                      'Full-screen animated flowers — a little gift that fills the whole display.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      _buildStudioDropdown(
                        context,
                        label: 'Flower animation',
                        value: _selectedFlower,
                        values: DeskFlower.values,
                        labelBuilder: (value) => value.label,
                        enabled: !controller.busy,
                        onChanged: (value) => setState(() => _selectedFlower = value),
                      ),
                      const SizedBox(height: 12),
                      Text(
                        _selectedFlower.description,
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 12),
                      SizedBox(
                        width: double.infinity,
                        child: ElevatedButton.icon(
                          onPressed: controller.busy || !controller.canControlDevice
                              ? null
                              : () => _perform(
                                    () => controller.sendFlower(
                                        _selectedFlower.command),
                                    success:
                                        '${_selectedFlower.label} blooming on the display!',
                                  ),
                          icon: const Icon(Icons.local_florist_outlined),
                          label: Text('Send ${_selectedFlower.label}'),
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Draw live',
                  subtitle: _drawModeEnabled
                      ? 'Draw mode is on. Page scrolling is locked until you turn it off.'
                      : 'Turn on Draw mode before sketching so the page does not scroll while you draw.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      OledDrawingPad(
                        bitmap: _drawBitmap,
                        showGrid: _showGrid,
                        enabled: _drawModeEnabled,
                        onPixel: _paintPixel,
                      ),
                      const SizedBox(height: 12),
                      Wrap(
                        spacing: 10,
                        runSpacing: 10,
                        children: [
                          ActionChip(
                            label: const Text('Fullscreen'),
                            avatar: const Icon(Icons.open_in_full, size: 18),
                            onPressed: () => _openFullscreenDrawEditor(),
                          ),
                          FilterChip(
                            label: const Text('Draw mode'),
                            selected: _drawModeEnabled,
                            onSelected: (_) => setState(
                                () => _drawModeEnabled = !_drawModeEnabled),
                          ),
                          FilterChip(
                            label: const Text('Pen'),
                            selected: !_eraserMode,
                            onSelected: (_) =>
                                setState(() => _eraserMode = false),
                          ),
                          FilterChip(
                            label: const Text('Eraser'),
                            selected: _eraserMode,
                            onSelected: (_) =>
                                setState(() => _eraserMode = true),
                          ),
                          FilterChip(
                            label: const Text('Grid'),
                            selected: _showGrid,
                            onSelected: (_) =>
                                setState(() => _showGrid = !_showGrid),
                          ),
                          FilterChip(
                            label: const Text('Live push'),
                            selected: _liveDraw,
                            onSelected: (_) =>
                                setState(() => _liveDraw = !_liveDraw),
                          ),
                        ],
                      ),
                      const SizedBox(height: 10),
                      Text(
                        'Brush size: ${_brushSize.round()} px',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      Slider(
                        min: 1,
                        max: 5,
                        divisions: 4,
                        value: _brushSize,
                        onChanged: (value) =>
                            setState(() => _brushSize = value),
                      ),
                      Row(
                        children: [
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy
                                  ? null
                                  : () {
                                      setState(() => _drawBitmap = Uint8List(
                                          OledBitmapCodec.byteLength));
                                      _queueLiveDraw();
                                    },
                              icon: const Icon(Icons.layers_clear_outlined),
                              label: const Text('Clear canvas'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy ? null : _fillCanvas,
                              icon: const Icon(Icons.texture_outlined),
                              label: const Text('Fill canvas'),
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 10),
                      Row(
                        children: [
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed:
                                  controller.busy ? null : _loadImageIntoCanvas,
                              icon: const Icon(Icons.move_down_outlined),
                              label: const Text('Use picked image'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed: controller.busy || !controller.canControlDevice
                                  ? null
                                  : () => _sendCanvas(controller),
                              icon: const Icon(Icons.draw_outlined),
                              label: const Text('Push drawing'),
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Image',
                  subtitle:
                      'Any image is resized to 128×64 and converted to a 1-bit OLED bitmap.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      if (_selectedImage != null)
                        Container(
                          width: double.infinity,
                          padding: const EdgeInsets.all(12),
                          decoration: BoxDecoration(
                            color: CompanionTheme.ink,
                            borderRadius: BorderRadius.circular(18),
                          ),
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              ClipRRect(
                                borderRadius: BorderRadius.circular(10),
                                child: Image.memory(
                                  _selectedImage!.previewPng,
                                  height: 96,
                                  width: double.infinity,
                                  fit: BoxFit.cover,
                                  gaplessPlayback: true,
                                ),
                              ),
                              const SizedBox(height: 10),
                              Text(
                                _selectedImage!.name,
                                style: Theme.of(context)
                                    .textTheme
                                    .titleMedium
                                    ?.copyWith(color: Colors.white),
                              ),
                              Text(
                                '${_selectedImage!.byteLength} bytes ready for OLED',
                                style: Theme.of(context)
                                    .textTheme
                                    .bodyMedium
                                    ?.copyWith(color: Colors.white70),
                              ),
                            ],
                          ),
                        )
                      else
                        Text(
                          'No image selected yet.',
                          style: Theme.of(context).textTheme.bodyMedium,
                        ),
                      const SizedBox(height: 10),
                      SwitchListTile.adaptive(
                        contentPadding: EdgeInsets.zero,
                        title: const Text('Invert image while converting'),
                        value: _invertImage,
                        onChanged: (value) =>
                            setState(() => _invertImage = value),
                      ),
                      Row(
                        children: [
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy ? null : _pickImage,
                              icon: const Icon(Icons.image_outlined),
                              label: const Text('Pick image'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed:
                                  controller.busy ||
                                          _selectedImage == null ||
                                          !controller.canControlDevice
                                      ? null
                                      : () => _sendImage(controller),
                              icon: const Icon(Icons.send_outlined),
                              label: const Text('Send image'),
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                ],
              ],
            ),
          ),
        ),
      ),
    );
  }

  Future<void> _pickImage() async {
    final result = await FilePicker.platform.pickFiles(
      allowMultiple: false,
      type: FileType.image,
      withData: true,
    );
    if (!mounted || result == null || result.files.isEmpty) {
      return;
    }

    final file = result.files.single;
    final bytes = file.bytes;
    if (bytes == null) {
      _showMessage('Image picker did not return bytes.');
      return;
    }

    try {
      final payload = OledBitmapCodec.encodeImage(
        sourceBytes: bytes,
        name: file.name,
        invert: _invertImage,
      );
      setState(() {
        _selectedImage = payload;
        _drawBitmap = Uint8List.fromList(payload.bitmap);
      });
    } on FormatException catch (error) {
      _showMessage(error.message);
    }
  }

  void _paintPixel(int x, int y) {
    final next = Uint8List.fromList(_drawBitmap);
    final radius = (_brushSize.round() - 1).clamp(0, 4);
    for (var py = y - radius; py <= y + radius; py++) {
      if (py < 0 || py >= 64) {
        continue;
      }
      for (var px = x - radius; px <= x + radius; px++) {
        if (px < 0 || px >= 128) {
          continue;
        }
        final byteIndex = py * 16 + (px >> 3);
        final bitMask = 1 << (7 - (px & 7));
        if (_eraserMode) {
          next[byteIndex] &= ~bitMask;
        } else {
          next[byteIndex] |= bitMask;
        }
      }
    }

    setState(() => _drawBitmap = next);
    _queueLiveDraw();
  }

  void _fillCanvas() {
    setState(() {
      _drawBitmap = Uint8List.fromList(
          List<int>.filled(OledBitmapCodec.byteLength, 0xFF));
    });
    _queueLiveDraw();
  }

  void _loadImageIntoCanvas() {
    final image = _selectedImage;
    if (image == null) {
      _showMessage('Pick an image first.');
      return;
    }

    setState(() => _drawBitmap = Uint8List.fromList(image.bitmap));
    _queueLiveDraw();
  }

  void _queueLiveDraw() {
    if (!_liveDraw) {
      return;
    }
    _liveSyncTimer?.cancel();
    _liveSyncTimer = Timer(const Duration(milliseconds: 120), () {
      if (!mounted) {
        return;
      }
      context.read<DeskCompanionController>().sendLiveBitmap(
            _drawBitmap,
          );
    });
  }

  Future<void> _openFullscreenDrawEditor() async {
    final result = await Navigator.of(context).push<_FullscreenDrawResult>(
      MaterialPageRoute(
        builder: (_) => _FullscreenDrawEditor(
          bitmap: _drawBitmap,
          showGrid: _showGrid,
          liveDraw: _liveDraw,
          eraserMode: _eraserMode,
          brushSize: _brushSize,
        ),
        fullscreenDialog: true,
      ),
    );

    if (!mounted || result == null) {
      return;
    }

    setState(() {
      _drawBitmap = result.bitmap;
      _showGrid = result.showGrid;
      _liveDraw = result.liveDraw;
      _eraserMode = result.eraserMode;
      _brushSize = result.brushSize;
      _drawModeEnabled = true;
    });
  }

  Future<void> _openAppearanceEditor() async {
    final controller = context.read<DeskCompanionController>();
    final currentPersonality =
        _personalityFromCommand(controller.petPersonality) ??
            _selectedPersonality;
    final currentPetMode =
        _petModeFromCommand(controller.activePetMode) ?? _selectedPetMode;
    final result = await Navigator.of(context).push<_AppearanceEditorResult>(
      MaterialPageRoute(
        builder: (_) => _FullscreenAppearanceEditor(
          personality: currentPersonality,
          petMode: currentPetMode,
          initialVisualModel: _selectedVisualModel,
          initialScene: _selectedScene,
          initialReferencePose: _appearancePreviewReferencePose,
          selectedExpression: _selectedExpression,
          hairStyle: _selectedHairStyle,
          earsStyle: _selectedEarsStyle,
          mustacheStyle: _selectedMustacheStyle,
          glassesStyle: _selectedGlassesStyle,
          headwearStyle: _selectedHeadwearStyle,
          piercingStyle: _selectedPiercingStyle,
          hairSize: _selectedHairSize,
          hairWidth: _selectedHairWidth,
          hairHeight: _selectedHairHeight,
          hairThickness: _selectedHairThickness,
          hairOffsetX: _selectedHairOffsetX,
          hairOffsetY: _selectedHairOffsetY,
          eyeOffsetY: _selectedEyeOffsetY,
          mouthOffsetY: _selectedMouthOffsetY,
          mustacheSize: _selectedMustacheSize,
          mustacheWidth: _selectedMustacheWidth,
          mustacheHeight: _selectedMustacheHeight,
          mustacheThickness: _selectedMustacheThickness,
          mustacheOffsetX: _selectedMustacheOffsetX,
          mustacheOffsetY: _selectedMustacheOffsetY,
          stickFigureScale: _stickFigureScale,
          stickFigureSpacing: _stickFigureSpacing,
          stickFigureEnergy: _stickFigureEnergy,
        ),
        fullscreenDialog: true,
      ),
    );

    if (!mounted || result == null) {
      return;
    }

    setState(() {
      _appearanceDraftDirty = true;
      _selectedVisualModel = result.visualModel;
      _selectedScene = result.scene;
      _appearancePreviewReferencePose = result.previewReferencePose;
      _selectedHairStyle = result.hairStyle;
      _selectedEarsStyle = result.earsStyle;
      _selectedMustacheStyle = result.mustacheStyle;
      _selectedGlassesStyle = result.glassesStyle;
      _selectedHeadwearStyle = result.headwearStyle;
      _selectedPiercingStyle = result.piercingStyle;
      _selectedHairSize = result.hairSize;
      _selectedHairWidth = result.hairWidth;
      _selectedHairHeight = result.hairHeight;
      _selectedHairThickness = result.hairThickness;
      _selectedHairOffsetX = result.hairOffsetX;
      _selectedHairOffsetY = result.hairOffsetY;
      _selectedMustacheSize = result.mustacheSize;
      _selectedMustacheWidth = result.mustacheWidth;
      _selectedMustacheHeight = result.mustacheHeight;
      _selectedMustacheThickness = result.mustacheThickness;
      _selectedMustacheOffsetX = result.mustacheOffsetX;
      _selectedMustacheOffsetY = result.mustacheOffsetY;
      _stickFigureScale = result.stickFigureScale;
      _stickFigureSpacing = result.stickFigureSpacing;
      _stickFigureEnergy = result.stickFigureEnergy;
    });
    _persistStudioPreviewState(controller);
  }

  Future<void> _openNoteEditor() async {
    final result = await Navigator.of(context).push<_NoteEditorResult>(
      MaterialPageRoute(
        builder: (_) => _FullscreenNoteEditor(
          text: _noteController.text.characters.take(kMaxNoteCharacters).toString(),
          fontSize: _noteFontSize,
          borderStyle: _noteBorderStyle,
          icons: List.unmodifiable(_noteIcons),
          flowerAccent: _noteFlowerAccent,
        ),
        fullscreenDialog: true,
      ),
    );

    if (!mounted || result == null) {
      return;
    }

    setState(() {
      _noteController.text = result.text;
      _noteFontSize = result.fontSize;
      _noteBorderStyle = result.borderStyle;
      _noteIcons
        ..clear()
        ..addAll(result.icons);
      _noteFlowerAccent = result.flowerAccent;
    });
  }

  Widget _buildStudioHeader(
    BuildContext context,
    DeskCompanionController controller,
  ) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(18),
      decoration: BoxDecoration(
        gradient: const LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [Color(0xFFFFFFFF), Color(0xFFEAEAEA)],
        ),
        borderRadius: BorderRadius.circular(28),
        border: Border.all(color: CompanionTheme.blush),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text('Companion Studio', style: Theme.of(context).textTheme.headlineMedium),
          const SizedBox(height: 8),
          Text(
            'Build looks, scenes, notes, and device actions without digging through one endless stack.',
            style: Theme.of(context).textTheme.bodyMedium,
          ),
          const SizedBox(height: 14),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              _ChipLabel(label: _selectedVisualModel.label),
              _ChipLabel(label: 'Scene: ${_selectedScene.label}'),
              _ChipLabel(label: controller.canControlDevice ? 'Device linked' : 'Preview mode'),
              _ChipLabel(label: _activeWorkspace.label),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildWorkspaceSwitcher(BuildContext context) {
    return Wrap(
      spacing: 10,
      runSpacing: 10,
      children: _StudioWorkspace.values
          .map(
            (workspace) => ChoiceChip(
              avatar: Icon(workspace.icon, size: 18),
              label: Text(workspace.label),
              selected: _activeWorkspace == workspace,
              onSelected: (_) {
                if (workspace != _StudioWorkspace.studio) {
                  _stopLiveScenePlayback();
                }
                setState(() => _activeWorkspace = workspace);
              },
            ),
          )
          .toList(growable: false),
    );
  }

  Widget _buildStudioSlider(
    BuildContext context,
    String label,
    double value,
    ValueChanged<double> onChanged,
    {
      double min = 70,
      double max = 170,
      int divisions = 20,
    }
  ) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(label, style: Theme.of(context).textTheme.bodyMedium),
        Slider(
          min: min,
          max: max,
          divisions: divisions,
          value: value,
          onChanged: onChanged,
        ),
      ],
    );
  }

  Widget _buildStudioDropdown<T>(
    BuildContext context, {
    required String label,
    required T value,
    required List<T> values,
    required String Function(T value) labelBuilder,
    required ValueChanged<T> onChanged,
    bool enabled = true,
  }) {
    return DropdownButtonFormField<T>(
      value: value,
      decoration: InputDecoration(labelText: label),
      items: values
          .map(
            (item) => DropdownMenuItem<T>(
              value: item,
              child: Text(labelBuilder(item)),
            ),
          )
          .toList(growable: false),
      onChanged: enabled
          ? (nextValue) {
              if (nextValue != null) {
                onChanged(nextValue);
              }
            }
          : null,
    );
  }

  Widget _buildStyleStudioSection(
    BuildContext context,
    DeskCompanionController controller,
    DeskPersonality currentPersonality,
    DeskPetMode currentPetMode,
  ) {
    final usesNativeClassicSend =
        _selectedVisualModel.isDeviceSupported && _selectedScene == CompanionScene.none;
    final supportsLiveBleScene = !usesNativeClassicSend && controller.isBleConnected;
    final streamsLiveBleScene =
      supportsLiveBleScene && !_appearancePreviewReferencePose;
    final previewPersonality = _appearancePreviewReferencePose
        ? _selectedPersonality.command
        : currentPersonality.command;
    final previewPetMode = _appearancePreviewReferencePose
        ? _selectedPetMode.command
        : currentPetMode.command;
    final previewExpression =
        _appearancePreviewReferencePose ? null : _selectedExpression.command;
    final previewMode = _appearancePreviewReferencePose ? 'Reference pose' : 'Live mood';
    final guideLabel = usesNativeClassicSend
        ? 'Classic native send is ready.'
      : streamsLiveBleScene
            ? 'BLE is connected, so this scene can stream live to the OLED.'
        : supportsLiveBleScene
          ? 'Reference pose stays on a still frame so BLE is not wasted on identical redraws.'
            : 'Without BLE, non-classic scenes fall back to a still bitmap snapshot.';
    final detailSummary = switch (_selectedVisualModel) {
      CompanionVisualModel.classic =>
        'Hair ${_selectedHairStyle.label}, ears ${_selectedEarsStyle.label}, mustache ${_selectedMustacheStyle.label}, glasses ${_selectedGlassesStyle.label}.',
      CompanionVisualModel.stickFigure =>
        'Scale ${_stickFigureScale.round()}%, spacing ${_stickFigureSpacing.round()}%, energy ${_stickFigureEnergy.round()}%.',
      CompanionVisualModel.robot =>
        'Robot uses the shared scene timeline and can stream live over BLE like the stick-figure scenes.',
    };

    return _SectionCard(
      title: 'Style studio',
      subtitle:
          'Use compact selectors here, then open the editor only when you need deeper detail work.',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            width: double.infinity,
            padding: const EdgeInsets.all(14),
            decoration: BoxDecoration(
              color: CompanionTheme.panel,
              borderRadius: BorderRadius.circular(20),
              border: Border.all(color: CompanionTheme.blush),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text('Live preview', style: Theme.of(context).textTheme.titleSmall),
                          const SizedBox(height: 2),
                          Text(
                            '${_selectedVisualModel.label} • ${_selectedScene.label} • $previewMode',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                DecoratedBox(
                  decoration: BoxDecoration(
                    color: CompanionTheme.ink,
                    borderRadius: BorderRadius.circular(18),
                  ),
                  child: Padding(
                    padding: const EdgeInsets.all(12),
                    child: CompanionFacePreview(
                      visualModel: _selectedVisualModel,
                      scene: _selectedScene,
                      personality: previewPersonality,
                      petMode: previewPetMode,
                      referencePose: _appearancePreviewReferencePose,
                      showScreenBoundary: true,
                      expression: previewExpression,
                      hair: _selectedHairStyle.command,
                      ears: _selectedEarsStyle.command,
                      mustache: _selectedMustacheStyle.command,
                      glasses: _selectedGlassesStyle.command,
                      headwear: _selectedHeadwearStyle.command,
                      piercing: _selectedPiercingStyle.command,
                      hairSize: _selectedHairSize.round(),
                      mustacheSize: _selectedMustacheSize.round(),
                      hairWidth: _selectedHairWidth.round(),
                      hairHeight: _selectedHairHeight.round(),
                      hairThickness: _selectedHairThickness.round(),
                      hairOffsetX: _selectedHairOffsetX.round(),
                      hairOffsetY: _selectedHairOffsetY.round(),
                      eyeOffsetY: _selectedEyeOffsetY.round(),
                      mouthOffsetY: _selectedMouthOffsetY.round(),
                      mustacheWidth: _selectedMustacheWidth.round(),
                      mustacheHeight: _selectedMustacheHeight.round(),
                      mustacheThickness: _selectedMustacheThickness.round(),
                      mustacheOffsetX: _selectedMustacheOffsetX.round(),
                      mustacheOffsetY: _selectedMustacheOffsetY.round(),
                      stickFigureScale: _stickFigureScale.round(),
                      stickFigureSpacing: _stickFigureSpacing.round(),
                      stickFigureEnergy: _stickFigureEnergy.round(),
                    ),
                  ),
                ),
                const SizedBox(height: 12),
                Text(
                  'Guide frame marks the real OLED area. $guideLabel',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
                const SizedBox(height: 12),
                Text('Preset packs', style: Theme.of(context).textTheme.titleSmall),
                const SizedBox(height: 8),
                SizedBox(
                  height: 148,
                  child: ListView.separated(
                    scrollDirection: Axis.horizontal,
                    itemBuilder: (context, index) {
                      final preset = _studioPresets[index];
                      final active = preset.visualModel == _selectedVisualModel &&
                          preset.scene == _selectedScene &&
                          preset.expression == _selectedExpression;
                      return SizedBox(
                        width: 224,
                        child: InkWell(
                          borderRadius: BorderRadius.circular(20),
                          onTap: controller.busy
                              ? null
                              : () => _applyStudioPreset(controller, preset),
                          child: Ink(
                            padding: const EdgeInsets.all(12),
                            decoration: BoxDecoration(
                              color: active
                                  ? CompanionTheme.coral.withValues(alpha: 0.18)
                                  : CompanionTheme.surface,
                              borderRadius: BorderRadius.circular(20),
                              border: Border.all(
                                color: active
                                    ? CompanionTheme.coral
                                    : CompanionTheme.blush,
                              ),
                            ),
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(preset.title, style: Theme.of(context).textTheme.titleSmall),
                                const SizedBox(height: 4),
                                Expanded(
                                  child: Text(
                                    preset.subtitle,
                                    maxLines: 3,
                                    overflow: TextOverflow.ellipsis,
                                    style: Theme.of(context).textTheme.bodySmall,
                                  ),
                                ),
                                const SizedBox(height: 10),
                                Container(
                                  padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
                                  decoration: BoxDecoration(
                                    color: CompanionTheme.panel,
                                    borderRadius: BorderRadius.circular(999),
                                    border: Border.all(color: CompanionTheme.blush),
                                  ),
                                  child: Text(
                                    '${preset.visualModel.label} / ${preset.scene.label}',
                                    maxLines: 1,
                                    overflow: TextOverflow.ellipsis,
                                    style: Theme.of(context).textTheme.labelSmall,
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ),
                      );
                    },
                    separatorBuilder: (_, __) => const SizedBox(width: 10),
                    itemCount: _studioPresets.length,
                  ),
                ),
                const SizedBox(height: 12),
                _StudioGroup(
                  title: 'Quick setup',
                  subtitle: 'The crowded chip walls are replaced with compact selectors here. Deep edits stay in the fullscreen editor.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      _buildStudioDropdown(
                        context,
                        label: 'Visual model',
                        value: _selectedVisualModel,
                        values: CompanionVisualModel.values,
                        labelBuilder: (value) => value.label,
                        enabled: !controller.busy,
                        onChanged: (value) {
                          _appearanceDraftDirty = true;
                          _updateSelectedVisualModel(controller, value);
                        },
                      ),
                      if (_selectedVisualModel != CompanionVisualModel.classic) ...[
                        const SizedBox(height: 10),
                        _buildStudioDropdown(
                          context,
                          label: 'Scene',
                          value: _selectedScene,
                          values: CompanionScene.values,
                          labelBuilder: (value) => value.label,
                          enabled: !controller.busy,
                          onChanged: (value) => setState(() {
                            _appearanceDraftDirty = true;
                            _selectedScene = value;
                            _persistStudioPreviewState(controller);
                          }),
                        ),
                        const SizedBox(height: 10),
                        Text(
                          _selectedScene.description,
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ] else ...[
                        const SizedBox(height: 10),
                        Text(
                          'Classic keeps scene editing out of the way here. Use Expression studio for mood changes and the fullscreen editor for face alignment.',
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ],
                      if (_selectedVisualModel == CompanionVisualModel.stickFigure) ...[
                        const SizedBox(height: 10),
                        _buildStudioDropdown(
                          context,
                          label: 'Expression / mood',
                          value: _selectedExpression,
                          values: DeskExpression.values,
                          labelBuilder: (value) => value.label,
                          enabled: !controller.busy,
                          onChanged: (value) => setState(() {
                            _selectedExpression = value;
                            if (_appearancePreviewReferencePose) {
                              _appearancePreviewReferencePose = false;
                            }
                          }),
                        ),
                      ] else if (_selectedVisualModel == CompanionVisualModel.robot) ...[
                        const SizedBox(height: 10),
                        Text(
                          'Robot currently follows scene timing only, so mood controls stay hidden until the robot motion language is expanded.',
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ],
                      const SizedBox(height: 6),
                      Text(
                        detailSummary,
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ],
                  ),
                ),
                if (_selectedVisualModel == CompanionVisualModel.stickFigure) ...[
                  const SizedBox(height: 12),
                  _StudioGroup(
                    title: 'Motion controls',
                    subtitle: 'Tune the stick-figure relationship motion without opening the full editor.',
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        _buildStudioSlider(
                          context,
                          'Figure scale ${_stickFigureScale.round()}%',
                          _stickFigureScale,
                          (value) => setState(() {
                            _appearanceDraftDirty = true;
                            _stickFigureScale = value;
                            _persistStudioPreviewState(controller);
                          }),
                        ),
                        _buildStudioSlider(
                          context,
                          'Partner spacing ${_stickFigureSpacing.round()}%',
                          _stickFigureSpacing,
                          (value) => setState(() {
                            _appearanceDraftDirty = true;
                            _stickFigureSpacing = value;
                            _persistStudioPreviewState(controller);
                          }),
                        ),
                        _buildStudioSlider(
                          context,
                          'Scene energy ${_stickFigureEnergy.round()}%',
                          _stickFigureEnergy,
                          (value) => setState(() {
                            _appearanceDraftDirty = true;
                            _stickFigureEnergy = value;
                            _persistStudioPreviewState(controller);
                          }),
                          min: 0,
                          max: 100,
                          divisions: 20,
                        ),
                      ],
                    ),
                  ),
                ] else if (_selectedVisualModel == CompanionVisualModel.robot) ...[
                  const SizedBox(height: 12),
                  _StudioGroup(
                    title: 'Robot model',
                    subtitle: 'Robot keeps the main Studio compact and can stream live over BLE when the device is connected directly.',
                    child: Text(
                      'Use presets, mood, and scene selection to drive the robot duo for now. Model-specific robot controls can come after the motion language settles.',
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
              ],
            ),
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: ElevatedButton.icon(
                  onPressed: controller.busy ? null : _openAppearanceEditor,
                  icon: const Icon(Icons.tune),
                  label: const Text('Edit studio'),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: controller.busy || !controller.canControlDevice
                      ? null
                      : () => usesNativeClassicSend
                          ? _sendCompanionStyle(controller)
                          : _toggleStudioSceneSend(
                              controller,
                              currentPersonality,
                              currentPetMode,
                            ),
                  icon: Icon(
                    usesNativeClassicSend
                        ? Icons.face_retouching_natural
                      : streamsLiveBleScene
                            ? (_scenePlaybackActive
                                ? Icons.stop_circle_outlined
                                : Icons.play_circle_outline)
                            : Icons.monitor_outlined,
                  ),
                  label: Text(
                    usesNativeClassicSend
                        ? 'Apply appearance'
                      : streamsLiveBleScene
                            ? (_scenePlaybackActive
                                ? 'Stop live scene'
                                : 'Start live scene')
                            : 'Send scene snapshot',
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          Text(
            usesNativeClassicSend
                ? 'Classic appearance sends as a native device command.'
                : streamsLiveBleScene
                    ? 'The app is now streaming the full scene animation over BLE instead of flattening it to a still frame.'
                    : 'Connect over BLE to send this app-rendered scene. Live scene streaming also requires BLE.',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ],
      ),
    );
  }

  Widget _buildExpressionStudioSection(
    BuildContext context,
    DeskCompanionController controller,
    DeskPersonality currentPersonality,
    DeskPetMode currentPetMode,
  ) {
    return _SectionCard(
      title: 'Expression studio',
      subtitle:
          'Live expression preview is back in Studio, with a dedicated editor for browsing moods before you send them.',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            width: double.infinity,
            padding: const EdgeInsets.all(14),
            decoration: BoxDecoration(
              color: CompanionTheme.panel,
              borderRadius: BorderRadius.circular(20),
              border: Border.all(color: CompanionTheme.blush),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'Live preview',
                  style: Theme.of(context).textTheme.titleSmall,
                ),
                const SizedBox(height: 8),
                DecoratedBox(
                  decoration: BoxDecoration(
                    color: CompanionTheme.ink,
                    borderRadius: BorderRadius.circular(18),
                  ),
                  child: Padding(
                    padding: const EdgeInsets.all(12),
                    child: CompanionFacePreview(
                      visualModel: CompanionVisualModel.classic,
                      scene: CompanionScene.none,
                      personality: currentPersonality.command,
                      petMode: currentPetMode.command,
                      referencePose: false,
                      showScreenBoundary: true,
                      expression: _selectedExpression.command,
                      hair: _selectedHairStyle.command,
                      ears: _selectedEarsStyle.command,
                      mustache: _selectedMustacheStyle.command,
                      glasses: _selectedGlassesStyle.command,
                      headwear: _selectedHeadwearStyle.command,
                      piercing: _selectedPiercingStyle.command,
                      hairSize: _selectedHairSize.round(),
                      mustacheSize: _selectedMustacheSize.round(),
                      hairWidth: _selectedHairWidth.round(),
                      hairHeight: _selectedHairHeight.round(),
                      hairThickness: _selectedHairThickness.round(),
                      hairOffsetX: _selectedHairOffsetX.round(),
                      hairOffsetY: _selectedHairOffsetY.round(),
                      eyeOffsetY: _selectedEyeOffsetY.round(),
                      mouthOffsetY: _selectedMouthOffsetY.round(),
                      mustacheWidth: _selectedMustacheWidth.round(),
                      mustacheHeight: _selectedMustacheHeight.round(),
                      mustacheThickness: _selectedMustacheThickness.round(),
                      mustacheOffsetX: _selectedMustacheOffsetX.round(),
                      mustacheOffsetY: _selectedMustacheOffsetY.round(),
                    ),
                  ),
                ),
                const SizedBox(height: 10),
                Text(
                  _selectedExpression.subtitle,
                  style: Theme.of(context).textTheme.bodyMedium,
                ),
                const SizedBox(height: 8),
                Text(
                  'Expression sends still use the device-native renderer. Scene mood in Style Studio stays separate from this native expression editor.',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),
          _buildStudioDropdown(
            context,
            label: 'Expression',
            value: _selectedExpression,
            values: DeskExpression.values,
            labelBuilder: (value) => value.label,
            enabled: !controller.busy,
            onChanged: (value) => setState(() => _selectedExpression = value),
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: controller.busy
                      ? null
                      : () => _openExpressionEditor(
                            currentPersonality,
                            currentPetMode,
                          ),
                  icon: const Icon(Icons.tune),
                  label: const Text('Open expression studio'),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: ElevatedButton.icon(
                  onPressed: controller.busy || !controller.canControlDevice
                      ? null
                      : () => _sendExpression(controller),
                  icon: const Icon(Icons.face_retouching_natural),
                  label: Text('Show ${_selectedExpression.label}'),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildStickyNoteSection(
    BuildContext context,
    DeskCompanionController controller,
  ) {
    final noteText = _noteController.text.characters.take(kMaxNoteCharacters).toString();
    return _SectionCard(
      title: 'Sticky note',
      subtitle:
          'Edit notes in the popup customizer with a firmware-style preview.',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            width: double.infinity,
            padding: const EdgeInsets.all(14),
            decoration: BoxDecoration(
              color: CompanionTheme.panel,
              borderRadius: BorderRadius.circular(20),
              border: Border.all(color: CompanionTheme.blush),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    _ChipLabel(label: 'Font ${_noteFontSize.round()}x'),
                    _ChipLabel(label: 'Border $_noteBorderStyle'),
                    _ChipLabel(label: _noteFlowerAccent == null ? 'No flower accent' : _noteFlowerAccent!.replaceAll('_', ' ')),
                    _ChipLabel(label: _noteIcons.isEmpty ? 'No top icons' : _noteIcons.join(', ')),
                  ],
                ),
                const SizedBox(height: 10),
                Text(
                  noteText.trim().isEmpty ? 'Write a note in the popup editor.' : noteText,
                  style: Theme.of(context).textTheme.bodyMedium,
                ),
                const SizedBox(height: 10),
                Center(
                  child: DecoratedBox(
                    decoration: BoxDecoration(
                      color: CompanionTheme.ink,
                      borderRadius: BorderRadius.circular(16),
                    ),
                    child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: SizedBox(
                        width: 128 * 3.0,
                        height: 64 * 3.0,
                        child: FittedBox(
                          fit: BoxFit.fill,
                          child: NoteCardPreview(
                            text: noteText,
                            fontSize: _noteFontSize.round(),
                            border: _noteBorderStyle,
                            icons: List.unmodifiable(_noteIcons),
                            flowerAccent: _noteFlowerAccent,
                          ),
                        ),
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 8),
                Text(
                  _noteFlowerAccent == null
                      ? 'Inline preview mirrors the built-in note renderer, including token width and icon spacing.'
                      : 'Flower accent mode reserves the left side and always renders note text at 1x, matching the device.',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),
          Column(
            children: [
              SizedBox(
                width: double.infinity,
                child: ElevatedButton.icon(
                  onPressed: controller.busy ? null : _openNoteEditor,
                  icon: const Icon(Icons.sticky_note_2_outlined),
                  label: const Text('Edit note'),
                ),
              ),
              const SizedBox(height: 10),
              SizedBox(
                width: double.infinity,
                child: ElevatedButton.icon(
                  onPressed: controller.busy || !controller.canControlDevice
                      ? null
                      : () => _sendNote(controller),
                  icon: const Icon(Icons.favorite_border),
                  label: const Text('Show note'),
                ),
              ),
              const SizedBox(height: 10),
              SizedBox(
                width: double.infinity,
                child: OutlinedButton.icon(
                  onPressed: controller.busy || !controller.canControlDevice
                      ? null
                      : () => _perform(
                            () => controller.clearDisplay(),
                            success: 'Display cleared.',
                          ),
                  icon: const Icon(Icons.cleaning_services_outlined),
                  label: const Text('Clear'),
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }

  Future<void> _sendNote(DeskCompanionController controller) async {
    final text = _noteController.text.characters
        .take(kMaxNoteCharacters)
        .toString()
        .trim();
    if (text.isEmpty) return;
    await _perform(
      () => controller.sendNote(
        text,
        fontSize: _noteFontSize.round(),
        border: _noteBorderStyle,
        icons: _noteIcons.join(','),
        flowerAccent: _noteFlowerAccent ?? '',
      ),
      success: 'Note delivered.',
    );
  }

  Future<void> _sendBanner(DeskCompanionController controller) async {
    await _perform(
      () => controller.sendBanner(
        _bannerController.text.trim(),
        speed: _bannerSpeed.round(),
      ),
      success: 'Banner started.',
    );
  }

  Future<void> _sendExpression(DeskCompanionController controller) async {
    _stopLiveScenePlayback();
    await _perform(
      () => controller.sendExpression(_selectedExpression.command),
      success: '${_selectedExpression.label} expression sent.',
    );
  }

  Future<void> _sendPersonality(DeskCompanionController controller) async {
    await _perform(
      () => controller.setPetPersonality(_selectedPersonality.command),
      success: 'Companion personality updated.',
    );
    _behaviorDraftDirty = false;
  }

  Future<void> _triggerPetMode(DeskCompanionController controller) async {
    await _perform(
      () => controller.triggerPetMode(_selectedPetMode.command),
      success: 'Companion mode updated.',
    );
    _behaviorDraftDirty = false;
  }

  Future<void> _sendCareAction(DeskCompanionController controller) async {
    await _perform(
      () => controller.sendCareAction(_selectedCareAction.command),
      success: '${_selectedCareAction.label} action sent.',
    );
  }

  Future<void> _sendCompanionStyle(DeskCompanionController controller) async {
    await _perform(
      () => controller.setCompanionStyle(
        hair: _selectedHairStyle.command,
        ears: _selectedEarsStyle.command,
        mustache: _selectedMustacheStyle.command,
        glasses: _selectedGlassesStyle.command,
        headwear: _selectedHeadwearStyle.command,
        piercing: _selectedPiercingStyle.command,
        hairSize: _selectedHairSize.round(),
        mustacheSize: _selectedMustacheSize.round(),
        hairWidth: _selectedHairWidth.round(),
        hairHeight: _selectedHairHeight.round(),
        hairThickness: _selectedHairThickness.round(),
        hairOffsetX: _selectedHairOffsetX.round(),
        hairOffsetY: _selectedHairOffsetY.round(),
        eyeOffsetY: _selectedEyeOffsetY.round(),
        mouthOffsetY: _selectedMouthOffsetY.round(),
        mustacheWidth: _selectedMustacheWidth.round(),
        mustacheHeight: _selectedMustacheHeight.round(),
        mustacheThickness: _selectedMustacheThickness.round(),
        mustacheOffsetX: _selectedMustacheOffsetX.round(),
        mustacheOffsetY: _selectedMustacheOffsetY.round(),
      ),
      success: 'Companion appearance applied.',
    );
    _appearanceDraftDirty = false;
  }

  Future<void> _toggleStudioSceneSend(
    DeskCompanionController controller,
    DeskPersonality currentPersonality,
    DeskPetMode currentPetMode,
  ) async {
    if (_selectedVisualModel.isDeviceSupported &&
        _selectedScene == CompanionScene.none) {
      await _sendCompanionStyle(controller);
      return;
    }

    if (!controller.isBleConnected) {
      await _sendStudioPreviewSnapshot(
        controller,
        currentPersonality,
        currentPetMode,
      );
      return;
    }

    if (_appearancePreviewReferencePose) {
      await _sendStudioPreviewSnapshot(
        controller,
        currentPersonality,
        currentPetMode,
      );
      return;
    }

    if (_scenePlaybackActive) {
      _stopLiveScenePlayback();
      _showMessage('Live scene streaming stopped.');
      return;
    }

    setState(() => _scenePlaybackActive = true);
    unawaited(
      _runLiveScenePlayback(
        controller,
        currentPersonality,
        currentPetMode,
      ),
    );
    _showMessage('Streaming the scene live over BLE.');
  }

  Duration _scenePlaybackDuration() {
    if (_appearancePreviewReferencePose) {
      return const Duration(milliseconds: 2400);
    }

    return switch (_selectedVisualModel) {
      CompanionVisualModel.classic => const Duration(milliseconds: 2400),
      CompanionVisualModel.stickFigure => switch (_selectedScene) {
          CompanionScene.holdHands => const Duration(milliseconds: 7200),
          CompanionScene.hug ||
          CompanionScene.kiss ||
          CompanionScene.shyLeanIn => const Duration(milliseconds: 5200),
          CompanionScene.wave ||
          CompanionScene.bow => const Duration(milliseconds: 3600),
          CompanionScene.none => const Duration(milliseconds: 3000),
        },
      CompanionVisualModel.robot => const Duration(milliseconds: 4200),
    };
  }

  Duration _scenePlaybackFrameInterval() {
    if (_appearancePreviewReferencePose) {
      return const Duration(milliseconds: 160);
    }

    switch (_selectedVisualModel) {
      case CompanionVisualModel.classic:
        return const Duration(milliseconds: 90);
      case CompanionVisualModel.stickFigure:
        final energyTrim = ((_stickFigureEnergy.round() - 40) / 15).floor();
        var intervalMs = switch (_selectedScene) {
          CompanionScene.holdHands => 70,
          CompanionScene.hug ||
          CompanionScene.kiss ||
          CompanionScene.shyLeanIn => 76,
          CompanionScene.wave || CompanionScene.bow => 64,
          CompanionScene.none => 78,
        };
        intervalMs -= energyTrim.clamp(0, 3) * 4;
        if (intervalMs < 58) {
          intervalMs = 58;
        }
        return Duration(milliseconds: intervalMs);
      case CompanionVisualModel.robot:
        final intervalMs = switch (_selectedScene) {
          CompanionScene.holdHands ||
          CompanionScene.hug ||
          CompanionScene.kiss ||
          CompanionScene.shyLeanIn => 72,
          CompanionScene.wave || CompanionScene.bow => 66,
          CompanionScene.none => 76,
        };
        return Duration(milliseconds: intervalMs);
    }
  }

  double _currentScenePlaybackProgress() {
    if (_appearancePreviewReferencePose) {
      return 0;
    }
    final durationMs = _scenePlaybackDuration().inMilliseconds;
    if (durationMs <= 0) {
      return 0;
    }
    final elapsedMs = _scenePlaybackClock.elapsedMilliseconds % durationMs;
    return elapsedMs / durationMs;
  }

  void _stopLiveScenePlayback({bool updateUi = true}) {
    _scenePlaybackToken += 1;
    _scenePlaybackClock.stop();
    _scenePlaybackClock.reset();
    if (updateUi && mounted) {
      setState(() => _scenePlaybackActive = false);
    } else {
      _scenePlaybackActive = false;
    }
  }

  Future<void> _runLiveScenePlayback(
    DeskCompanionController controller,
    DeskPersonality currentPersonality,
    DeskPetMode currentPetMode,
  ) async {
    final playbackToken = ++_scenePlaybackToken;
    _scenePlaybackClock
      ..reset()
      ..start();

    while (mounted &&
        _scenePlaybackActive &&
        _scenePlaybackToken == playbackToken &&
        controller.isBleConnected) {
      await _pushLiveSceneFrame(
        controller,
        currentPersonality,
        currentPetMode,
      );
      await Future<void>.delayed(_scenePlaybackFrameInterval());
    }

    if (_scenePlaybackToken == playbackToken) {
      _stopLiveScenePlayback();
    }
  }

  Future<void> _pushLiveSceneFrame(
    DeskCompanionController controller,
    DeskPersonality currentPersonality,
    DeskPetMode currentPetMode,
  ) async {
    final payload = await renderCompanionFacePreviewPayload(
      visualModel: _selectedVisualModel,
      scene: _selectedScene,
      personality: _appearancePreviewReferencePose
          ? _selectedPersonality.command
          : currentPersonality.command,
      petMode: _appearancePreviewReferencePose
          ? _selectedPetMode.command
          : currentPetMode.command,
      referencePose: _appearancePreviewReferencePose,
      expression: _appearancePreviewReferencePose
          ? null
          : _selectedExpression.command,
      hair: _selectedHairStyle.command,
      ears: _selectedEarsStyle.command,
      mustache: _selectedMustacheStyle.command,
      glasses: _selectedGlassesStyle.command,
      headwear: _selectedHeadwearStyle.command,
      piercing: _selectedPiercingStyle.command,
      hairSize: _selectedHairSize.round(),
      mustacheSize: _selectedMustacheSize.round(),
      hairWidth: _selectedHairWidth.round(),
      hairHeight: _selectedHairHeight.round(),
      hairThickness: _selectedHairThickness.round(),
      hairOffsetX: _selectedHairOffsetX.round(),
      hairOffsetY: _selectedHairOffsetY.round(),
      eyeOffsetY: _selectedEyeOffsetY.round(),
      mouthOffsetY: _selectedMouthOffsetY.round(),
      mustacheWidth: _selectedMustacheWidth.round(),
      mustacheHeight: _selectedMustacheHeight.round(),
      mustacheThickness: _selectedMustacheThickness.round(),
      mustacheOffsetX: _selectedMustacheOffsetX.round(),
      mustacheOffsetY: _selectedMustacheOffsetY.round(),
      stickFigureScale: _stickFigureScale.round(),
      stickFigureSpacing: _stickFigureSpacing.round(),
      stickFigureEnergy: _stickFigureEnergy.round(),
      animationProgress: _currentScenePlaybackProgress(),
      name: 'studio_live_${_selectedVisualModel.command}_${_selectedScene.command}',
    );
    await controller.sendLiveBitmap(payload.bitmap);
  }

  Future<void> _openExpressionEditor(
    DeskPersonality currentPersonality,
    DeskPetMode currentPetMode,
  ) async {
    final controller = context.read<DeskCompanionController>();
    final result = await Navigator.of(context).push<_AppearanceEditorResult>(
      MaterialPageRoute(
        builder: (_) => _FullscreenAppearanceEditor(
          editorTitle: 'Expression studio',
            initialSection: _selectedVisualModel == CompanionVisualModel.classic
              ? _AppearanceEditorSection.face
              : _selectedVisualModel == CompanionVisualModel.stickFigure
                ? _AppearanceEditorSection.motion
                : _AppearanceEditorSection.scene,
          personality: currentPersonality,
          petMode: currentPetMode,
          initialVisualModel: _selectedVisualModel,
          initialScene: _selectedScene,
          initialReferencePose: _appearancePreviewReferencePose,
          selectedExpression: _selectedExpression,
          hairStyle: _selectedHairStyle,
          earsStyle: _selectedEarsStyle,
          mustacheStyle: _selectedMustacheStyle,
          glassesStyle: _selectedGlassesStyle,
          headwearStyle: _selectedHeadwearStyle,
          piercingStyle: _selectedPiercingStyle,
          hairSize: _selectedHairSize,
          hairWidth: _selectedHairWidth,
          hairHeight: _selectedHairHeight,
          hairThickness: _selectedHairThickness,
          hairOffsetX: _selectedHairOffsetX,
          hairOffsetY: _selectedHairOffsetY,
          eyeOffsetY: _selectedEyeOffsetY,
          mouthOffsetY: _selectedMouthOffsetY,
          mustacheSize: _selectedMustacheSize,
          mustacheWidth: _selectedMustacheWidth,
          mustacheHeight: _selectedMustacheHeight,
          mustacheThickness: _selectedMustacheThickness,
          mustacheOffsetX: _selectedMustacheOffsetX,
          mustacheOffsetY: _selectedMustacheOffsetY,
          stickFigureScale: _stickFigureScale,
          stickFigureSpacing: _stickFigureSpacing,
          stickFigureEnergy: _stickFigureEnergy,
        ),
        fullscreenDialog: true,
      ),
    );

    if (!mounted || result == null) {
      return;
    }

    setState(() {
      _appearanceDraftDirty = true;
      _selectedVisualModel = result.visualModel;
      _selectedScene = result.scene;
      _appearancePreviewReferencePose = result.previewReferencePose;
      _selectedExpression = result.expression;
      _selectedHairStyle = result.hairStyle;
      _selectedEarsStyle = result.earsStyle;
      _selectedMustacheStyle = result.mustacheStyle;
      _selectedGlassesStyle = result.glassesStyle;
      _selectedHeadwearStyle = result.headwearStyle;
      _selectedPiercingStyle = result.piercingStyle;
      _selectedHairSize = result.hairSize;
      _selectedHairWidth = result.hairWidth;
      _selectedHairHeight = result.hairHeight;
      _selectedHairThickness = result.hairThickness;
      _selectedHairOffsetX = result.hairOffsetX;
      _selectedHairOffsetY = result.hairOffsetY;
      _selectedEyeOffsetY = result.eyeOffsetY;
      _selectedMouthOffsetY = result.mouthOffsetY;
      _selectedMustacheSize = result.mustacheSize;
      _selectedMustacheWidth = result.mustacheWidth;
      _selectedMustacheHeight = result.mustacheHeight;
      _selectedMustacheThickness = result.mustacheThickness;
      _selectedMustacheOffsetX = result.mustacheOffsetX;
      _selectedMustacheOffsetY = result.mustacheOffsetY;
      _stickFigureScale = result.stickFigureScale;
      _stickFigureSpacing = result.stickFigureSpacing;
      _stickFigureEnergy = result.stickFigureEnergy;
    });
    _persistStudioPreviewState(controller);
  }

  Future<void> _sendStudioPreviewSnapshot(
    DeskCompanionController controller,
    DeskPersonality currentPersonality,
    DeskPetMode currentPetMode,
  ) async {
    final payload = await renderCompanionFacePreviewPayload(
      visualModel: _selectedVisualModel,
      scene: _selectedScene,
      personality: _appearancePreviewReferencePose
          ? _selectedPersonality.command
          : currentPersonality.command,
      petMode: _appearancePreviewReferencePose
          ? _selectedPetMode.command
          : currentPetMode.command,
      referencePose: _appearancePreviewReferencePose,
      expression: _appearancePreviewReferencePose
          ? null
          : _selectedExpression.command,
      hair: _selectedHairStyle.command,
      ears: _selectedEarsStyle.command,
      mustache: _selectedMustacheStyle.command,
      glasses: _selectedGlassesStyle.command,
      headwear: _selectedHeadwearStyle.command,
      piercing: _selectedPiercingStyle.command,
      hairSize: _selectedHairSize.round(),
      mustacheSize: _selectedMustacheSize.round(),
      hairWidth: _selectedHairWidth.round(),
      hairHeight: _selectedHairHeight.round(),
      hairThickness: _selectedHairThickness.round(),
      hairOffsetX: _selectedHairOffsetX.round(),
      hairOffsetY: _selectedHairOffsetY.round(),
      eyeOffsetY: _selectedEyeOffsetY.round(),
      mouthOffsetY: _selectedMouthOffsetY.round(),
      mustacheWidth: _selectedMustacheWidth.round(),
      mustacheHeight: _selectedMustacheHeight.round(),
      mustacheThickness: _selectedMustacheThickness.round(),
      mustacheOffsetX: _selectedMustacheOffsetX.round(),
      mustacheOffsetY: _selectedMustacheOffsetY.round(),
      stickFigureScale: _stickFigureScale.round(),
      stickFigureSpacing: _stickFigureSpacing.round(),
      stickFigureEnergy: _stickFigureEnergy.round(),
      name: 'studio_${_selectedVisualModel.command}_${_selectedScene.command}',
    );

    await _perform(
      () => controller.sendImage(payload),
      success: 'Scene snapshot delivered to the OLED.',
    );
  }

  Future<void> _sendCanvas(DeskCompanionController controller) async {
    final payload = OledBitmapCodec.fromBitmap(
      bitmap: _drawBitmap,
      name: 'oled_drawing',
    );
    await _perform(
      () => controller.sendImage(payload),
      success: 'Drawing delivered.',
    );
  }

  Future<void> _sendImage(DeskCompanionController controller) async {
    final payload = _selectedImage;
    if (payload == null) {
      return;
    }

    await _perform(
      () => controller.sendImage(payload),
      success: 'Image delivered.',
    );
  }

  Future<void> _perform(Future<void> Function() action,
      {required String success}) async {
    try {
      await action();
      if (!mounted) {
        return;
      }
      _showMessage(success);
    } catch (error) {
      if (!mounted) {
        return;
      }
      _showMessage('$error');
    }
  }

  void _showMessage(String message) {
    ScaffoldMessenger.of(context)
        .showSnackBar(SnackBar(content: Text(message)));
  }

  DeskPersonality? _personalityFromCommand(String value) {
    for (final personality in DeskPersonality.values) {
      if (personality.command == value.trim()) {
        return personality;
      }
    }
    return null;
  }

  DeskPetMode? _petModeFromCommand(String value) {
    for (final petMode in DeskPetMode.values) {
      if (petMode.command == value.trim()) {
        return petMode;
      }
    }
    return null;
  }

  DeskHairStyle? _hairStyleFromCommand(String value) {
    for (final hair in DeskHairStyle.values) {
      if (hair.command == value.trim()) {
        return hair;
      }
    }
    return null;
  }

  DeskEarsStyle? _earsStyleFromCommand(String value) {
    for (final ears in DeskEarsStyle.values) {
      if (ears.command == value.trim()) {
        return ears;
      }
    }
    return null;
  }

  DeskMustacheStyle? _mustacheStyleFromCommand(String value) {
    for (final mustache in DeskMustacheStyle.values) {
      if (mustache.command == value.trim()) {
        return mustache;
      }
    }
    return null;
  }

  DeskGlassesStyle? _glassesStyleFromCommand(String value) {
    for (final glasses in DeskGlassesStyle.values) {
      if (glasses.command == value.trim()) {
        return glasses;
      }
    }
    return null;
  }

  DeskHeadwearStyle? _headwearStyleFromCommand(String value) {
    for (final headwear in DeskHeadwearStyle.values) {
      if (headwear.command == value.trim()) {
        return headwear;
      }
    }
    return null;
  }

  DeskPiercingStyle? _piercingStyleFromCommand(String value) {
    for (final piercing in DeskPiercingStyle.values) {
      if (piercing.command == value.trim()) {
        return piercing;
      }
    }
    return null;
  }
}

class _SectionCard extends StatelessWidget {
  const _SectionCard({
    required this.title,
    required this.subtitle,
    required this.child,
  });

  final String title;
  final String subtitle;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 4),
            Text(subtitle, style: Theme.of(context).textTheme.bodyMedium),
            const SizedBox(height: 14),
            child,
          ],
        ),
      ),
    );
  }
}

class _ChipLabel extends StatelessWidget {
  const _ChipLabel({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: CompanionTheme.surface,
        borderRadius: BorderRadius.circular(999),
        border: Border.all(color: CompanionTheme.blush),
      ),
      child: Text(label, style: Theme.of(context).textTheme.bodySmall),
    );
  }
}

class _StudioGroup extends StatelessWidget {
  const _StudioGroup({
    required this.title,
    required this.subtitle,
    required this.child,
  });

  final String title;
  final String subtitle;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: CompanionTheme.surface,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: CompanionTheme.blush),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(title, style: Theme.of(context).textTheme.titleSmall),
          const SizedBox(height: 4),
          Text(subtitle, style: Theme.of(context).textTheme.bodyMedium),
          const SizedBox(height: 12),
          child,
        ],
      ),
    );
  }
}

class _FullscreenDrawResult {
  const _FullscreenDrawResult({
    required this.bitmap,
    required this.showGrid,
    required this.liveDraw,
    required this.eraserMode,
    required this.brushSize,
  });

  final Uint8List bitmap;
  final bool showGrid;
  final bool liveDraw;
  final bool eraserMode;
  final double brushSize;
}

class _FullscreenDrawEditor extends StatefulWidget {
  const _FullscreenDrawEditor({
    required this.bitmap,
    required this.showGrid,
    required this.liveDraw,
    required this.eraserMode,
    required this.brushSize,
  });

  final Uint8List bitmap;
  final bool showGrid;
  final bool liveDraw;
  final bool eraserMode;
  final double brushSize;

  @override
  State<_FullscreenDrawEditor> createState() => _FullscreenDrawEditorState();
}

class _FullscreenDrawEditorState extends State<_FullscreenDrawEditor> {
  late Uint8List _bitmap;
  late bool _showGrid;
  late bool _liveDraw;
  late bool _eraserMode;
  late double _brushSize;
  bool _controlsVisible = false;
  Timer? _liveSyncTimer;

  @override
  void initState() {
    super.initState();
    _bitmap = Uint8List.fromList(widget.bitmap);
    _showGrid = widget.showGrid;
    _liveDraw = widget.liveDraw;
    _eraserMode = widget.eraserMode;
    _brushSize = widget.brushSize;
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
    SystemChrome.setPreferredOrientations(const [
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
  }

  @override
  void dispose() {
    _liveSyncTimer?.cancel();
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
    SystemChrome.setPreferredOrientations(const [
      DeviceOrientation.portraitUp,
    ]);
    super.dispose();
  }

  Widget _buildEditorContent(DeskCompanionController controller) {
    return Stack(
      children: [
        Positioned.fill(
          child: Container(color: CompanionTheme.ink),
        ),
        Positioned.fill(
          child: Padding(
            padding:
                EdgeInsets.fromLTRB(12, 12, 12, _controlsVisible ? 150 : 12),
            child: Center(
              child: FractionallySizedBox(
                widthFactor: 1,
                heightFactor: 1,
                child: OledDrawingPad(
                  bitmap: _bitmap,
                  showGrid: _showGrid,
                  enabled: true,
                  onPixel: _paintPixel,
                ),
              ),
            ),
          ),
        ),
        Positioned(
          top: 12,
          right: 12,
          child: SafeArea(
            bottom: false,
            child: Align(
              alignment: Alignment.topRight,
              child: IconButton.filledTonal(
                onPressed: () =>
                    setState(() => _controlsVisible = !_controlsVisible),
                icon: Icon(_controlsVisible ? Icons.menu_open : Icons.tune),
                tooltip: _controlsVisible ? 'Hide controls' : 'Show controls',
              ),
            ),
          ),
        ),
        if (_controlsVisible)
          Positioned(
            left: 12,
            right: 12,
            bottom: 12,
            child: SafeArea(
              top: false,
              child: Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: CompanionTheme.cream.withValues(alpha: 0.94),
                  borderRadius: BorderRadius.circular(20),
                  boxShadow: const [
                    BoxShadow(
                      color: Color(0x33000000),
                      blurRadius: 20,
                      offset: Offset(0, 8),
                    ),
                  ],
                ),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Row(
                      children: [
                        IconButton.filledTonal(
                          onPressed: () => Navigator.of(context).maybePop(),
                          icon: const Icon(Icons.close),
                          tooltip: 'Close',
                        ),
                        const SizedBox(width: 8),
                        IconButton.filled(
                          onPressed: _close,
                          icon: const Icon(Icons.check),
                          tooltip: 'Save and close',
                        ),
                        const Spacer(),
                        Text(
                          'Drawing controls',
                          style: Theme.of(context).textTheme.titleSmall,
                        ),
                      ],
                    ),
                    const SizedBox(height: 8),
                    Wrap(
                      spacing: 10,
                      runSpacing: 10,
                      children: [
                        FilterChip(
                          label: const Text('Pen'),
                          selected: !_eraserMode,
                          onSelected: (_) =>
                              setState(() => _eraserMode = false),
                        ),
                        FilterChip(
                          label: const Text('Eraser'),
                          selected: _eraserMode,
                          onSelected: (_) => setState(() => _eraserMode = true),
                        ),
                        FilterChip(
                          label: const Text('Grid'),
                          selected: _showGrid,
                          onSelected: (_) =>
                              setState(() => _showGrid = !_showGrid),
                        ),
                        FilterChip(
                          label: const Text('Live push'),
                          selected: _liveDraw,
                          onSelected: (_) =>
                              setState(() => _liveDraw = !_liveDraw),
                        ),
                      ],
                    ),
                    const SizedBox(height: 8),
                    Text(
                      'Brush size: ${_brushSize.round()} px',
                      style: Theme.of(context).textTheme.bodyMedium,
                    ),
                    Slider(
                      min: 1,
                      max: 5,
                      divisions: 4,
                      value: _brushSize,
                      onChanged: (value) => setState(() => _brushSize = value),
                    ),
                    Row(
                      children: [
                        Expanded(
                          child: OutlinedButton.icon(
                            onPressed: controller.busy
                                ? null
                                : () {
                                    setState(() => _bitmap =
                                        Uint8List(OledBitmapCodec.byteLength));
                                    _queueLiveDraw();
                                  },
                            icon: const Icon(Icons.layers_clear_outlined),
                            label: const Text('Clear'),
                          ),
                        ),
                        const SizedBox(width: 10),
                        Expanded(
                          child: ElevatedButton.icon(
                            onPressed: controller.busy
                                ? null
                                : () {
                                    final payload = OledBitmapCodec.fromBitmap(
                                      bitmap: _bitmap,
                                      name: 'oled_drawing',
                                    );
                                    controller.sendImage(payload);
                                  },
                            icon: const Icon(Icons.draw_outlined),
                            label: const Text('Push drawing'),
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
          ),
      ],
    );
  }

  void _paintPixel(int x, int y) {
    final next = Uint8List.fromList(_bitmap);
    final radius = (_brushSize.round() - 1).clamp(0, 4);
    for (var py = y - radius; py <= y + radius; py++) {
      if (py < 0 || py >= 64) {
        continue;
      }
      for (var px = x - radius; px <= x + radius; px++) {
        if (px < 0 || px >= 128) {
          continue;
        }
        final byteIndex = py * 16 + (px >> 3);
        final bitMask = 1 << (7 - (px & 7));
        if (_eraserMode) {
          next[byteIndex] &= ~bitMask;
        } else {
          next[byteIndex] |= bitMask;
        }
      }
    }

    setState(() => _bitmap = next);
    _queueLiveDraw();
  }

  void _queueLiveDraw() {
    if (!_liveDraw) {
      return;
    }
    _liveSyncTimer?.cancel();
    _liveSyncTimer = Timer(const Duration(milliseconds: 120), () {
      if (!mounted) {
        return;
      }
      context.read<DeskCompanionController>().sendLiveBitmap(_bitmap);
    });
  }

  void _close() {
    Navigator.of(context).pop(
      _FullscreenDrawResult(
        bitmap: _bitmap,
        showGrid: _showGrid,
        liveDraw: _liveDraw,
        eraserMode: _eraserMode,
        brushSize: _brushSize,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final controller = context.watch<DeskCompanionController>();
    final mediaQuery = MediaQuery.of(context);
    final isPortrait = mediaQuery.size.height > mediaQuery.size.width;
    final editorContent = _buildEditorContent(controller);

    return Scaffold(
      backgroundColor: CompanionTheme.ink,
      body: SafeArea(
        child: isPortrait
            ? Center(
                child: RotatedBox(
                  quarterTurns: 1,
                  child: SizedBox(
                    width: mediaQuery.size.height,
                    height: mediaQuery.size.width,
                    child: editorContent,
                  ),
                ),
              )
            : editorContent,
      ),
    );
  }
}

enum _AppearanceEditorSection { scene, silhouette, face, mustache, motion }

extension _AppearanceEditorSectionExt on _AppearanceEditorSection {
  String get label => switch (this) {
        _AppearanceEditorSection.scene => 'Scene',
        _AppearanceEditorSection.silhouette => 'Top layer',
        _AppearanceEditorSection.face => 'Face',
        _AppearanceEditorSection.mustache => 'Mustache',
        _AppearanceEditorSection.motion => 'Motion',
      };

  IconData get icon => switch (this) {
        _AppearanceEditorSection.scene => Icons.movie_filter_outlined,
        _AppearanceEditorSection.silhouette => Icons.style_outlined,
        _AppearanceEditorSection.face => Icons.face_retouching_natural,
        _AppearanceEditorSection.mustache => Icons.brush_outlined,
        _AppearanceEditorSection.motion => Icons.animation_outlined,
      };
}

class _AppearanceEditorResult {
  const _AppearanceEditorResult({
    required this.visualModel,
    required this.scene,
    required this.previewReferencePose,
    required this.expression,
    required this.hairStyle,
    required this.earsStyle,
    required this.mustacheStyle,
    required this.glassesStyle,
    required this.headwearStyle,
    required this.piercingStyle,
    required this.hairSize,
    required this.hairWidth,
    required this.hairHeight,
    required this.hairThickness,
    required this.hairOffsetX,
    required this.hairOffsetY,
    required this.eyeOffsetY,
    required this.mouthOffsetY,
    required this.mustacheSize,
    required this.mustacheWidth,
    required this.mustacheHeight,
    required this.mustacheThickness,
    required this.mustacheOffsetX,
    required this.mustacheOffsetY,
    required this.stickFigureScale,
    required this.stickFigureSpacing,
    required this.stickFigureEnergy,
  });

  final CompanionVisualModel visualModel;
  final CompanionScene scene;
  final bool previewReferencePose;
  final DeskExpression expression;
  final DeskHairStyle hairStyle;
  final DeskEarsStyle earsStyle;
  final DeskMustacheStyle mustacheStyle;
  final DeskGlassesStyle glassesStyle;
  final DeskHeadwearStyle headwearStyle;
  final DeskPiercingStyle piercingStyle;
  final double hairSize;
  final double hairWidth;
  final double hairHeight;
  final double hairThickness;
  final double hairOffsetX;
  final double hairOffsetY;
  final double eyeOffsetY;
  final double mouthOffsetY;
  final double mustacheSize;
  final double mustacheWidth;
  final double mustacheHeight;
  final double mustacheThickness;
  final double mustacheOffsetX;
  final double mustacheOffsetY;
  final double stickFigureScale;
  final double stickFigureSpacing;
  final double stickFigureEnergy;
}

class _NoteEditorResult {
  const _NoteEditorResult({
    required this.text,
    required this.fontSize,
    required this.borderStyle,
    required this.icons,
    required this.flowerAccent,
  });

  final String text;
  final double fontSize;
  final int borderStyle;
  final List<String> icons;
  final String? flowerAccent;
}

class _FullscreenAppearanceEditor extends StatefulWidget {
  const _FullscreenAppearanceEditor({
    this.editorTitle = 'Appearance editor',
    this.initialSection,
    required this.personality,
    required this.petMode,
    required this.initialVisualModel,
    required this.initialScene,
    required this.initialReferencePose,
    required this.selectedExpression,
    required this.hairStyle,
    required this.earsStyle,
    required this.mustacheStyle,
    required this.glassesStyle,
    required this.headwearStyle,
    required this.piercingStyle,
    required this.hairSize,
    required this.hairWidth,
    required this.hairHeight,
    required this.hairThickness,
    required this.hairOffsetX,
    required this.hairOffsetY,
    required this.eyeOffsetY,
    required this.mouthOffsetY,
    required this.mustacheSize,
    required this.mustacheWidth,
    required this.mustacheHeight,
    required this.mustacheThickness,
    required this.mustacheOffsetX,
    required this.mustacheOffsetY,
    required this.stickFigureScale,
    required this.stickFigureSpacing,
    required this.stickFigureEnergy,
  });

  final String editorTitle;
  final _AppearanceEditorSection? initialSection;
  final DeskPersonality personality;
  final DeskPetMode petMode;
  final CompanionVisualModel initialVisualModel;
  final CompanionScene initialScene;
  final bool initialReferencePose;
  final DeskExpression selectedExpression;
  final DeskHairStyle hairStyle;
  final DeskEarsStyle earsStyle;
  final DeskMustacheStyle mustacheStyle;
  final DeskGlassesStyle glassesStyle;
  final DeskHeadwearStyle headwearStyle;
  final DeskPiercingStyle piercingStyle;
  final double hairSize;
  final double hairWidth;
  final double hairHeight;
  final double hairThickness;
  final double hairOffsetX;
  final double hairOffsetY;
  final double eyeOffsetY;
  final double mouthOffsetY;
  final double mustacheSize;
  final double mustacheWidth;
  final double mustacheHeight;
  final double mustacheThickness;
  final double mustacheOffsetX;
  final double mustacheOffsetY;
  final double stickFigureScale;
  final double stickFigureSpacing;
  final double stickFigureEnergy;

  @override
  State<_FullscreenAppearanceEditor> createState() =>
      _FullscreenAppearanceEditorState();
}

class _FullscreenAppearanceEditorState
    extends State<_FullscreenAppearanceEditor> {
  late CompanionVisualModel _visualModel;
  late CompanionScene _scene;
  late _AppearanceEditorSection _activeSection;
  late bool _referencePose;
  late DeskExpression _selectedExpression;
  late DeskHairStyle _hairStyle;
  late DeskEarsStyle _earsStyle;
  late DeskMustacheStyle _mustacheStyle;
  late DeskGlassesStyle _glassesStyle;
  late DeskHeadwearStyle _headwearStyle;
  late DeskPiercingStyle _piercingStyle;
  late double _hairSize;
  late double _hairWidth;
  late double _hairHeight;
  late double _hairThickness;
  late double _hairOffsetX;
  late double _hairOffsetY;
  late double _eyeOffsetY;
  late double _mouthOffsetY;
  late double _mustacheSize;
  late double _mustacheWidth;
  late double _mustacheHeight;
  late double _mustacheThickness;
  late double _mustacheOffsetX;
  late double _mustacheOffsetY;
  late double _stickFigureScale;
  late double _stickFigureSpacing;
  late double _stickFigureEnergy;

  @override
  void initState() {
    super.initState();
    _visualModel = widget.initialVisualModel;
    _scene = widget.initialScene;
    _activeSection = widget.initialSection ?? _defaultSectionForModel(_visualModel);
    _referencePose = widget.initialReferencePose;
    _selectedExpression = widget.selectedExpression;
    _hairStyle = widget.hairStyle;
    _earsStyle = widget.earsStyle;
    _mustacheStyle = widget.mustacheStyle;
    _glassesStyle = widget.glassesStyle;
    _headwearStyle = widget.headwearStyle;
    _piercingStyle = widget.piercingStyle;
    _hairSize = widget.hairSize;
    _hairWidth = widget.hairWidth;
    _hairHeight = widget.hairHeight;
    _hairThickness = widget.hairThickness;
    _hairOffsetX = widget.hairOffsetX;
    _hairOffsetY = widget.hairOffsetY;
    _eyeOffsetY = widget.eyeOffsetY;
    _mouthOffsetY = widget.mouthOffsetY;
    _mustacheSize = widget.mustacheSize;
    _mustacheWidth = widget.mustacheWidth;
    _mustacheHeight = widget.mustacheHeight;
    _mustacheThickness = widget.mustacheThickness;
    _mustacheOffsetX = widget.mustacheOffsetX;
    _mustacheOffsetY = widget.mustacheOffsetY;
    _stickFigureScale = widget.stickFigureScale;
    _stickFigureSpacing = widget.stickFigureSpacing;
    _stickFigureEnergy = widget.stickFigureEnergy;
  }

  List<_AppearanceEditorSection> get _availableSections => switch (_visualModel) {
        CompanionVisualModel.classic => const [
            _AppearanceEditorSection.scene,
            _AppearanceEditorSection.silhouette,
            _AppearanceEditorSection.face,
            _AppearanceEditorSection.mustache,
          ],
        CompanionVisualModel.stickFigure => const [
            _AppearanceEditorSection.scene,
            _AppearanceEditorSection.motion,
          ],
        CompanionVisualModel.robot => const [_AppearanceEditorSection.scene],
      };

  _AppearanceEditorSection _defaultSectionForModel(CompanionVisualModel model) {
    return switch (model) {
      CompanionVisualModel.classic => _AppearanceEditorSection.silhouette,
      CompanionVisualModel.stickFigure => _AppearanceEditorSection.motion,
      CompanionVisualModel.robot => _AppearanceEditorSection.scene,
    };
  }

  void _setVisualModel(CompanionVisualModel value) {
    if (_visualModel == value) {
      return;
    }

    setState(() {
      _visualModel = value;
      final availableSections = _availableSections;
      if (!availableSections.contains(_activeSection)) {
        _activeSection = _defaultSectionForModel(value);
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.editorTitle),
        actions: [
          TextButton(
            onPressed: _close,
            child: const Text('Save'),
          ),
        ],
      ),
      body: SafeArea(
        child: LayoutBuilder(
          builder: (context, constraints) {
            final previewPadding = EdgeInsets.fromLTRB(
              16,
              12,
              16,
              constraints.maxHeight < 700 ? 10 : 14,
            );
            return Column(
              children: [
                Padding(
                  padding: previewPadding,
                  child: _buildPinnedPreview(context),
                ),
                if (_availableSections.length > 1)
                  Padding(
                    padding: const EdgeInsets.fromLTRB(16, 0, 16, 10),
                    child: _buildSectionSwitcher(context),
                  ),
                Expanded(
                  child: AnimatedSwitcher(
                    duration: const Duration(milliseconds: 220),
                    child: SingleChildScrollView(
                      key: ValueKey('${_visualModel.name}-${_activeSection.name}'),
                      padding: const EdgeInsets.fromLTRB(16, 0, 16, 24),
                      child: _buildSectionContent(context),
                    ),
                  ),
                ),
              ],
            );
          },
        ),
      ),
    );
  }

  Widget _buildPinnedPreview(BuildContext context) {
    final supportLabel = _visualModel.isDeviceSupported
      ? 'Device-ready renderer'
      : 'Live BLE renderer';
    final moodLabel = _referencePose
        ? 'Reference pose keeps placement locked while you edit.'
        : 'Live mood previews ${_selectedExpression.label.toLowerCase()} inside the current scene.';
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: CompanionTheme.panel,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: CompanionTheme.blush),
        boxShadow: const [
          BoxShadow(
            color: Color(0x12000000),
            blurRadius: 18,
            offset: Offset(0, 6),
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Live preview', style: Theme.of(context).textTheme.titleSmall),
                    const SizedBox(height: 2),
                    Text(
                      '$supportLabel • ${_visualModel.label} • Scene ${_scene.label}',
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          DecoratedBox(
            decoration: BoxDecoration(
              color: CompanionTheme.ink,
              borderRadius: BorderRadius.circular(18),
            ),
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: CompanionFacePreview(
                visualModel: _visualModel,
                scene: _scene,
                personality: widget.personality.command,
                petMode: widget.petMode.command,
                referencePose: _referencePose,
                showScreenBoundary: true,
                expression: _referencePose ? null : _selectedExpression.command,
                hair: _hairStyle.command,
                ears: _earsStyle.command,
                mustache: _mustacheStyle.command,
                glasses: _glassesStyle.command,
                headwear: _headwearStyle.command,
                piercing: _piercingStyle.command,
                hairSize: _hairSize.round(),
                mustacheSize: _mustacheSize.round(),
                hairWidth: _hairWidth.round(),
                hairHeight: _hairHeight.round(),
                hairThickness: _hairThickness.round(),
                hairOffsetX: _hairOffsetX.round(),
                hairOffsetY: _hairOffsetY.round(),
                eyeOffsetY: _eyeOffsetY.round(),
                mouthOffsetY: _mouthOffsetY.round(),
                mustacheWidth: _mustacheWidth.round(),
                mustacheHeight: _mustacheHeight.round(),
                mustacheThickness: _mustacheThickness.round(),
                mustacheOffsetX: _mustacheOffsetX.round(),
                mustacheOffsetY: _mustacheOffsetY.round(),
                stickFigureScale: _stickFigureScale.round(),
                stickFigureSpacing: _stickFigureSpacing.round(),
                stickFigureEnergy: _stickFigureEnergy.round(),
              ),
            ),
          ),
          const SizedBox(height: 12),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              ChoiceChip(
                label: const Text('Reference pose'),
                selected: _referencePose,
                onSelected: (_) => setState(() => _referencePose = true),
              ),
              ChoiceChip(
                label: const Text('Live mood'),
                selected: !_referencePose,
                onSelected: (_) => setState(() => _referencePose = false),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Text(
            'Guide frame marks the real OLED area. $moodLabel',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ],
      ),
    );
  }

  Widget _buildSectionSwitcher(BuildContext context) {
    return SingleChildScrollView(
      scrollDirection: Axis.horizontal,
      child: Row(
        children: _availableSections
            .map(
              (section) => Padding(
                padding: const EdgeInsets.only(right: 8),
                child: ChoiceChip(
                  avatar: Icon(section.icon, size: 18),
                  label: Text(section.label),
                  selected: _activeSection == section,
                  onSelected: (_) => setState(() => _activeSection = section),
                ),
              ),
            )
            .toList(growable: false),
      ),
    );
  }

  Widget _buildSectionContent(BuildContext context) {
    return Column(
      key: ValueKey(_activeSection),
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        switch (_activeSection) {
          _AppearanceEditorSection.scene => _buildScenePanel(context),
          _AppearanceEditorSection.silhouette => _buildSilhouettePanel(context),
          _AppearanceEditorSection.face => _buildFacePanel(context),
          _AppearanceEditorSection.mustache => _buildMustachePanel(context),
          _AppearanceEditorSection.motion => _buildMotionPanel(context),
        },
      ],
    );
  }

  Widget _buildScenePanel(BuildContext context) {
    return _EditorPanel(
      title: 'Scene setup',
      subtitle: _visualModel == CompanionVisualModel.classic
          ? 'Classic keeps the scene page intentionally sparse so accessory and face edits do not get buried under irrelevant controls.'
          : _visualModel == CompanionVisualModel.stickFigure
              ? 'Pick the relationship scene and scene mood here. Preview mode stays pinned above so alignment work is always one tap away.'
              : 'Robot currently focuses on scene selection only. Extra robot-only mood controls stay hidden until they have a real effect.',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _buildChoiceWrap(
            context,
            'Visual model',
            CompanionVisualModel.values,
            _visualModel,
            _setVisualModel,
            (value) => value.label,
          ),
          const SizedBox(height: 12),
          Text(
            _visualModel.description,
            style: Theme.of(context).textTheme.bodySmall,
          ),
          if (_visualModel != CompanionVisualModel.classic) ...[
            const SizedBox(height: 14),
            _buildChoiceWrap(
              context,
              'Scene',
              CompanionScene.values,
              _scene,
              (value) => setState(() => _scene = value),
              (value) => value.label,
            ),
            const SizedBox(height: 8),
            Text(
              _scene.description,
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ] else ...[
            const SizedBox(height: 14),
            Text(
              'Classic mode does not use relationship scenes here. Preview mode stays pinned above, and mood changes belong in Expression studio.',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ],
          if (_visualModel == CompanionVisualModel.stickFigure) ...[
            const SizedBox(height: 14),
            _buildChoiceWrap(
              context,
              'Expression / mood',
              DeskExpression.values,
              _selectedExpression,
              (value) => setState(() {
                _selectedExpression = value;
                if (_referencePose) {
                  _referencePose = false;
                }
              }),
              (value) => value.label,
            ),
            const SizedBox(height: 10),
            Text(
              _referencePose
                  ? 'Reference pose is best for silhouette alignment and fixed accessory placement.'
                  : 'Live mood is previewing ${_selectedExpression.label.toLowerCase()} inside the active stick-figure scene.',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ] else if (_visualModel == CompanionVisualModel.robot) ...[
            const SizedBox(height: 14),
            Text(
              'Robot stays scene-first for now. The preview chips above still let you freeze the pose when you need alignment work.',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ],
          const SizedBox(height: 10),
          Text(
            _visualModel.isDeviceSupported
                ? 'This model is ready to send directly to the desk companion.'
                : 'This model is app-rendered. Over BLE it can stream live to the OLED or send as a single snapshot.',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ],
      ),
    );
  }

  Widget _buildSilhouettePanel(BuildContext context) {
    return _EditorPanel(
      title: 'Top layer',
      subtitle: 'Shape the silhouette first, then move on. Everything unrelated stays out of the way.',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _buildChoiceWrap(
            context,
            'Hair',
            DeskHairStyle.values,
            _hairStyle,
            (value) => setState(() => _hairStyle = value),
            (value) => value.label,
          ),
          const SizedBox(height: 12),
          _buildChoiceWrap(
            context,
            'Ears',
            DeskEarsStyle.values,
            _earsStyle,
            (value) => setState(() => _earsStyle = value),
            (value) => value.label,
          ),
          const SizedBox(height: 12),
          _buildChoiceWrap(
            context,
            'Headwear',
            DeskHeadwearStyle.values,
            _headwearStyle,
            (value) => setState(() => _headwearStyle = value),
            (value) => value.label,
          ),
          const SizedBox(height: 12),
          _buildSlider(
            context,
            'Hair size ${_hairSize.round()}%',
            _hairSize,
            (value) => setState(() => _hairSize = value),
          ),
          _buildSlider(
            context,
            'Hair width ${_hairWidth.round()}%',
            _hairWidth,
            (value) => setState(() => _hairWidth = value),
          ),
          _buildSlider(
            context,
            'Hair height ${_hairHeight.round()}%',
            _hairHeight,
            (value) => setState(() => _hairHeight = value),
          ),
          _buildSlider(
            context,
            'Hair thickness ${_hairThickness.round()}%',
            _hairThickness,
            (value) => setState(() => _hairThickness = value),
          ),
          const SizedBox(height: 8),
          _OffsetPad(
            title: 'Hair placement',
            offsetX: _hairOffsetX,
            offsetY: _hairOffsetY,
            onChanged: (x, y) => setState(() {
              _hairOffsetX = x;
              _hairOffsetY = y;
            }),
          ),
        ],
      ),
    );
  }

  Widget _buildFacePanel(BuildContext context) {
    return _EditorPanel(
      title: 'Face details',
      subtitle: 'Classic face alignment lives here: glasses, piercing, and the eye and mouth lines that keep hair from crashing into the expression.',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _buildChoiceWrap(
            context,
            'Glasses',
            DeskGlassesStyle.values,
            _glassesStyle,
            (value) => setState(() => _glassesStyle = value),
            (value) => value.label,
          ),
          const SizedBox(height: 12),
          _buildChoiceWrap(
            context,
            'Piercing',
            DeskPiercingStyle.values,
            _piercingStyle,
            (value) => setState(() => _piercingStyle = value),
            (value) => value.label,
          ),
          const SizedBox(height: 12),
          _buildSlider(
            context,
            'Eye line ${_eyeOffsetY.round()} px',
            _eyeOffsetY,
            (value) => setState(() => _eyeOffsetY = value),
            min: -18,
            max: 18,
            divisions: 36,
          ),
          _buildSlider(
            context,
            'Mouth line ${_mouthOffsetY.round()} px',
            _mouthOffsetY,
            (value) => setState(() => _mouthOffsetY = value),
            min: -18,
            max: 18,
            divisions: 36,
          ),
        ],
      ),
    );
  }

  Widget _buildMustachePanel(BuildContext context) {
    return _EditorPanel(
      title: 'Mustache studio',
      subtitle: 'Keep all mustache shape and placement controls in one place instead of splitting them across the page.',
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          _buildChoiceWrap(
            context,
            'Mustache',
            DeskMustacheStyle.values,
            _mustacheStyle,
            (value) => setState(() => _mustacheStyle = value),
            (value) => value.label,
          ),
          const SizedBox(height: 12),
          _buildSlider(
            context,
            'Mustache size ${_mustacheSize.round()}%',
            _mustacheSize,
            (value) => setState(() => _mustacheSize = value),
          ),
          _buildSlider(
            context,
            'Mustache width ${_mustacheWidth.round()}%',
            _mustacheWidth,
            (value) => setState(() => _mustacheWidth = value),
          ),
          _buildSlider(
            context,
            'Mustache height ${_mustacheHeight.round()}%',
            _mustacheHeight,
            (value) => setState(() => _mustacheHeight = value),
          ),
          _buildSlider(
            context,
            'Mustache thickness ${_mustacheThickness.round()}%',
            _mustacheThickness,
            (value) => setState(() => _mustacheThickness = value),
          ),
          const SizedBox(height: 8),
          _OffsetPad(
            title: 'Mustache placement',
            offsetX: _mustacheOffsetX,
            offsetY: _mustacheOffsetY,
            onChanged: (x, y) => setState(() {
              _mustacheOffsetX = x;
              _mustacheOffsetY = y;
            }),
          ),
        ],
      ),
    );
  }

  Widget _buildMotionPanel(BuildContext context) {
    return _EditorPanel(
      title: _visualModel == CompanionVisualModel.stickFigure
          ? 'Motion studio'
          : 'Model motion',
      subtitle: _visualModel == CompanionVisualModel.stickFigure
          ? 'Stick-figure tuning lives here on its own, without the classic accessory editor crowding it.'
          : 'Robot uses the shared animated scene system and currently keeps its tuning intentionally simple.',
      child: _visualModel == CompanionVisualModel.stickFigure
          ? Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                _buildSlider(
                  context,
                  'Figure scale ${_stickFigureScale.round()}%',
                  _stickFigureScale,
                  (value) => setState(() => _stickFigureScale = value),
                ),
                _buildSlider(
                  context,
                  'Partner spacing ${_stickFigureSpacing.round()}%',
                  _stickFigureSpacing,
                  (value) => setState(() => _stickFigureSpacing = value),
                ),
                _buildSlider(
                  context,
                  'Scene energy ${_stickFigureEnergy.round()}%',
                  _stickFigureEnergy,
                  (value) => setState(() => _stickFigureEnergy = value),
                  min: 0,
                  max: 100,
                  divisions: 20,
                ),
                const SizedBox(height: 10),
                Text(
                  'Spacing controls where the pair finally meets. Energy controls stride and bounce. Scale controls how large the characters feel before they walk off into the distance.',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
            )
          : Text(
              'Use the scene section to drive the robot preview. Extra robot-only sliders can come later once the motion language is stable.',
              style: Theme.of(context).textTheme.bodySmall,
            ),
    );
  }

  Widget _buildChoiceWrap<T>(
    BuildContext context,
    String label,
    List<T> values,
    T selected,
    ValueChanged<T> onSelected,
    String Function(T value) title,
  ) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(label, style: Theme.of(context).textTheme.bodyMedium),
        const SizedBox(height: 6),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: values
              .map(
                (value) => ChoiceChip(
                  label: Text(title(value)),
                  selected: selected == value,
                  onSelected: (_) => onSelected(value),
                ),
              )
              .toList(growable: false),
        ),
      ],
    );
  }

  Widget _buildSlider(
    BuildContext context,
    String label,
    double value,
    ValueChanged<double> onChanged,
    {
      double min = 70,
      double max = 170,
      int divisions = 20,
    }
  ) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(label, style: Theme.of(context).textTheme.bodyMedium),
        Slider(
          min: min,
          max: max,
          divisions: divisions,
          value: value,
          onChanged: onChanged,
        ),
      ],
    );
  }

  void _close() {
    Navigator.of(context).pop(
      _AppearanceEditorResult(
        visualModel: _visualModel,
        scene: _scene,
        previewReferencePose: _referencePose,
        expression: _selectedExpression,
        hairStyle: _hairStyle,
        earsStyle: _earsStyle,
        mustacheStyle: _mustacheStyle,
        glassesStyle: _glassesStyle,
        headwearStyle: _headwearStyle,
        piercingStyle: _piercingStyle,
        hairSize: _hairSize,
        hairWidth: _hairWidth,
        hairHeight: _hairHeight,
        hairThickness: _hairThickness,
        hairOffsetX: _hairOffsetX,
        hairOffsetY: _hairOffsetY,
        eyeOffsetY: _eyeOffsetY,
        mouthOffsetY: _mouthOffsetY,
        mustacheSize: _mustacheSize,
        mustacheWidth: _mustacheWidth,
        mustacheHeight: _mustacheHeight,
        mustacheThickness: _mustacheThickness,
        mustacheOffsetX: _mustacheOffsetX,
        mustacheOffsetY: _mustacheOffsetY,
        stickFigureScale: _stickFigureScale,
        stickFigureSpacing: _stickFigureSpacing,
        stickFigureEnergy: _stickFigureEnergy,
      ),
    );
  }
}

class _FullscreenNoteEditor extends StatefulWidget {
  const _FullscreenNoteEditor({
    required this.text,
    required this.fontSize,
    required this.borderStyle,
    required this.icons,
    required this.flowerAccent,
  });

  final String text;
  final double fontSize;
  final int borderStyle;
  final List<String> icons;
  final String? flowerAccent;

  @override
  State<_FullscreenNoteEditor> createState() => _FullscreenNoteEditorState();
}

class _FullscreenNoteEditorState extends State<_FullscreenNoteEditor> {
  late final TextEditingController _controller;
  late double _fontSize;
  late int _borderStyle;
  late List<String> _icons;
  String? _flowerAccent;

  @override
  void initState() {
    super.initState();
    _controller = TextEditingController(text: widget.text)
      ..addListener(() => setState(() {}));
    _fontSize = widget.fontSize;
    _borderStyle = widget.borderStyle;
    _icons = List<String>.from(widget.icons);
    _flowerAccent = widget.flowerAccent;
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final noteText = _controller.text.characters.take(kMaxNoteCharacters).toString();
    return Scaffold(
      appBar: AppBar(
        title: const Text('Sticky note editor'),
        actions: [
          TextButton(
            onPressed: _close,
            child: const Text('Save'),
          ),
        ],
      ),
      body: SafeArea(
        child: LayoutBuilder(
          builder: (context, constraints) {
            final previewPadding = EdgeInsets.fromLTRB(
              16,
              12,
              16,
              constraints.maxHeight < 700 ? 10 : 14,
            );
            return Column(
              children: [
                Padding(
                  padding: previewPadding,
                  child: _buildPinnedNotePreview(context, noteText),
                ),
                Expanded(
                  child: ListView(
                    padding: const EdgeInsets.fromLTRB(16, 0, 16, 24),
                    children: [
                      _EditorPanel(
                        title: 'Message',
                        subtitle: 'Use inline tokens: <3, <*>, <~>, <n>, <m>.',
                        child: TextField(
                          controller: _controller,
                          minLines: 4,
                          maxLines: 6,
                          maxLength: kMaxNoteCharacters,
                          decoration: const InputDecoration(
                            hintText: 'good luck today, i packed snacks in your bag',
                          ),
                        ),
                      ),
                      const SizedBox(height: 14),
                      _EditorPanel(
                        title: 'Layout',
                        subtitle: 'Font, border, icons, and flower accent all update the live preview above.',
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text('Font size: ${_fontSize.round()}x', style: Theme.of(context).textTheme.bodyMedium),
                            Slider(
                              min: 1,
                              max: 4,
                              divisions: 3,
                              value: _fontSize,
                              onChanged: (value) => setState(() => _fontSize = value),
                            ),
                            Text('Border', style: Theme.of(context).textTheme.bodyMedium),
                            const SizedBox(height: 6),
                            Wrap(
                              spacing: 8,
                              runSpacing: 8,
                              children: [
                                (0, 'None'),
                                (1, 'Outline'),
                                (2, 'Stitched'),
                                (3, 'Hearts'),
                                (4, 'Dots'),
                              ]
                                  .map(
                                    (entry) => ChoiceChip(
                                      label: Text(entry.$2),
                                      selected: _borderStyle == entry.$1,
                                      onSelected: (_) =>
                                          setState(() => _borderStyle = entry.$1),
                                    ),
                                  )
                                  .toList(growable: false),
                            ),
                            const SizedBox(height: 12),
                            Text('Top icons', style: Theme.of(context).textTheme.bodyMedium),
                            const SizedBox(height: 6),
                            Wrap(
                              spacing: 8,
                              runSpacing: 8,
                              children: [
                                ('heart', 'Heart'),
                                ('star', 'Star'),
                                ('flower', 'Flower'),
                                ('note', 'Note'),
                                ('moon', 'Moon'),
                              ]
                                  .map(
                                    (entry) => FilterChip(
                                      label: Text(entry.$2),
                                      selected: _icons.contains(entry.$1),
                                      onSelected: (selected) => setState(() {
                                        if (selected) {
                                          if (!_icons.contains(entry.$1)) {
                                            _icons.add(entry.$1);
                                          }
                                        } else {
                                          _icons.remove(entry.$1);
                                        }
                                      }),
                                    ),
                                  )
                                  .toList(growable: false),
                            ),
                            const SizedBox(height: 12),
                            Text('Flower accent', style: Theme.of(context).textTheme.bodyMedium),
                            const SizedBox(height: 6),
                            Wrap(
                              spacing: 8,
                              runSpacing: 8,
                              children: [
                                ChoiceChip(
                                  label: const Text('None'),
                                  selected: _flowerAccent == null,
                                  onSelected: (_) => setState(() => _flowerAccent = null),
                                ),
                                ...DeskFlower.values.map(
                                  (flower) => ChoiceChip(
                                    label: Text(flower.label),
                                    selected: _flowerAccent == flower.command,
                                    onSelected: (_) => setState(() => _flowerAccent = flower.command),
                                  ),
                                ),
                              ],
                            ),
                            const SizedBox(height: 8),
                            Text(
                              _flowerAccent == null
                                  ? 'Without a flower accent, font size, icons, and border all affect the final card.'
                                  : 'With a flower accent, the device reserves the left side for the bloom and renders the note text at 1x on the right.',
                              style: Theme.of(context).textTheme.bodySmall,
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            );
          },
        ),
      ),
    );
  }

  Widget _buildPinnedNotePreview(BuildContext context, String noteText) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: CompanionTheme.panel,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: CompanionTheme.blush),
        boxShadow: const [
          BoxShadow(
            color: Color(0x12000000),
            blurRadius: 18,
            offset: Offset(0, 6),
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Text('Live preview', style: Theme.of(context).textTheme.titleSmall),
              const Spacer(),
              const _ChipLabel(label: 'Pinned'),
            ],
          ),
          const SizedBox(height: 8),
          Center(
            child: DecoratedBox(
              decoration: BoxDecoration(
                color: CompanionTheme.ink,
                borderRadius: BorderRadius.circular(16),
              ),
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: SizedBox(
                  width: 128 * 3.0,
                  height: 64 * 3.0,
                  child: FittedBox(
                    fit: BoxFit.fill,
                    child: NoteCardPreview(
                      text: noteText,
                      fontSize: _fontSize.round(),
                      border: _borderStyle,
                      icons: _icons,
                      flowerAccent: _flowerAccent,
                    ),
                  ),
                ),
              ),
            ),
          ),
          const SizedBox(height: 8),
          Text(
            _flowerAccent == null
                ? 'Preview stays fixed while the message and note styling controls scroll below it.'
                : 'Preview stays fixed while you edit the note; flower accent mode still mirrors the device\'s right-side 1x text layout.',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ],
      ),
    );
  }

  void _close() {
    Navigator.of(context).pop(
      _NoteEditorResult(
        text: _controller.text.characters.take(kMaxNoteCharacters).toString(),
        fontSize: _fontSize,
        borderStyle: _borderStyle,
        icons: List.unmodifiable(_icons),
        flowerAccent: _flowerAccent,
      ),
    );
  }
}

class _EditorPanel extends StatelessWidget {
  const _EditorPanel({
    required this.title,
    required this.subtitle,
    required this.child,
  });

  final String title;
  final String subtitle;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: CompanionTheme.surface,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: CompanionTheme.blush),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(title, style: Theme.of(context).textTheme.titleSmall),
          const SizedBox(height: 4),
          Text(subtitle, style: Theme.of(context).textTheme.bodyMedium),
          const SizedBox(height: 12),
          child,
        ],
      ),
    );
  }
}

class _OffsetPad extends StatelessWidget {
  const _OffsetPad({
    required this.title,
    required this.offsetX,
    required this.offsetY,
    required this.onChanged,
  });

  final String title;
  final double offsetX;
  final double offsetY;
  final void Function(double x, double y) onChanged;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Text(title, style: Theme.of(context).textTheme.bodyMedium),
            const Spacer(),
            Text('${offsetX.round()}, ${offsetY.round()}'),
          ],
        ),
        const SizedBox(height: 8),
        SizedBox(
          width: double.infinity,
          height: 140,
          child: LayoutBuilder(
            builder: (context, constraints) {
              final size = Size(constraints.maxWidth, constraints.maxHeight);
              final centerX = constraints.maxWidth / 2;
              final centerY = constraints.maxHeight / 2;
              final knobX = centerX + (offsetX / 18) * (constraints.maxWidth / 2 - 14);
              final knobY = centerY + (offsetY / 18) * (constraints.maxHeight / 2 - 14);
              return GestureDetector(
                onPanDown: (details) => _update(details.localPosition, size),
                onPanUpdate: (details) => _update(details.localPosition, size),
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    color: CompanionTheme.cream,
                    borderRadius: BorderRadius.circular(18),
                    border: Border.all(color: CompanionTheme.blush),
                  ),
                  child: Stack(
                    children: [
                      Positioned.fill(
                        child: CustomPaint(
                          painter: _OffsetPadPainter(),
                        ),
                      ),
                      Positioned(
                        left: knobX - 12,
                        top: knobY - 12,
                        child: Container(
                          width: 24,
                          height: 24,
                          decoration: BoxDecoration(
                            color: CompanionTheme.blush,
                            shape: BoxShape.circle,
                            border: Border.all(color: CompanionTheme.ink, width: 1.5),
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              );
            },
          ),
        ),
        const SizedBox(height: 8),
        Row(
          children: [
            IconButton.filledTonal(
              onPressed: () => onChanged((offsetX - 1).clamp(-18, 18), offsetY),
              icon: const Icon(Icons.arrow_left),
            ),
            IconButton.filledTonal(
              onPressed: () => onChanged(offsetX, (offsetY - 1).clamp(-18, 18)),
              icon: const Icon(Icons.arrow_upward),
            ),
            IconButton.filledTonal(
              onPressed: () => onChanged(offsetX, (offsetY + 1).clamp(-18, 18)),
              icon: const Icon(Icons.arrow_downward),
            ),
            IconButton.filledTonal(
              onPressed: () => onChanged((offsetX + 1).clamp(-18, 18), offsetY),
              icon: const Icon(Icons.arrow_right),
            ),
            const Spacer(),
            OutlinedButton(
              onPressed: () => onChanged(0, 0),
              child: const Text('Reset'),
            ),
          ],
        ),
      ],
    );
  }

  void _update(Offset localPosition, Size size) {
    final safeWidth = size.width.isFinite && size.width > 0 ? size.width : 300.0;
    final safeHeight = size.height > 0 ? size.height : 140.0;
    final dx = ((localPosition.dx / safeWidth) * 36) - 18;
    final dy = ((localPosition.dy / safeHeight) * 36) - 18;
    onChanged(dx.clamp(-18, 18), dy.clamp(-18, 18));
  }
}

class _OffsetPadPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = CompanionTheme.blush
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1;
    canvas.drawLine(Offset(size.width / 2, 10), Offset(size.width / 2, size.height - 10), paint);
    canvas.drawLine(Offset(10, size.height / 2), Offset(size.width - 10, size.height / 2), paint);
    canvas.drawRect(Rect.fromLTWH(10, 10, size.width - 20, size.height - 20), paint);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}
