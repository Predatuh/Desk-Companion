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
      };
}

class DeskCompanionStudioScreen extends StatefulWidget {
  const DeskCompanionStudioScreen({super.key});

  @override
  State<DeskCompanionStudioScreen> createState() =>
      _DeskCompanionStudioScreenState();
}

class _DeskCompanionStudioScreenState extends State<DeskCompanionStudioScreen> {
  final _wifiPasswordController = TextEditingController();
  final _relayUrlController = TextEditingController();
  final _deviceTokenController = TextEditingController();
  final _noteController = TextEditingController();
  final _bannerController = TextEditingController(text: 'miss you already <3');
  String? _selectedWifiSsid;
  double _noteFontSize = 1;
  int _noteBorderStyle = 0;
  final List<String> _noteIcons = [];

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
  Timer? _liveSyncTimer;

  @override
  void initState() {
    super.initState();
    _noteController.addListener(_onNoteTextChanged);
  }

  @override
  void dispose() {
    _noteController.removeListener(_onNoteTextChanged);
    _wifiPasswordController.dispose();
    _relayUrlController.dispose();
    _deviceTokenController.dispose();
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
    final availableWifiNetworks = controller.availableWifiNetworks;
    final selectedWifiSsid = _resolveSelectedWifiSsid(
      availableWifiNetworks,
      controller.connectedSsid,
    );
    final canReachDevice = controller.canControlDevice;

    _syncController(_relayUrlController, controller.relayBaseUrl);
    _syncController(_deviceTokenController, controller.deviceToken);

    final canUseRelay = controller.hasRelayTarget;

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
                            label: controller.deviceIp.isEmpty
                                ? 'IP: no connection'
                                : 'IP: ${controller.deviceIp}',
                          ),
                          _ChipLabel(
                            label: controller.connectedSsid.isEmpty
                                ? 'Wi-Fi: not joined'
                                : 'Wi-Fi: ${controller.connectedSsid}',
                          ),
                          _ChipLabel(
                            label: _relayStatusLabel(controller),
                          ),
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
                                      : controller.hasRelayTarget
                                          ? () => _perform(
                                                () => controller
                                                    .refreshDeviceStatus(),
                                                success: controller
                                                        .isRelayOnline
                                                    ? 'Device is online over Wi-Fi.'
                                                    : 'Wi-Fi status checked.',
                                              )
                                          : () => _perform(
                                                () => controller
                                                    .refreshDeviceStatus(),
                                                success:
                                                    'Device status refreshed.',
                                              ),
                              icon: Icon(
                                controller.isBleConnected
                                    ? Icons.link_off
                                    : controller.hasRelayTarget
                                        ? Icons.wifi
                                        : Icons.refresh,
                              ),
                              label: Text(
                                controller.isBleConnected
                                    ? 'Disconnect'
                                    : controller.hasRelayTarget
                                        ? 'Connect Wi-Fi'
                                        : 'Refresh status',
                              ),
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 14),
                      Text(
                        canUseRelay
                            ? 'BLE is only needed for setup. If the relay target is saved and the desk is online, you can connect and send over Wi-Fi directly.'
                            : 'Connect over BLE once to save the relay URL and token to the device for future Wi-Fi use.',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Remote relay',
                  subtitle:
                      'Use this when you want to send something even when the phone is not on the same Wi-Fi as the desk.',
                  child: Column(
                    children: [
                      TextField(
                        controller: _relayUrlController,
                        onChanged: controller.updateRelayBaseUrl,
                        decoration: const InputDecoration(
                          labelText: 'Relay base URL',
                          hintText:
                              'https://desk-companion-production.up.railway.app',
                          helperText:
                              'Use the base domain only. Full paths like /v1/device/... are cleaned automatically.',
                        ),
                      ),
                      const SizedBox(height: 12),
                      TextField(
                        controller: _deviceTokenController,
                        onChanged: controller.updateDeviceToken,
                        decoration: const InputDecoration(
                          labelText: 'Device token',
                          hintText: 'gf-desk-01',
                        ),
                      ),
                      const SizedBox(height: 12),
                      Row(
                        children: [
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed:
                                  controller.busy || !controller.isBleConnected
                                      ? null
                                      : () => _saveRelay(controller),
                              icon: const Icon(Icons.cloud_done_outlined),
                              label: const Text('Save to device'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy ||
                                      !controller.hasRelayTarget
                                  ? null
                                  : () => _perform(
                                        () => controller.refreshDeviceStatus(),
                                        success: 'Relay status refreshed.',
                                      ),
                              icon: const Icon(Icons.cloud_sync_outlined),
                              label: const Text('Check relay'),
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Wi-Fi setup',
                  subtitle:
                      'Scan Wi-Fi from the device, pick a network, then send only the password.',
                  child: Column(
                    children: [
                      InputDecorator(
                        decoration: const InputDecoration(
                          labelText: 'Wi-Fi network',
                          hintText: 'Scan from device first',
                        ),
                        child: availableWifiNetworks.isEmpty
                            ? const Align(
                                alignment: Alignment.centerLeft,
                                child: Text('No scanned networks yet.'),
                              )
                            : Wrap(
                                spacing: 8,
                                runSpacing: 8,
                                children: availableWifiNetworks
                                    .map(
                                      (ssid) => ChoiceChip(
                                        label: Text(
                                          ssid,
                                          overflow: TextOverflow.ellipsis,
                                        ),
                                        selected: selectedWifiSsid == ssid,
                                        onSelected: controller.busy
                                            ? null
                                            : (_) => setState(
                                                () => _selectedWifiSsid = ssid),
                                      ),
                                    )
                                    .toList(growable: false),
                              ),
                      ),
                      const SizedBox(height: 12),
                      TextField(
                        controller: _wifiPasswordController,
                        obscureText: true,
                        decoration:
                            const InputDecoration(labelText: 'Wi-Fi password'),
                      ),
                      const SizedBox(height: 12),
                      Row(
                        children: [
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy || !canReachDevice
                                  ? null
                                  : () {
                                      setState(() => _selectedWifiSsid = null);
                                      _perform(
                                        () => controller.scanWifiNetworks(),
                                        success:
                                            controller.isBleConnected
                                                ? 'Wi-Fi networks refreshed from device. Pick one from the list.'
                                                : 'Wi-Fi scan requested over relay. Wait a moment for the list to refresh.',
                                      );
                                    },
                              icon: const Icon(Icons.wifi_find_outlined),
                              label: const Text('Scan networks'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed: controller.busy ||
                                      !canReachDevice ||
                                      selectedWifiSsid == null
                                  ? null
                                  : () => _sendWifi(controller),
                              icon: const Icon(Icons.wifi),
                              label: const Text('Send password'),
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 8),
                      Align(
                        alignment: Alignment.centerLeft,
                        child: TextButton.icon(
                          onPressed: controller.busy || !canReachDevice
                              ? null
                              : () {
                                  setState(() {
                                    _selectedWifiSsid = null;
                                    _wifiPasswordController.clear();
                                  });
                                  _perform(
                                    () => controller.forgetWifi(),
                                    success: 'Wi-Fi credentials cleared on device.',
                                  );
                                },
                          icon: const Icon(Icons.wifi_off, size: 18),
                          label: const Text('Forget Wi-Fi'),
                        ),
                      ),
                      if (availableWifiNetworks.isEmpty) ...[
                        const SizedBox(height: 10),
                        Align(
                          alignment: Alignment.centerLeft,
                          child: Text(
                            'No networks yet — tap Scan networks to load them from the device.',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                        ),
                      ],
                      if (controller.wifiScanning || controller.wifiConnecting) ...[
                        const SizedBox(height: 12),
                        Column(
                          children: [
                            const LinearProgressIndicator(),
                            const SizedBox(height: 6),
                            Text(
                              controller.statusMessage,
                              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                                fontWeight: FontWeight.w600,
                              ),
                            ),
                          ],
                        ),
                      ],
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
                              onPressed: controller.busy
                                  ? null
                                  : () => _sendNote(controller),
                              icon: const Icon(Icons.favorite_border),
                              label: const Text('Show note'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy
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
                          onPressed: controller.busy
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
                          onPressed: controller.busy
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
                              onPressed: controller.busy
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
                                  controller.busy || _selectedImage == null
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

  void _syncController(TextEditingController controller, String value) {
    if (controller.text != value) {
      controller.text = value;
      controller.selection = TextSelection.fromPosition(
        TextPosition(offset: controller.text.length),
      );
    }
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

  Future<void> _sendWifi(DeskCompanionController controller) async {
    final selectedWifiSsid = _resolveSelectedWifiSsid(
      controller.availableWifiNetworks,
      controller.connectedSsid,
    );
    if (selectedWifiSsid == null) {
      _showMessage('Scan and pick a Wi-Fi network first.');
      return;
    }
    await _perform(
      () => controller.sendWifiCredentials(
        ssid: selectedWifiSsid,
        password: _wifiPasswordController.text,
      ),
      success: controller.isBleConnected
          ? 'Wi-Fi credentials sent. Use refresh once the display joins.'
          : 'Wi-Fi credentials sent over relay. Watch the status chip for delivery confirmation.',
    );
  }

  String? _resolveSelectedWifiSsid(
      List<String> availableWifiNetworks, String connectedSsid) {
    if (_selectedWifiSsid != null &&
        availableWifiNetworks.contains(_selectedWifiSsid)) {
      return _selectedWifiSsid;
    }
    if (connectedSsid.trim().isNotEmpty) {
      return connectedSsid.trim();
    }
    return null;
  }

  Future<void> _saveRelay(DeskCompanionController controller) async {
    await _perform(
      () => controller.configureRelay(
        relayUrl: _relayUrlController.text.trim(),
        token: _deviceTokenController.text.trim(),
      ),
      success: 'Relay settings saved to device.',
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
      ),
      success: controller.hasRelayTarget && !controller.isBleConnected
          ? 'Note sent via relay — check the status chip for delivery confirmation.'
          : 'Note delivered.',
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

  String _relayStatusLabel(DeskCompanionController controller) {
    if (!controller.hasRelayTarget) {
      return 'Relay: not set';
    }
    if (!controller.relayStatusKnown) {
      return 'Relay: checking';
    }
    if (controller.isRelayOnline) {
      return 'Relay: online';
    }
    if (controller.relayLastSeenAt != null) {
      final now = DateTime.now();
      final minutes = now.difference(controller.relayLastSeenAt!).inMinutes;
      if (minutes <= 0) {
        return 'Relay: just seen';
      }
      return 'Relay: offline (${minutes}m ago)';
    }
    return 'Relay: offline';
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
