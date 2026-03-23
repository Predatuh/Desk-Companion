import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:http/http.dart' as http;

import '../models/companion_image_payload.dart';

enum CompanionBleState { disconnected, scanning, connecting, connected }

class DeskCompanionController extends ChangeNotifier {
  static const String serviceUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f001';
  static const String commandUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f002';
  static const String statusUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f003';
  static const String imageUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f004';
  static const String targetName = 'Desk Companion S3';

  CompanionBleState _bleState = CompanionBleState.disconnected;
  String _statusMessage = 'Ready to connect.';
  String _mode = 'idle';
  String _deviceIp = '';
  String _connectedSsid = '';
  String _deviceName = '';
  String _relayBaseUrl = '';
  String _deviceToken = '';
  List<String> _availableWifiNetworks = const [];
  String? _lastRelayError;
  bool _busy = false;

  BluetoothDevice? _device;
  BluetoothCharacteristic? _commandCharacteristic;
  BluetoothCharacteristic? _statusCharacteristic;
  BluetoothCharacteristic? _imageCharacteristic;

  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<List<int>>? _notifySub;
  StreamSubscription<BluetoothConnectionState>? _connectionSub;

  bool _liveSendInFlight = false;
  Uint8List? _queuedLiveBitmap;

  CompanionBleState get bleState => _bleState;
  String get statusMessage => _statusMessage;
  String get mode => _mode;
  String get deviceIp => _deviceIp;
  String get connectedSsid => _connectedSsid;
  String get deviceName => _deviceName;
  String get relayBaseUrl => _relayBaseUrl;
  String get deviceToken => _deviceToken;
  List<String> get availableWifiNetworks => _availableWifiNetworks;
  bool get busy => _busy;
  bool get isBleConnected => _bleState == CompanionBleState.connected;

  Uri? get _resolvedRelayUri {
    final sanitized = _sanitizeRelayBaseUrl(_relayBaseUrl);
    if (sanitized.isEmpty) return null;
    final withScheme = (sanitized.startsWith('http://') || sanitized.startsWith('https://'))
        ? sanitized
        : 'https://$sanitized';
    return Uri.tryParse('$withScheme/');
  }

  bool get hasRelayTarget => _resolvedRelayUri != null && _deviceToken.trim().isNotEmpty;

  static String _sanitizeRelayBaseUrl(String value) {
    var url = value.trim();
    if (url.isEmpty) return '';

    // Strip query string and fragment.
    final qIdx = url.indexOf('?');
    if (qIdx != -1) url = url.substring(0, qIdx);
    final hIdx = url.indexOf('#');
    if (hIdx != -1) url = url.substring(0, hIdx);

    // Strip known relay path suffixes so pasted full URLs still work.
    final v1Idx = url.indexOf('/v1/device');
    if (v1Idx != -1) url = url.substring(0, v1Idx);
    final healthIdx = url.indexOf('/health');
    if (healthIdx != -1) url = url.substring(0, healthIdx);

    // Remove trailing slashes.
    while (url.endsWith('/')) {
      url = url.substring(0, url.length - 1);
    }

    // Force https for known hosted platforms (Railway, Render, etc.)
    // to avoid HTTP→HTTPS redirects that break POST requests.
    if (url.startsWith('http://') && !url.contains('localhost') && !url.contains('127.0.0.1') && !url.contains('192.168.')) {
      url = 'https://${url.substring(7)}';
    }

    return url;
  }

  void updateRelayBaseUrl(String value) {
    _relayBaseUrl = _sanitizeRelayBaseUrl(value);
    notifyListeners();
  }

  void updateDeviceToken(String value) {
    _deviceToken = value.trim();
    notifyListeners();
  }

