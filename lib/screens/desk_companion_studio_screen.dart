import 'dart:async';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';

import '../models/companion_image_payload.dart';
import '../providers/desk_companion_controller.dart';
import '../theme/companion_theme.dart';
import '../utils/oled_bitmap_codec.dart';
import '../widgets/oled_drawing_pad.dart';

const int kMaxNoteCharacters = 80;

class DeskCompanionStudioScreen extends StatefulWidget {
  const DeskCompanionStudioScreen({super.key});

  @override
  State<DeskCompanionStudioScreen> createState() => _DeskCompanionStudioScreenState();
}

class _DeskCompanionStudioScreenState extends State<DeskCompanionStudioScreen> {
  final _wifiPasswordController = TextEditingController();
  final _relayUrlController = TextEditingController();
  final _deviceTokenController = TextEditingController();
  final _noteController = TextEditingController();
  final _bannerController = TextEditingController(text: 'miss you already <3');
  String? _selectedWifiSsid;

  CompanionImagePayload? _selectedImage;
  Uint8List _drawBitmap = Uint8List(OledBitmapCodec.byteLength);
  bool _invertImage = false;
  bool _showGrid = true;
  bool _drawModeEnabled = false;
  bool _liveDraw = false;
  bool _eraserMode = false;
  double _bannerSpeed = 35;
  double _brushSize = 2;
  Timer? _liveSyncTimer;

  @override
  void dispose() {
    _wifiPasswordController.dispose();
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
    final availableWifiNetworks = controller.availableWifiNetworks;
    final selectedWifiSsid = _resolveSelectedWifiSsid(
      availableWifiNetworks,
      controller.connectedSsid,
    );

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
                              label: const Text('Connect device'),
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
                      Text(
                        canUseRelay
                            ? 'This build uses BLE for setup and relay for remote delivery.'
                            : 'Connect over BLE first, then save a relay URL and token for remote delivery.',
                        style: Theme.of(context).textTheme.bodyMedium,
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
                          hintText: 'http://your-server.example.com',
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
                              onPressed: controller.busy || !controller.isBleConnected
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
                  subtitle: 'Scan Wi-Fi from the device, pick a network, then send only the password.',
                  child: Column(
                    children: [
                      DropdownButtonFormField<String>(
                        value: selectedWifiSsid,
                        items: availableWifiNetworks
                            .map(
                              (ssid) => DropdownMenuItem<String>(
                                value: ssid,
                                child: Text(ssid, overflow: TextOverflow.ellipsis),
                              ),
                            )
                            .toList(growable: false),
                        onChanged: controller.busy || !controller.isBleConnected
                            ? null
                            : (value) => setState(() => _selectedWifiSsid = value),
                        decoration: const InputDecoration(
                          labelText: 'Wi-Fi network',
                          hintText: 'Scan from device first',
                        ),
                      ),
                      const SizedBox(height: 12),
                      TextField(
                        controller: _wifiPasswordController,
                        obscureText: true,
                        decoration: const InputDecoration(labelText: 'Wi-Fi password'),
                      ),
                      const SizedBox(height: 12),
                      Row(
                        children: [
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: controller.busy || !controller.isBleConnected
                                  ? null
                                  : () => _perform(
                                        () => controller.scanWifiNetworks(),
                                        success: 'Wi-Fi networks refreshed from device.',
                                      ),
                              icon: const Icon(Icons.wifi_find_outlined),
                              label: const Text('Scan networks'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed: controller.busy || !controller.isBleConnected || selectedWifiSsid == null
                                  ? null
                                  : () => _sendWifi(controller),
                              icon: const Icon(Icons.wifi),
                              label: const Text('Send password'),
                            ),
                          ),
                        ],
                      ),
                      if (availableWifiNetworks.isEmpty) ...[
                        const SizedBox(height: 10),
                        Align(
                          alignment: Alignment.centerLeft,
                          child: Text(
                            'No scanned networks yet. Connect over BLE and tap Scan networks.',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                        ),
                      ],
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
                        maxLength: kMaxNoteCharacters,
                        decoration: const InputDecoration(
                          labelText: 'Message',
                          hintText: 'good luck today, i packed snacks in your bag',
                          helperText: 'Up to 80 characters so the full note fits on screen.',
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
                            onSelected: (_) => setState(() => _drawModeEnabled = !_drawModeEnabled),
                          ),
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
      success: 'Wi-Fi credentials sent. Use refresh once the display joins.',
    );
  }

  String? _resolveSelectedWifiSsid(List<String> availableWifiNetworks, String connectedSsid) {
    if (_selectedWifiSsid != null && availableWifiNetworks.contains(_selectedWifiSsid)) {
      return _selectedWifiSsid;
    }
    if (connectedSsid.isNotEmpty && availableWifiNetworks.contains(connectedSsid)) {
      return connectedSsid;
    }
    if (availableWifiNetworks.isNotEmpty) {
      return availableWifiNetworks.first;
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
    final boundedNote = _noteController.text.trim();
    await _perform(
      () => controller.sendNote(
        boundedNote.length > kMaxNoteCharacters
            ? boundedNote.substring(0, kMaxNoteCharacters)
            : boundedNote,
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
            padding: EdgeInsets.fromLTRB(12, 12, 12, _controlsVisible ? 150 : 12),
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
                onPressed: () => setState(() => _controlsVisible = !_controlsVisible),
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
                                    setState(() => _bitmap = Uint8List(OledBitmapCodec.byteLength));
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