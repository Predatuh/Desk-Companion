import 'dart:async';
import 'dart:typed_data';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/companion_image_payload.dart';
import '../providers/desk_companion_controller.dart';
import '../theme/companion_theme.dart';
import '../utils/oled_bitmap_codec.dart';
import '../widgets/oled_drawing_pad.dart';

class DeskCompanionStudioScreen extends StatefulWidget {
  const DeskCompanionStudioScreen({super.key});

  @override
  State<DeskCompanionStudioScreen> createState() => _DeskCompanionStudioScreenState();
}

class _DeskCompanionStudioScreenState extends State<DeskCompanionStudioScreen> {
  final _wifiSsidController = TextEditingController();
  final _wifiPasswordController = TextEditingController();
  final _hostController = TextEditingController();
  final _relayUrlController = TextEditingController();
  final _deviceTokenController = TextEditingController();
  final _noteController = TextEditingController();
  final _bannerController = TextEditingController(text: 'miss you already <3');

  CompanionImagePayload? _selectedImage;
  Uint8List _drawBitmap = Uint8List(OledBitmapCodec.byteLength);
  bool _preferHttp = true;
  bool _invertImage = false;
  bool _showGrid = true;
  bool _liveDraw = false;
  bool _eraserMode = false;
  double _bannerSpeed = 35;
  double _brushSize = 2;
  Timer? _liveSyncTimer;

