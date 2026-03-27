import 'dart:async';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';

import '../models/companion_image_payload.dart';
import '../providers/desk_companion_controller.dart';
import '../theme/companion_theme.dart';
import '../utils/oled_bitmap_codec.dart';
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

enum DeskHairStyle { none, tuft, bangs, spiky }

extension DeskHairStyleExt on DeskHairStyle {
  String get label => switch (this) {
        DeskHairStyle.none => 'None',
        DeskHairStyle.tuft => 'Tuft',
        DeskHairStyle.bangs => 'Bangs',
        DeskHairStyle.spiky => 'Spiky',
      };

  String get command => switch (this) {
        DeskHairStyle.none => 'none',
        DeskHairStyle.tuft => 'tuft',
        DeskHairStyle.bangs => 'bangs',
        DeskHairStyle.spiky => 'spiky',
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

enum DeskMustacheStyle { none, classic, curled }

extension DeskMustacheStyleExt on DeskMustacheStyle {
  String get label => switch (this) {
        DeskMustacheStyle.none => 'None',
        DeskMustacheStyle.classic => 'Classic',
        DeskMustacheStyle.curled => 'Curled',
      };

  String get command => switch (this) {
        DeskMustacheStyle.none => 'none',
        DeskMustacheStyle.classic => 'classic',
        DeskMustacheStyle.curled => 'curled',
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

class DeskCompanionStudioScreen extends StatefulWidget {
  const DeskCompanionStudioScreen({super.key});

  @override
  State<DeskCompanionStudioScreen> createState() =>
      _DeskCompanionStudioScreenState();
}

class _DeskCompanionStudioScreenState extends State<DeskCompanionStudioScreen> {
  final _wifiSsidController = TextEditingController();
  final _wifiPasswordController = TextEditingController();
  final _relayBaseUrlController = TextEditingController();
  final _deviceTokenController = TextEditingController();
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
  DeskExpression _selectedExpression = DeskExpression.happy;
  DeskPersonality _selectedPersonality = DeskPersonality.curious;
  DeskPetMode _selectedPetMode = DeskPetMode.hangout;
  DeskCareAction _selectedCareAction = DeskCareAction.pet;
  DeskHairStyle _selectedHairStyle = DeskHairStyle.none;
  DeskEarsStyle _selectedEarsStyle = DeskEarsStyle.none;
  DeskMustacheStyle _selectedMustacheStyle = DeskMustacheStyle.none;
  Timer? _liveSyncTimer;

  @override
  void initState() {
    super.initState();
    _noteController.addListener(_onNoteTextChanged);
  }

  @override
  void dispose() {
    _wifiSsidController.dispose();
    _wifiPasswordController.dispose();
    _relayBaseUrlController.dispose();
    _deviceTokenController.dispose();
    _noteController.removeListener(_onNoteTextChanged);
    _noteController.dispose();
    _bannerController.dispose();
    _liveSyncTimer?.cancel();
    super.dispose();
  }

  void _onNoteTextChanged() {
    if (mounted) setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    final controller = context.watch<DeskCompanionController>();
    _syncTextController(_relayBaseUrlController, controller.relayBaseUrl);
    _syncTextController(_deviceTokenController, controller.deviceToken);
    final currentPersonality =
      _personalityFromCommand(controller.petPersonality) ??
        _selectedPersonality;
    final currentPetMode =
      _petModeFromCommand(controller.activePetMode) ?? _selectedPetMode;

    return Scaffold(
      appBar: AppBar(title: const Text('Desk Companion')),
      body: DecoratedBox(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: [Color(0xFFFFF4EE), CompanionTheme.cream],
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
                            label: controller.connectedSsid.isEmpty
                                ? 'Wi-Fi: not joined'
                                : 'Wi-Fi: ${controller.connectedSsid}',
                          ),
                          _ChipLabel(label: _relayChipLabel(controller)),
                        ],
                      ),
                      if (_relayStatusText(controller).isNotEmpty) ...[
                        const SizedBox(height: 10),
                        Text(
                          _relayStatusText(controller),
                          style: Theme.of(context).textTheme.bodyMedium,
                        ),
                      ],
                      if (controller.hasRelayTarget) ...[
                        const SizedBox(height: 10),
                        Text(
                          'Token: ${controller.deviceToken} | Pending: ${controller.relayPendingCount}',
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                        if (controller.relayLastCommandAt != null)
                          Text(
                            'Last command queued: ${_relativeTimeLabel(controller.relayLastCommandAt!)}',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                        if (controller.relayLastSeenAt != null)
                          Text(
                            'Last command poll: ${_relativeTimeLabel(controller.relayLastSeenAt!)}',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                        if (controller.relayLastStatusAt != null)
                          Text(
                            'Last status post: ${_relativeTimeLabel(controller.relayLastStatusAt!)}',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                      ],
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
                                      : controller.hasRelayTarget
                                          ? () => _connectWifiDevice(controller)
                                          : null,
                              icon: Icon(
                                controller.isBleConnected
                                    ? Icons.link_off
                                    : controller.isRemoteConnected
                                        ? Icons.wifi_tethering
                                        : Icons.wifi_find_outlined,
                              ),
                              label: Text(
                                controller.isBleConnected
                                    ? 'Disconnect'
                                    : 'Connect over Wi-Fi',
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
                  title: 'Remote setup',
                  subtitle:
                      'Provision Wi-Fi and relay settings over BLE once. After that, the device should stay on Wi-Fi and talk to the hosted relay on its own.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Wi-Fi',
                        style: Theme.of(context).textTheme.titleSmall,
                      ),
                      const SizedBox(height: 8),
                      if (controller.availableWifiNetworks.isNotEmpty) ...[
                        Wrap(
                          spacing: 8,
                          runSpacing: 8,
                          children: controller.availableWifiNetworks
                              .map(
                                (ssid) => ChoiceChip(
                                  label: Text(ssid),
                                  selected: _wifiSsidController.text.trim() == ssid,
                                  onSelected: (_) => setState(
                                    () => _wifiSsidController.text = ssid,
                                  ),
                                ),
                              )
                              .toList(growable: false),
                        ),
                        const SizedBox(height: 10),
                      ],
                      TextField(
                        controller: _wifiSsidController,
                        decoration: const InputDecoration(
                          labelText: 'Wi-Fi name',
                        ),
                      ),
                      const SizedBox(height: 10),
                      TextField(
                        controller: _wifiPasswordController,
                        obscureText: true,
                        decoration: const InputDecoration(
                          labelText: 'Wi-Fi password',
                        ),
                      ),
                      const SizedBox(height: 10),
                      Row(
                        children: [
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy || !controller.isBleConnected
                                  ? null
                                  : () => _scanWifiNetworks(controller),
                              icon: const Icon(Icons.wifi_find_outlined),
                              label: const Text('Scan Wi-Fi'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed: controller.busy || !controller.isBleConnected
                                  ? null
                                  : () => _sendWifi(controller),
                              icon: const Icon(Icons.wifi),
                              label: const Text('Send Wi-Fi'),
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 10),
                      SizedBox(
                        width: double.infinity,
                        child: OutlinedButton.icon(
                          onPressed: controller.busy || !controller.isBleConnected
                              ? null
                              : () => _forgetWifi(controller),
                          icon: const Icon(Icons.wifi_off_outlined),
                          label: const Text('Forget device Wi-Fi'),
                        ),
                      ),
                      const SizedBox(height: 18),
                      Text(
                        'Relay',
                        style: Theme.of(context).textTheme.titleSmall,
                      ),
                      const SizedBox(height: 8),
                      TextField(
                        controller: _relayBaseUrlController,
                        onChanged: controller.updateRelayBaseUrl,
                        decoration: const InputDecoration(
                          labelText: 'Relay base URL',
                          hintText: 'https://relay.yourdomain.com',
                        ),
                      ),
                      const SizedBox(height: 10),
                      TextField(
                        controller: _deviceTokenController,
                        onChanged: controller.updateDeviceToken,
                        decoration: const InputDecoration(
                          labelText: 'Device token',
                          hintText: 'desk-01',
                        ),
                      ),
                      const SizedBox(height: 10),
                      Row(
                        children: [
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy || !controller.hasRelayTarget
                                  ? null
                                  : () => _connectWifiDevice(controller),
                              icon: Icon(
                                controller.isRemoteConnected
                                    ? Icons.wifi_tethering
                                    : Icons.wifi_find_outlined,
                              ),
                              label: Text(
                                controller.isRemoteConnected
                                    ? 'Connect to device'
                                    : 'Connect to device',
                              ),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed: controller.busy || !controller.isBleConnected
                                  ? null
                                  : () => _configureRelay(controller),
                              icon: const Icon(Icons.cloud_done_outlined),
                              label: const Text('Save relay to device'),
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Pet companion',
                  subtitle:
                      'Current personality: ${currentPersonality.label}. Active pet mode: ${currentPetMode.label}.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Personality',
                        style: Theme.of(context).textTheme.titleSmall,
                      ),
                      const SizedBox(height: 8),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: DeskPersonality.values
                            .map(
                              (personality) => ChoiceChip(
                                label: Text(personality.label),
                                selected: _selectedPersonality == personality,
                                onSelected: controller.busy
                                    ? null
                                    : (_) => setState(
                                          () => _selectedPersonality =
                                              personality,
                                        ),
                              ),
                            )
                            .toList(growable: false),
                      ),
                      const SizedBox(height: 12),
                      Text(
                        _selectedPersonality.description,
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 12),
                      SizedBox(
                        width: double.infinity,
                        child: ElevatedButton.icon(
                          onPressed: controller.busy || !controller.canControlDevice
                              ? null
                              : () => _sendPersonality(controller),
                          icon: const Icon(Icons.pets_outlined),
                          label: Text(
                            'Set ${_selectedPersonality.label} personality',
                          ),
                        ),
                      ),
                      const SizedBox(height: 18),
                      Text(
                        'Companion mode',
                        style: Theme.of(context).textTheme.titleSmall,
                      ),
                      const SizedBox(height: 8),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: DeskPetMode.values
                            .map(
                              (petMode) => ChoiceChip(
                                label: Text(petMode.label),
                                selected: _selectedPetMode == petMode,
                                onSelected: controller.busy
                                    ? null
                                    : (_) => setState(
                                          () => _selectedPetMode = petMode,
                                        ),
                              ),
                            )
                            .toList(growable: false),
                      ),
                      const SizedBox(height: 12),
                      Text(
                        _selectedPetMode.description,
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 12),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: [
                          _ChipLabel(label: 'Bond ${controller.bondLevel}%'),
                          _ChipLabel(label: 'Energy ${controller.energyLevel}%'),
                          _ChipLabel(label: 'Boredom ${controller.boredomLevel}%'),
                        ],
                      ),
                      const SizedBox(height: 12),
                      SizedBox(
                        width: double.infinity,
                        child: OutlinedButton.icon(
                          onPressed: controller.busy || !controller.canControlDevice
                              ? null
                              : () => _triggerPetMode(controller),
                          icon: const Icon(Icons.auto_awesome_outlined),
                          label: Text('Set ${_selectedPetMode.label} mode'),
                        ),
                      ),
                      const SizedBox(height: 18),
                      Text(
                        'Care action',
                        style: Theme.of(context).textTheme.titleSmall,
                      ),
                      const SizedBox(height: 8),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: DeskCareAction.values
                            .map(
                              (action) => ChoiceChip(
                                label: Text(action.label),
                                selected: _selectedCareAction == action,
                                onSelected: controller.busy
                                    ? null
                                    : (_) => setState(
                                          () => _selectedCareAction = action,
                                        ),
                              ),
                            )
                            .toList(growable: false),
                      ),
                      const SizedBox(height: 12),
                      Text(
                        _selectedCareAction.description,
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 12),
                      SizedBox(
                        width: double.infinity,
                        child: ElevatedButton.icon(
                          onPressed: controller.busy || !controller.canControlDevice
                              ? null
                              : () => _sendCareAction(controller),
                          icon: const Icon(Icons.favorite_outline),
                          label: Text('Send ${_selectedCareAction.label} action'),
                        ),
                      ),
                      const SizedBox(height: 18),
                      Text(
                        'Appearance',
                        style: Theme.of(context).textTheme.titleSmall,
                      ),
                      const SizedBox(height: 8),
                      Text(
                        'Hair',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 6),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: DeskHairStyle.values
                            .map(
                              (hair) => ChoiceChip(
                                label: Text(hair.label),
                                selected: _selectedHairStyle == hair,
                                onSelected: controller.busy
                                    ? null
                                    : (_) => setState(
                                          () => _selectedHairStyle = hair,
                                        ),
                              ),
                            )
                            .toList(growable: false),
                      ),
                      const SizedBox(height: 12),
                      Text(
                        'Ears',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 6),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: DeskEarsStyle.values
                            .map(
                              (ears) => ChoiceChip(
                                label: Text(ears.label),
                                selected: _selectedEarsStyle == ears,
                                onSelected: controller.busy
                                    ? null
                                    : (_) => setState(
                                          () => _selectedEarsStyle = ears,
                                        ),
                              ),
                            )
                            .toList(growable: false),
                      ),
                      const SizedBox(height: 12),
                      Text(
                        'Mustache',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 6),
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: DeskMustacheStyle.values
                            .map(
                              (mustache) => ChoiceChip(
                                label: Text(mustache.label),
                                selected: _selectedMustacheStyle == mustache,
                                onSelected: controller.busy
                                    ? null
                                    : (_) => setState(
                                          () => _selectedMustacheStyle = mustache,
                                        ),
                              ),
                            )
                            .toList(growable: false),
                      ),
                      const SizedBox(height: 12),
                      SizedBox(
                        width: double.infinity,
                        child: OutlinedButton.icon(
                          onPressed: controller.busy || !controller.canControlDevice
                              ? null
                              : () => _sendCompanionStyle(controller),
                          icon: const Icon(Icons.face_retouching_natural),
                          label: const Text('Apply appearance'),
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Sticky note',
                  subtitle:
                      'Type a message — rendered with the display\'s crisp built-in font.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      TextField(
                        controller: _noteController,
                        minLines: 3,
                        maxLines: 5,
                        maxLength: kMaxNoteCharacters,
                        decoration: const InputDecoration(
                          labelText: 'Message',
                          hintText:
                              'good luck today, i packed snacks in your bag',
                          helperText:
                              'Up to 80 chars. Use <3 ♥  <*> ★  <~> ✿  <n> ♪  <m> ☾ inline!',
                        ),
                      ),
                      const SizedBox(height: 12),
                      Text(
                        'Font size: ${_noteFontSize.round()}x',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      Slider(
                        min: 1,
                        max: 4,
                        divisions: 3,
                        value: _noteFontSize,
                        onChanged: (value) => setState(() {
                          _noteFontSize = value;
                        }),
                      ),
                      const SizedBox(height: 8),
                      Text(
                        'Border',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 4),
                      Wrap(
                        spacing: 6,
                        runSpacing: 4,
                        children: [
                          for (final (idx, label) in [
                            (0, 'None'),
                            (1, 'Outline'),
                            (2, 'Stitched'),
                            (3, 'Hearts'),
                            (4, 'Dots'),
                          ])
                            ChoiceChip(
                              label: Text(label),
                              selected: _noteBorderStyle == idx,
                              onSelected: (_) =>
                                  setState(() => _noteBorderStyle = idx),
                            ),
                        ],
                      ),
                      const SizedBox(height: 8),
                      Text(
                        'Decorations',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 4),
                      Wrap(
                        spacing: 6,
                        runSpacing: 4,
                        children: [
                          for (final (name, label) in [
                            ('heart', '♥ Heart'),
                            ('star',  '★ Star'),
                            ('flower','✿ Flower'),
                            ('note',  '♪ Note'),
                            ('moon',  '☾ Moon'),
                          ])
                            FilterChip(
                              label: Text(label),
                              selected: _noteIcons.contains(name),
                              onSelected: (on) => setState(() {
                                on
                                    ? _noteIcons.add(name)
                                    : _noteIcons.remove(name);
                              }),
                            ),
                        ],
                      ),
                      const SizedBox(height: 8),
                      Text(
                        'Flower accent',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 4),
                      Wrap(
                        spacing: 6,
                        runSpacing: 4,
                        children: [
                          for (final flower in DeskFlower.values)
                            ChoiceChip(
                              label: Text(flower.label),
                              selected: _noteFlowerAccent == flower.command,
                              onSelected: (_) => setState(() {
                                _noteFlowerAccent = _noteFlowerAccent == flower.command
                                    ? null
                                    : flower.command;
                              }),
                            ),
                        ],
                      ),
                      if (_noteFlowerAccent != null)
                        Padding(
                          padding: const EdgeInsets.only(top: 4),
                          child: Text(
                            'A large ${_noteFlowerAccent!.replaceAll('_', ' ')} will be drawn beside your note.',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                        ),
                      const SizedBox(height: 8),
                      Text(
                        'Preview',
                        style: Theme.of(context).textTheme.bodyMedium,
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
                                  text: _noteController.text.characters
                                      .take(kMaxNoteCharacters)
                                      .toString(),
                                  fontSize: _noteFontSize.round(),
                                  border: _noteBorderStyle,
                                  icons: List.unmodifiable(_noteIcons),
                                ),
                              ),
                            ),
                          ),
                        ),
                      ),
                      const SizedBox(height: 12),
                      Row(
                        children: [
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed: controller.busy || !controller.canControlDevice
                                  ? null
                                  : () => _sendNote(controller),
                              icon: const Icon(Icons.favorite_border),
                              label: const Text('Show note'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy || !controller.canControlDevice
                                  ? null
                                  : () => _perform(
                                        () => controller.clearDisplay(),
                                        success: 'Display cleared.',
                                      ),
                              icon:
                                  const Icon(Icons.cleaning_services_outlined),
                              label: const Text('Clear'),
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
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
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: DeskFlower.values
                            .map(
                              (flower) => ChoiceChip(
                                label: Text(flower.label),
                                selected: _selectedFlower == flower,
                                onSelected: controller.busy
                                    ? null
                                    : (_) => setState(
                                          () => _selectedFlower = flower,
                                        ),
                              ),
                            )
                            .toList(growable: false),
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
                  title: 'Expressions',
                  subtitle:
                      'Animated desk eyes and moods that run directly on the device after you send them.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Wrap(
                        spacing: 8,
                        runSpacing: 8,
                        children: DeskExpression.values
                            .map(
                              (expression) => ChoiceChip(
                                label: Text(expression.label),
                                selected: _selectedExpression == expression,
                                onSelected: controller.busy
                                    ? null
                                    : (_) => setState(
                                          () => _selectedExpression = expression,
                                        ),
                              ),
                            )
                            .toList(growable: false),
                      ),
                      const SizedBox(height: 12),
                      Text(
                        _selectedExpression.subtitle,
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 12),
                      SizedBox(
                        width: double.infinity,
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

  Future<void> _scanWifiNetworks(DeskCompanionController controller) async {
    await _perform(
      controller.scanWifiNetworks,
      success: 'Wi-Fi scan requested.',
    );
  }

  Future<void> _sendWifi(DeskCompanionController controller) async {
    final ssid = _wifiSsidController.text.trim();
    if (ssid.isEmpty) {
      _showMessage('Enter a Wi-Fi name first.');
      return;
    }

    await _perform(
      () => controller.sendWifiCredentials(
        ssid: ssid,
        password: _wifiPasswordController.text,
      ),
      success: 'Wi-Fi credentials sent.',
    );
  }

  Future<void> _forgetWifi(DeskCompanionController controller) async {
    await _perform(
      controller.forgetWifi,
      success: 'Device Wi-Fi forgotten.',
    );
  }

  Future<void> _configureRelay(DeskCompanionController controller) async {
    final relayBase = _relayBaseUrlController.text.trim();
    final token = _deviceTokenController.text.trim();
    if (relayBase.isEmpty || token.isEmpty) {
      _showMessage('Relay URL and device token are required.');
      return;
    }

    await _perform(
      () => controller.configureRelay(
        relayUrl: relayBase,
        token: token,
      ),
      success: 'Relay settings sent to the device.',
    );
  }

  Future<void> _connectWifiDevice(DeskCompanionController controller) async {
    try {
      final ok = await controller.connectRemoteDevice();
      if (!mounted) {
        return;
      }
      _showMessage(
        ok
            ? 'Device connected over Wi-Fi. Remote sending is ready.'
            : 'The device is not online through the relay right now.',
      );
    } catch (error) {
      if (!mounted) {
        return;
      }
      _showMessage('$error');
    }
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
    await _perform(
      () => controller.sendExpression(_selectedExpression.command),
      success: '${_selectedExpression.label} expression sent.',
    );
  }

  Future<void> _sendPersonality(DeskCompanionController controller) async {
    await _perform(
      () => controller.setPetPersonality(_selectedPersonality.command),
      success: '${_selectedPersonality.label} personality set.',
    );
  }

  Future<void> _triggerPetMode(DeskCompanionController controller) async {
    await _perform(
      () => controller.triggerPetMode(_selectedPetMode.command),
      success: '${_selectedPetMode.label} mode set.',
    );
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
      ),
      success: 'Companion appearance applied.',
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

  void _syncTextController(TextEditingController controller, String value) {
    if (controller.text == value) {
      return;
    }

    controller.value = TextEditingValue(
      text: value,
      selection: TextSelection.collapsed(offset: value.length),
    );
  }

  String _relayChipLabel(DeskCompanionController controller) {
    if (!controller.hasRelayTarget) {
      return 'Wi-Fi link: not configured';
    }
    if (!controller.relayStatusKnown) {
      return 'Wi-Fi link: ready';
    }
    return controller.isRemoteConnected
        ? 'Wi-Fi link: connected'
        : 'Wi-Fi link: not connected';
  }

  String _relayStatusText(DeskCompanionController controller) {
    if (!controller.hasRelayTarget) {
      return '';
    }
    final lastSeenAt = controller.relayLastSeenAt;
    if (lastSeenAt == null) {
      final lastStatusAt = controller.relayLastStatusAt;
      if (lastStatusAt != null) {
        final age = DateTime.now().difference(lastStatusAt);
        final ageLabel = age.inMinutes < 1
            ? '${age.inSeconds}s ago'
            : age.inHours < 1
                ? '${age.inMinutes}m ago'
                : '${age.inHours}h ago';
        return 'Device posted status $ageLabel, but the relay command link is not active yet.';
      }
      return controller.isBleConnected
          ? 'Remote delivery is configured. The device should stay linked through Wi-Fi and the relay after BLE setup.'
          : 'Remote delivery is configured. Press Connect over Wi-Fi to confirm the device is online.';
    }

    final age = DateTime.now().difference(lastSeenAt);
    final ageLabel = age.inMinutes < 1
        ? '${age.inSeconds}s ago'
        : age.inHours < 1
            ? '${age.inMinutes}m ago'
            : '${age.inHours}h ago';
    return 'Last device check-in over Wi-Fi: $ageLabel';
  }

  String _relativeTimeLabel(DateTime value) {
    final age = DateTime.now().difference(value);
    if (age.inMinutes < 1) {
      return '${age.inSeconds}s ago';
    }
    if (age.inHours < 1) {
      return '${age.inMinutes}m ago';
    }
    return '${age.inHours}h ago';
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
        color: CompanionTheme.blush,
        borderRadius: BorderRadius.circular(999),
      ),
      child: Text(label, style: Theme.of(context).textTheme.bodyMedium),
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