  Future<void> scanAndConnect() async {
    if (_bleState != CompanionBleState.disconnected) {
      return;
    }

    final adapterState = await FlutterBluePlus.adapterState.first;
    if (adapterState != BluetoothAdapterState.on) {
      _setStatus('Bluetooth is off. Turn it on and retry.');
      if (Platform.isAndroid) {
        await FlutterBluePlus.turnOn();
      }
      return;
    }

    _setBleState(CompanionBleState.scanning, 'Scanning for Desk Companion S3...');

    await FlutterBluePlus.stopScan();
    await _scanSub?.cancel();
    _scanSub = FlutterBluePlus.onScanResults.listen((results) {
      for (final result in results) {
        final nameMatches = result.device.platformName == targetName ||
            result.advertisementData.advName == targetName;
        final serviceMatches = result.advertisementData.serviceUuids.any(
          (uuid) => uuid.str.toLowerCase() == serviceUuid,
        );
        if (nameMatches || serviceMatches) {
          FlutterBluePlus.stopScan();
          unawaited(_connect(result.device));
          return;
        }
      }
    });

    await FlutterBluePlus.startScan(
      withServices: [Guid(serviceUuid)],
      timeout: const Duration(seconds: 10),
    );

    if (_bleState == CompanionBleState.scanning) {
      _setBleState(CompanionBleState.disconnected, 'Desk Companion not found over BLE.');
    }
  }

