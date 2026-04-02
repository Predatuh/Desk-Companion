import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/companion_image_payload.dart';
import '../providers/companion_device_provider.dart';
import '../theme/companion_theme.dart';
import '../utils/image_encoder.dart';

class CompanionHomeScreen extends StatefulWidget {
  const CompanionHomeScreen({super.key});

  @override
  State<CompanionHomeScreen> createState() => _CompanionHomeScreenState();
}

class _CompanionHomeScreenState extends State<CompanionHomeScreen> {
  final _wifiSsidController = TextEditingController();
  final _wifiPasswordController = TextEditingController();
  final _hostController = TextEditingController();
  final _noteController = TextEditingController();
  final _bannerController = TextEditingController(text: 'miss you already <3');

  CompanionImagePayload? _selectedImage;
  bool _preferHttp = true;
  bool _obscurePassword = true;
  bool _invertImage = false;
  double _bannerSpeed = 35;

  @override
  void dispose() {
    _wifiSsidController.dispose();
    _wifiPasswordController.dispose();
    _hostController.dispose();
    _noteController.dispose();
    _bannerController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<CompanionDeviceProvider>();

    if (_hostController.text != provider.manualHost) {
      _hostController.text = provider.manualHost;
      _hostController.selection = TextSelection.fromPosition(
        TextPosition(offset: _hostController.text.length),
      );
    }

    final canUseHttp = provider.hasHttpTarget;
    final transportHint = _preferHttp && canUseHttp
        ? 'Wi-Fi first, BLE fallback'
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
                  bleState: provider.bleState.name,
                  mode: provider.mode,
                  transportHint: transportHint,
                ),
                const SizedBox(height: 16),
                _SectionCard(
                  title: 'Device link',
                  subtitle: provider.statusMessage,
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Wrap(
                        spacing: 10,
                        runSpacing: 10,
                        children: [
                          _ChipLabel(
                            label: provider.isBleConnected
                                ? 'BLE: ${provider.deviceName}'
                                : 'BLE: disconnected',
                          ),
                          _ChipLabel(
                            label: provider.deviceIp.isEmpty
                                ? 'IP: no connection'
                                : 'IP: ${provider.deviceIp}',
                          ),
                          _ChipLabel(
                            label: provider.connectedSsid.isEmpty
                                ? 'Wi-Fi: not joined'
                                : 'Wi-Fi: ${provider.connectedSsid}',
                          ),
                        ],
                      ),
                      const SizedBox(height: 14),
                      Row(
                        children: [
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed: provider.busy
                                  ? null
                                  : provider.isBleConnected
                                      ? null
                                      : () => provider.scanAndConnect(),
                              icon: const Icon(Icons.bluetooth_searching),
                              label: const Text('Find device'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: provider.busy
                                  ? null
                                  : provider.isBleConnected
                                      ? () => provider.disconnect()
                                      : () => provider.refreshDeviceStatus(),
                              icon: Icon(provider.isBleConnected
                                  ? Icons.link_off
                                  : Icons.refresh),
                              label: Text(provider.isBleConnected
                                  ? 'Disconnect'
                                  : 'Refresh status'),
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 14),
                      TextField(
                        controller: _hostController,
                        onChanged: provider.updateManualHost,
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
                            : 'BLE will be used until the device reports an IP address.'),
                        value: _preferHttp,
                        onChanged: (value) => setState(() => _preferHttp = value),
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
                        obscureText: _obscurePassword,
                        decoration: InputDecoration(
                          labelText: 'Wi-Fi password',
                          suffixIcon: IconButton(
                            icon: Icon(_obscurePassword ? Icons.visibility_off : Icons.visibility),
                            onPressed: () => setState(() => _obscurePassword = !_obscurePassword),
                          ),
                        ),
                      ),
                      const SizedBox(height: 12),
                      SizedBox(
                        width: double.infinity,
                        child: ElevatedButton.icon(
                          onPressed: provider.busy || !provider.isBleConnected
                              ? null
                              : () => _sendWifi(provider),
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
                  subtitle: 'Plain message centered on the OLED.',
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
                              onPressed: provider.busy ? null : () => _sendNote(provider),
                              icon: const Icon(Icons.favorite_border),
                              label: const Text('Show note'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: provider.busy
                                  ? null
                                  : () => _perform(
                                        () => provider.clearDisplay(preferHttp: _preferHttp),
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
                          onPressed: provider.busy ? null : () => _sendBanner(provider),
                          icon: const Icon(Icons.view_carousel_outlined),
                          label: const Text('Start banner'),
                        ),
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
                        title: const Text('Invert image'),
                        value: _invertImage,
                        onChanged: (value) => setState(() => _invertImage = value),
                      ),
                      Row(
                        children: [
                          Expanded(
                            child: OutlinedButton.icon(
                              onPressed: provider.busy ? null : _pickImage,
                              icon: const Icon(Icons.image_outlined),
                              label: const Text('Pick image'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: ElevatedButton.icon(
                              onPressed: provider.busy || _selectedImage == null
                                  ? null
                                  : () => _sendImage(provider),
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
      final payload = DeskImageEncoder.encode(
        sourceBytes: bytes,
        name: file.name,
        invert: _invertImage,
      );
      setState(() => _selectedImage = payload);
    } on FormatException catch (error) {
      _showMessage(error.message);
    }
  }

  Future<void> _sendWifi(CompanionDeviceProvider provider) async {
    await _perform(
      () => provider.sendWifiCredentials(
        ssid: _wifiSsidController.text.trim(),
        password: _wifiPasswordController.text,
      ),
      success: 'Wi-Fi credentials sent. Use refresh once the display joins.',
    );
  }

  Future<void> _sendNote(CompanionDeviceProvider provider) async {
    await _perform(
      () => provider.sendNote(_noteController.text.trim(), preferHttp: _preferHttp),
      success: 'Note delivered.',
    );
  }

  Future<void> _sendBanner(CompanionDeviceProvider provider) async {
    await _perform(
      () => provider.sendBanner(
        _bannerController.text.trim(),
        speed: _bannerSpeed.round(),
        preferHttp: _preferHttp,
      ),
      success: 'Banner started.',
    );
  }

  Future<void> _sendImage(CompanionDeviceProvider provider) async {
    final payload = _selectedImage;
    if (payload == null) {
      return;
    }

    await _perform(
      () => provider.sendImage(
        payload,
        preferHttp: _preferHttp,
        invert: _invertImage,
      ),
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
  });

  final String bleState;
  final String mode;
  final String transportHint;

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
            'Cute desk display control',
            style: Theme.of(context)
                .textTheme
                .headlineMedium
                ?.copyWith(color: Colors.white),
          ),
          const SizedBox(height: 8),
          Text(
            'Send a quick note, a scrolling banner, or a tiny bitmap straight to the ESP32-S3 zero with its 1.3 inch I2C OLED.',
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