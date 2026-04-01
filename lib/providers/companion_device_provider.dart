import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:http/http.dart' as http;

import '../models/companion_image_payload.dart';

enum CompanionBleState { disconnected, scanning, connecting, connected }

class CompanionDeviceProvider extends ChangeNotifier {
  static const String serviceUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f001';
  static const String commandUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f002';
  static const String statusUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f003';
  static const String imageUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f004';
  static const String targetName = 'Desk Companion';
  static const List<String> targetNames = [
    'Desk Companion S3',
    'Desk Companion Mini',
  ];

  CompanionBleState _bleState = CompanionBleState.disconnected;
  CompanionBleState get bleState => _bleState;

  String _statusMessage = 'Ready to connect.';
  String get statusMessage => _statusMessage;

  String _mode = 'idle';
  String get mode => _mode;

  String _deviceIp = '';
  String get deviceIp => _deviceIp;

  String _connectedSsid = '';
  String get connectedSsid => _connectedSsid;

  String _manualHost = '';
  String get manualHost => _manualHost;

  String _deviceName = '';
  String get deviceName => _deviceName;

  bool _busy = false;
  bool get busy => _busy;

  BluetoothDevice? _device;
  BluetoothCharacteristic? _commandCharacteristic;
  BluetoothCharacteristic? _statusCharacteristic;
  BluetoothCharacteristic? _imageCharacteristic;

  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<List<int>>? _notifySub;
  StreamSubscription<BluetoothConnectionState>? _connectionSub;

  Uri? get _resolvedBaseUri {
    final rawHost = _manualHost.trim().isNotEmpty ? _manualHost.trim() : _deviceIp.trim();
    if (rawHost.isEmpty) {
      return null;
    }
    if (rawHost.startsWith('http://') || rawHost.startsWith('https://')) {
      return Uri.tryParse(rawHost);
    }
    return Uri.tryParse('http://$rawHost');
  }

  bool get hasHttpTarget => _resolvedBaseUri != null;
  bool get isBleConnected => _bleState == CompanionBleState.connected;