  Future<void> _connect(BluetoothDevice device) async {
    _setBleState(
      CompanionBleState.connecting,
      'Connecting to ${device.platformName.isEmpty ? targetName : device.platformName}...',
    );
    await _scanSub?.cancel();

    try {
      await device.connect(autoConnect: false);
      _device = device;
      _deviceName = device.platformName.isEmpty ? targetName : device.platformName;

      _connectionSub = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _handleDisconnection();
        }
      });

      final services = await device.discoverServices();
      for (final service in services) {
        if (service.uuid.str.toLowerCase() != serviceUuid) {
          continue;
        }
        for (final characteristic in service.characteristics) {
          final uuid = characteristic.uuid.str.toLowerCase();
          if (uuid == commandUuid) {
            _commandCharacteristic = characteristic;
          } else if (uuid == statusUuid) {
            _statusCharacteristic = characteristic;
          } else if (uuid == imageUuid) {
            _imageCharacteristic = characteristic;
          }
        }
      }

      if (_commandCharacteristic == null ||
          _statusCharacteristic == null ||
          _imageCharacteristic == null) {
        await disconnect();
        _setStatus('Connected device is missing required characteristics.');
        return;
      }

      await _statusCharacteristic!.setNotifyValue(true);
      _notifySub = _statusCharacteristic!.lastValueStream.listen(_handleStatusBytes);
      final currentStatus = await _statusCharacteristic!.read();
      _handleStatusBytes(currentStatus);
      _setBleState(CompanionBleState.connected, 'BLE connected.');
    } catch (error) {
      _handleDisconnection();
      _setStatus('BLE connection failed: $error');
    }
  }

  void _handleStatusBytes(List<int> bytes) {
    if (bytes.isEmpty) {
      return;
    }
    try {
      final decoded = utf8.decode(bytes).trim();
      final payload = jsonDecode(decoded);
      if (payload is Map<String, dynamic>) {
        _applyStatusMap(payload);
      }
    } catch (_) {
      _setStatus('Received unreadable status from device.');
    }
  }

  Future<void> sendWifiCredentials({
    required String ssid,
    required String password,
  }) async {
    await _runBusy(() async {
      await _sendBleCommand({
        'type': 'connect_wifi',
        'ssid': ssid,
        'password': password,
      });
      _setStatus('Sent Wi-Fi credentials over BLE.');
    });
  }

  Future<void> scanWifiNetworks() async {
    await _runBusy(() async {
      await _sendBleCommand({'type': 'scan_wifi'});
      _setStatus('Requested Wi-Fi scan from device.');
    });
  }

  Future<void> configureRelay({
    required String relayUrl,
    required String token,
  }) async {
    await _runBusy(() async {
      final sanitizedRelayUrl = _sanitizeRelayBaseUrl(relayUrl);
      await _sendBleCommand({
        'type': 'set_relay',
        'relayUrl': sanitizedRelayUrl,
        'deviceToken': token,
      });
      _relayBaseUrl = sanitizedRelayUrl;
      _deviceToken = token.trim();
      _setStatus('Relay configuration sent over BLE.');
    });
  }

  Future<void> refreshDeviceStatus() async {
    await _runBusy(() async {
      if (hasRelayTarget) {
        await _fetchRelayStatus();
        return;
      }
      await _sendBleCommand({'type': 'status'});
    });
  }

  Future<void> sendNote(String text) async {
    await _runBusy(() async {
      if (hasRelayTarget) {
        final sent = await _postRelay({'type': 'set_note', 'text': text});
        if (sent) {
          _mode = 'note';
          _setStatus('Note queued through relay.');
          return;
        }
        throw HttpException(_lastRelayError ?? 'Relay send failed.');
      }
      await _sendBleCommand({'type': 'set_note', 'text': text});
      _mode = 'note';
      _setStatus('Note sent over BLE.');
    });
  }

  Future<void> sendBanner(
    String text, {
    required int speed,
  }) async {
    await _runBusy(() async {
      if (hasRelayTarget) {
        final sent = await _postRelay({'type': 'set_banner', 'text': text, 'speed': speed});
        if (sent) {
          _mode = 'banner';
          _setStatus('Banner queued through relay.');
          return;
        }
        throw HttpException(_lastRelayError ?? 'Relay send failed.');
      }
      await _sendBleCommand({'type': 'set_banner', 'text': text, 'speed': speed});
      _mode = 'banner';
      _setStatus('Banner sent over BLE.');
    });
  }

  Future<void> sendImage(CompanionImagePayload payload) async {
    await _runBusy(() async {
      await _sendBitmap(
        payload.bitmap,
        allowRelay: true,
        silent: false,
      );
    });
  }

  Future<void> sendLiveBitmap(Uint8List bitmap) async {
    _queuedLiveBitmap = Uint8List.fromList(bitmap);
    if (_liveSendInFlight) {
      return;
    }

    _liveSendInFlight = true;
    try {
      while (_queuedLiveBitmap != null) {
        final nextBitmap = _queuedLiveBitmap!;
        _queuedLiveBitmap = null;
        await _sendBitmap(
          nextBitmap,
          allowRelay: false,
          silent: true,
        );
      }
    } finally {
      _liveSendInFlight = false;
    }
  }

  Future<void> clearDisplay() async {
    await _runBusy(() async {
      if (hasRelayTarget) {
        final sent = await _postRelay({'type': 'clear'});
        if (sent) {
          _mode = 'idle';
          _setStatus('Display clear queued through relay.');
          return;
        }
        throw HttpException(_lastRelayError ?? 'Relay send failed.');
      }
      await _sendBleCommand({'type': 'clear'});
      _mode = 'idle';
      _setStatus('Display cleared over BLE.');
    });
  }

  Future<bool> _postRelay(Map<String, dynamic> command) async {
    final base = _sanitizeRelayBaseUrl(_relayBaseUrl);
    final token = _deviceToken.trim();
    _lastRelayError = null;
    if (base.isEmpty || token.isEmpty) {
      _lastRelayError = 'Relay URL or token is missing.';
      return false;
    }

    final url = '$base/v1/device/${Uri.encodeComponent(token)}/command';
    debugPrint('[relay] POST $url');
    try {
      final response = await http.post(
        Uri.parse(url),
        headers: const {'content-type': 'application/json'},
        body: jsonEncode({'command': command}),
      );
      debugPrint('[relay] POST response: ${response.statusCode}');
      if (response.statusCode >= 200 && response.statusCode < 300) {
        _setStatus('Queued through relay.');
        return true;
      }
      _lastRelayError = 'Relay $url → ${response.statusCode}';
      _setStatus(_lastRelayError!);
      return false;
    } catch (error) {
      _lastRelayError = 'Relay $url → $error';
      _setStatus(_lastRelayError!);
      return false;
    }
  }

  Future<void> _fetchRelayStatus() async {
    final base = _sanitizeRelayBaseUrl(_relayBaseUrl);
    final token = _deviceToken.trim();
    if (base.isEmpty || token.isEmpty) {
      _setStatus('Relay URL or token is empty — cannot check relay.');
      return;
    }

    final url = '$base/v1/device/${Uri.encodeComponent(token)}/status';
    debugPrint('[relay] GET $url');
    try {
      final response = await http.get(Uri.parse(url));
      debugPrint('[relay] GET response: ${response.statusCode} ${response.body.length} bytes');
      if (response.statusCode < 200 || response.statusCode >= 300) {
        _setStatus('Relay $url → ${response.statusCode}');
        return;
      }

      final payload = jsonDecode(response.body);
      if (payload is! Map<String, dynamic>) {
        return;
      }

      _setStatus('Relay status refreshed.');
      final lastStatus = payload['lastStatus'];
      if (lastStatus is Map<String, dynamic>) {
        _applyStatusMap(lastStatus);
      }
    } catch (error) {
      _setStatus('Relay $url → $error');
    }
  }

  void _applyStatusMap(Map<String, dynamic> payload) {
    _mode = (payload['mode'] as String? ?? _mode).trim();
    _deviceIp = (payload['ip'] as String? ?? _deviceIp).trim();
    _connectedSsid = (payload['ssid'] as String? ?? _connectedSsid).trim();
    _statusMessage = (payload['status'] as String? ?? _statusMessage).trim();
    final incomingRelayUrl = _sanitizeRelayBaseUrl(
      payload['relayUrl'] as String? ?? '',
    );
    if (incomingRelayUrl.isNotEmpty) {
      _relayBaseUrl = incomingRelayUrl;
    }
    final incomingToken = (payload['deviceToken'] as String? ?? '').trim();
    if (incomingToken.isNotEmpty) {
      _deviceToken = incomingToken;
    }
    final wifiNetworks = payload['wifiNetworks'];
    if (wifiNetworks is List) {
      _availableWifiNetworks = wifiNetworks
          .whereType<String>()
          .map((value) => value.trim())
          .where((value) => value.isNotEmpty)
          .toSet()
          .toList(growable: false);
    }
    notifyListeners();
  }

  Future<void> _sendBitmap(
    Uint8List bitmap, {
    required bool allowRelay,
    required bool silent,
  }) async {
    if (allowRelay && hasRelayTarget) {
      if (await _postRelay({'type': 'set_image', 'data': base64Encode(bitmap)})) {
        _mode = 'image';
        if (!silent) {
          _setStatus('Image queued through relay.');
        }
        return;
      }
      throw HttpException(_lastRelayError ?? 'Relay send failed.');
    }

    await _sendBleCommand({
      'type': 'begin_image',
      'total': bitmap.length,
    });

    const chunkSize = 180;
    for (var offset = 0; offset < bitmap.length; offset += chunkSize) {
      final end = (offset + chunkSize < bitmap.length)
          ? offset + chunkSize
          : bitmap.length;
      await _imageCharacteristic!.write(
        bitmap.sublist(offset, end),
        withoutResponse: true,
      );
    }

    await _sendBleCommand({'type': 'commit_image'});
    _mode = 'image';
    if (!silent) {
      _setStatus('Image sent over BLE.');
    }
  }

  Future<void> _sendBleCommand(Map<String, dynamic> body) async {
    if (_commandCharacteristic == null || !isBleConnected) {
      throw HttpException('BLE is not connected.');
    }
    await _commandCharacteristic!.write(
      utf8.encode(jsonEncode(body)),
      withoutResponse: false,
    );
  }

  Future<void> disconnect() async {
    await _scanSub?.cancel();
    await _notifySub?.cancel();
    await _connectionSub?.cancel();
    await _device?.disconnect();
    _handleDisconnection();
  }

  void _handleDisconnection() {
    _bleState = CompanionBleState.disconnected;
    _device = null;
    _commandCharacteristic = null;
    _statusCharacteristic = null;
    _imageCharacteristic = null;
    _deviceName = '';
    notifyListeners();
  }

  Future<void> _runBusy(Future<void> Function() action) async {
    if (_busy) {
      return;
    }

    _busy = true;
    notifyListeners();
    try {
      await action();
    } finally {
      _busy = false;
      notifyListeners();
    }
  }

  void _setStatus(String value) {
    _statusMessage = value;
    notifyListeners();
  }

  void _setBleState(CompanionBleState value, String status) {
    _bleState = value;
    _statusMessage = status;
    notifyListeners();
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    _notifySub?.cancel();
    _connectionSub?.cancel();
    _device?.disconnect();
    super.dispose();
  }
}