  @override
  void dispose() {
    _wifiSsidController.dispose();
    _wifiPasswordController.dispose();
    _hostController.dispose();
    _relayUrlController.dispose();
    _deviceTokenController.dispose();
    _noteController.dispose();
    _bannerController.dispose();
    _liveSyncTimer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final controller = context.watch<DeskCompanionController>();

    _syncController(_hostController, controller.manualHost);
    _syncController(_relayUrlController, controller.relayBaseUrl);
    _syncController(_deviceTokenController, controller.deviceToken);

    final canUseHttp = controller.hasHttpTarget;
    final canUseRelay = controller.hasRelayTarget;
    final transportHint = _preferHttp && canUseHttp
        ? 'Wi-Fi first, relay or BLE fallback'
        : canUseRelay
            ? 'Relay or BLE'
            : 'BLE only';

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
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 24),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                _HeroPanel(
                  bleState: controller.bleState.name,
                  mode: controller.mode,
                  transportHint: transportHint,
                  liveDraw: _liveDraw,
                ),
                const SizedBox(height: 16),
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
                                ? 'IP: waiting'
                                : 'IP: ${controller.deviceIp}',
                          ),
                          _ChipLabel(
                            label: controller.connectedSsid.isEmpty
                                ? 'Wi-Fi: not joined'
                                : 'Wi-Fi: ${controller.connectedSsid}',
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
                              label: const Text('Find device'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy
                                  ? null
                                  : controller.isBleConnected
                                      ? () => controller.disconnect()
                                      : () => _perform(
                                            () => controller.refreshDeviceStatus(),
                                            success: 'Device status refreshed.',
                                          ),
                              icon: Icon(controller.isBleConnected
                                  ? Icons.link_off
                                  : Icons.refresh),
                              label: Text(controller.isBleConnected
                                  ? 'Disconnect'
                                  : 'Refresh status'),
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 14),
                      TextField(
                        controller: _hostController,
                        onChanged: controller.updateManualHost,
                        decoration: const InputDecoration(
                          labelText: 'Manual host override',
                          hintText: '192.168.1.88 or http://desk.local',
                        ),
                      ),
                      const SizedBox(height: 10),
                      SwitchListTile.adaptive(
                        contentPadding: EdgeInsets.zero,
                        title: const Text('Prefer Wi-Fi delivery when available'),
                        subtitle: Text(canUseHttp
                            ? 'Current host is ready for HTTP commands.'
                            : canUseRelay
                                ? 'Relay is configured, so remote commands can still queue.'
                                : 'BLE will be used until the device reports an IP address.'),
                        value: _preferHttp,
                        onChanged: (value) => setState(() => _preferHttp = value),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Remote relay',
                  subtitle: 'Use this when you want to send something even when the phone is not on the same Wi-Fi as the desk.',
                  child: Column(
                    children: [
                      TextField(
                        controller: _relayUrlController,
                        onChanged: controller.updateRelayBaseUrl,
                        decoration: const InputDecoration(
                          labelText: 'Relay base URL',
                          hintText: 'https://your-server.example.com',
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
                              onPressed: controller.busy ||
                                      (!controller.isBleConnected && !controller.hasHttpTarget)
                                  ? null
                                  : () => _saveRelay(controller),
                              icon: const Icon(Icons.cloud_done_outlined),
                              label: const Text('Save to device'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy || !controller.hasRelayTarget
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
                  subtitle: 'Provision the desk device over BLE so it joins the same network as your phone.',
                  child: Column(
                    children: [
                      TextField(
                        controller: _wifiSsidController,
                        decoration: const InputDecoration(labelText: 'Wi-Fi name'),
                      ),
                      const SizedBox(height: 12),
                      TextField(
                        controller: _wifiPasswordController,
                        obscureText: true,
                        decoration: const InputDecoration(labelText: 'Wi-Fi password'),
                      ),
                      const SizedBox(height: 12),
                      SizedBox(
                        width: double.infinity,
                        child: ElevatedButton.icon(
                          onPressed: controller.busy || !controller.isBleConnected
                              ? null
                              : () => _sendWifi(controller),
                          icon: const Icon(Icons.wifi),
                          label: const Text('Send Wi-Fi credentials'),
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Sticky note',
                  subtitle: 'A centered message card on the OLED.',
                  child: Column(
                    children: [
                      TextField(
                        controller: _noteController,
                        minLines: 3,
                        maxLines: 5,
                        decoration: const InputDecoration(
                          labelText: 'Message',
                          hintText: 'good luck today, i packed snacks in your bag',
                        ),
                      ),
                      const SizedBox(height: 12),
                      Row(
                        children: [
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed: controller.busy ? null : () => _sendNote(controller),
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
                                        () => controller.clearDisplay(preferHttp: _preferHttp),
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
                        decoration: const InputDecoration(labelText: 'Banner text'),
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
                        onChanged: (value) => setState(() => _bannerSpeed = value),
                      ),
                      SizedBox(
                        width: double.infinity,
                        child: ElevatedButton.icon(
                          onPressed: controller.busy ? null : () => _sendBanner(controller),
                          icon: const Icon(Icons.view_carousel_outlined),
                          label: const Text('Start banner'),
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Draw live',
                  subtitle: 'The phone canvas is the OLED screen. Every pixel you set here maps straight to the display buffer.',
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      OledDrawingPad(
                        bitmap: _drawBitmap,
                        showGrid: _showGrid,
                        onPixel: _paintPixel,
                      ),
                      const SizedBox(height: 12),
                      Wrap(
                        spacing: 10,
                        runSpacing: 10,
                        children: [
                          FilterChip(
                            label: const Text('Pen'),
                            selected: !_eraserMode,
                            onSelected: (_) => setState(() => _eraserMode = false),
                          ),
                          FilterChip(
                            label: const Text('Eraser'),
                            selected: _eraserMode,
                            onSelected: (_) => setState(() => _eraserMode = true),
                          ),
                          FilterChip(
                            label: const Text('Grid'),
                            selected: _showGrid,
                            onSelected: (_) => setState(() => _showGrid = !_showGrid),
                          ),
                          FilterChip(
                            label: const Text('Live push'),
                            selected: _liveDraw,
                            onSelected: (_) => setState(() => _liveDraw = !_liveDraw),
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
                        onChanged: (value) => setState(() => _brushSize = value),
                      ),
                      Row(
                        children: [
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy
                                  ? null
                                  : () {
                                      setState(() => _drawBitmap = Uint8List(OledBitmapCodec.byteLength));
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
                              onPressed: controller.busy ? null : _loadImageIntoCanvas,
                              icon: const Icon(Icons.move_down_outlined),
                              label: const Text('Use picked image'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed: controller.busy ? null : () => _sendCanvas(controller),
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
                  subtitle: 'Any image is resized to 128×64 and converted to a 1-bit OLED bitmap.',
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
                        onChanged: (value) => setState(() => _invertImage = value),
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
                              onPressed: controller.busy || _selectedImage == null
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
      _drawBitmap = Uint8List.fromList(List<int>.filled(OledBitmapCodec.byteLength, 0xFF));
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
            preferHttp: _preferHttp,
          );
    });
  }

  Future<void> _sendWifi(DeskCompanionController controller) async {
    await _perform(
      () => controller.sendWifiCredentials(
        ssid: _wifiSsidController.text.trim(),
        password: _wifiPasswordController.text,
      ),
      success: 'Wi-Fi credentials sent. Use refresh once the display joins.',
    );
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
    await _perform(
      () => controller.sendNote(_noteController.text.trim(), preferHttp: _preferHttp),
      success: 'Note delivered.',
    );
  }

  Future<void> _sendBanner(DeskCompanionController controller) async {
    await _perform(
      () => controller.sendBanner(
        _bannerController.text.trim(),
        speed: _bannerSpeed.round(),
        preferHttp: _preferHttp,
      ),
      success: 'Banner started.',
    );
  }

  Future<void> _sendCanvas(DeskCompanionController controller) async {
    final payload = OledBitmapCodec.fromBitmap(
      bitmap: _drawBitmap,
      name: 'oled_drawing',
    );
    await _perform(
      () => controller.sendImage(payload, preferHttp: _preferHttp),
      success: 'Drawing delivered.',
    );
  }

  Future<void> _sendImage(DeskCompanionController controller) async {
    final payload = _selectedImage;
    if (payload == null) {
      return;
    }

    await _perform(
      () => controller.sendImage(payload, preferHttp: _preferHttp),
      success: 'Image delivered.',
    );
  }

  Future<void> _perform(Future<void> Function() action, {required String success}) async {
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
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(message)));
  }
}

class _HeroPanel extends StatelessWidget {
  const _HeroPanel({
    required this.bleState,
    required this.mode,
    required this.transportHint,
    required this.liveDraw,
  });

  final String bleState;
  final String mode;
  final String transportHint;
  final bool liveDraw;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(22),
      decoration: BoxDecoration(
        color: CompanionTheme.charcoal,
        borderRadius: BorderRadius.circular(28),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'Desk display studio',
            style: Theme.of(context)
                .textTheme
                .headlineMedium
                ?.copyWith(color: Colors.white),
          ),
          const SizedBox(height: 8),
          Text(
            'Send a note, a scrolling banner, a converted image, or draw directly on a 128×64 OLED template and mirror it live.',
            style: Theme.of(context)
                .textTheme
                .bodyLarge
                ?.copyWith(color: Colors.white70),
          ),
          const SizedBox(height: 16),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              _Pill(label: 'BLE ${bleState.toUpperCase()}'),
              _Pill(label: 'MODE ${mode.toUpperCase()}'),
              _Pill(label: transportHint.toUpperCase()),
              if (liveDraw) const _Pill(label: 'LIVE DRAW ON'),
            ],
          ),
        ],
      ),
    );
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

class _Pill extends StatelessWidget {
  const _Pill({required this.label});

  final String label;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(999),
      ),
      child: Text(
        label,
        style: Theme.of(context)
            .textTheme
            .bodyMedium
            ?.copyWith(color: Colors.white),
      ),
    );
  }
}