  void updateManualHost(String value) {
    _manualHost = value.trim();
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

    _setBleState(CompanionBleState.scanning, 'Scanning for Desk Companion devices...');

    await FlutterBluePlus.stopScan();
    await _scanSub?.cancel();
    _scanSub = FlutterBluePlus.onScanResults.listen((results) {
      for (final result in results) {
        final nameMatches = _matchesTargetName(result.device.platformName) ||
            _matchesTargetName(result.advertisementData.advName);
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

    await FlutterBluePlus.startScan(timeout: const Duration(seconds: 10));

    if (_bleState == CompanionBleState.scanning) {
      _setBleState(CompanionBleState.disconnected, 'Desk Companion not found over BLE.');
    }
  }

  bool _matchesTargetName(String? value) {
    final normalized = value?.trim().toLowerCase();
    if (normalized == null || normalized.isEmpty) {
      return false;
    }

    return normalized.startsWith(targetName.toLowerCase()) ||
        targetNames.any(
      (candidate) => candidate.toLowerCase() == normalized,
    );
  }

  Future<void> _connect(BluetoothDevice device) async {
    _setBleState(CompanionBleState.connecting, 'Connecting to ${device.platformName.isEmpty ? targetName : device.platformName}...');
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
      if (payload is! Map<String, dynamic>) {
        return;
      }

      _mode = (payload['mode'] as String? ?? _mode).trim();
      _deviceIp = (payload['ip'] as String? ?? _deviceIp).trim();
      _connectedSsid = (payload['ssid'] as String? ?? _connectedSsid).trim();
      _statusMessage = (payload['status'] as String? ?? _statusMessage).trim();
      notifyListeners();
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

  Future<void> refreshDeviceStatus() async {
    await _runBusy(() async {
      if (hasHttpTarget) {
        await _fetchHttpStatus();
        return;
      }
      await _sendBleCommand({'type': 'status'});
    });
  }

  Future<void> sendNote(String text, {required bool preferHttp}) async {
    await _runBusy(() async {
      final payload = {'text': text};
      if (preferHttp) {
        final sent = await _tryHttpPost('/api/note', payload);
        if (sent) {
          _mode = 'note';
          _setStatus('Note sent over Wi-Fi.');
          return;
        }
      }

      await _sendBleCommand({'type': 'set_note', ...payload});
      _mode = 'note';
      _setStatus('Note sent over BLE.');
    });
  }

  Future<void> sendBanner(
    String text, {
    required int speed,
    required bool preferHttp,
  }) async {
    await _runBusy(() async {
      final payload = {'text': text, 'speed': speed};
      if (preferHttp) {
        final sent = await _tryHttpPost('/api/banner', payload);
        if (sent) {
          _mode = 'banner';
          _setStatus('Banner sent over Wi-Fi.');
          return;
        }
      }

      await _sendBleCommand({'type': 'set_banner', ...payload});
      _mode = 'banner';
      _setStatus('Banner sent over BLE.');
    });
  }

  Future<void> sendImage(
    CompanionImagePayload payload, {
    required bool preferHttp,
    bool invert = false,
  }) async {
    await _runBusy(() async {
      if (preferHttp) {
        final sent = await _tryHttpPost('/api/image', {
          'data': base64Encode(payload.bitmap),
          'invert': invert,
        });
        if (sent) {
          _mode = 'image';
          _setStatus('Image sent over Wi-Fi.');
          return;
        }
      }

      await _sendBleCommand({
        'type': 'begin_image',
        'total': payload.byteLength,
        'invert': invert,
      });

      const chunkSize = 180;
      for (var offset = 0; offset < payload.bitmap.length; offset += chunkSize) {
        final end = (offset + chunkSize < payload.bitmap.length)
            ? offset + chunkSize
            : payload.bitmap.length;
        await _imageCharacteristic!.write(
          payload.bitmap.sublist(offset, end),
          withoutResponse: true,
        );
      }

      await _sendBleCommand({'type': 'commit_image'});
      _mode = 'image';
      _setStatus('Image sent over BLE.');
    });
  }

  Future<void> clearDisplay({required bool preferHttp}) async {
    await _runBusy(() async {
      if (preferHttp) {
        final sent = await _tryHttpPost('/api/clear', const {});
        if (sent) {
          _mode = 'idle';
          _setStatus('Display cleared over Wi-Fi.');
          return;
        }
      }

      await _sendBleCommand({'type': 'clear'});
      _mode = 'idle';
      _setStatus('Display cleared over BLE.');
    });
  }

  Future<bool> _tryHttpPost(String path, Map<String, dynamic> body) async {
    final baseUri = _resolvedBaseUri;
    if (baseUri == null) {
      return false;
    }

    final uri = baseUri.resolve(path);
    try {
      final response = await http.post(
        uri,
        headers: const {'content-type': 'application/json'},
        body: jsonEncode(body),
      );
      if (response.statusCode >= 200 && response.statusCode < 300) {
        await _fetchHttpStatus();
        return true;
      }
      _setStatus('Wi-Fi request failed: ${response.statusCode}');
      return false;
    } catch (_) {
      return false;
    }
  }

  Future<void> _fetchHttpStatus() async {
    final baseUri = _resolvedBaseUri;
    if (baseUri == null) {
      return;
    }

    final response = await http.get(baseUri.resolve('/api/status'));
    if (response.statusCode < 200 || response.statusCode >= 300) {
      _setStatus('Failed to read device status over Wi-Fi.');
      return;
    }

    final payload = jsonDecode(response.body);
    if (payload is Map<String, dynamic>) {
      _mode = (payload['mode'] as String? ?? _mode).trim();
      _deviceIp = (payload['ip'] as String? ?? _deviceIp).trim();
      _connectedSsid = (payload['ssid'] as String? ?? _connectedSsid).trim();
      _statusMessage = (payload['status'] as String? ?? _statusMessage).trim();
      notifyListeners();
    }
  }

  Future<void> _sendBleCommand(Map<String, dynamic> body) async {
    if (_commandCharacteristic == null || !isBleConnected) {
      throw const HttpException('BLE is not connected.');
    }

    final payload = utf8.encode(jsonEncode(body));
    await _commandCharacteristic!.write(payload, withoutResponse: false);
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