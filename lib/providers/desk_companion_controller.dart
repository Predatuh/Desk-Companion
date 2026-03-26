import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../models/companion_image_payload.dart';

enum CompanionBleState { disconnected, scanning, connecting, connected }

class DeskCompanionController extends ChangeNotifier {
  DeskCompanionController();

  static const String serviceUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f001';
  static const String commandUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f002';
  static const String statusUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f003';
  static const String imageUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f004';
  static const String targetName = 'Desk Companion S3';

  CompanionBleState _bleState = CompanionBleState.disconnected;
  String _statusMessage = 'Ready to connect.';
  String _mode = 'idle';
  String _deviceName = '';
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
  bool _intentionalDisconnect = false;

  CompanionBleState get bleState => _bleState;
  String get statusMessage => _statusMessage;
  String get mode => _mode;
  String get deviceName => _deviceName;
  bool get busy => _busy;
  bool get isBleConnected => _bleState == CompanionBleState.connected;

  Future<void> scanAndConnect() async {
    if (_bleState != CompanionBleState.disconnected) return;

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
      _deviceName =
          device.platformName.isEmpty ? targetName : device.platformName;

      // Request higher MTU for larger status payloads (wifi networks list)
      if (Platform.isAndroid) {
        await device.requestMtu(512);
      }

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
      _notifySub =
          _statusCharacteristic!.lastValueStream.listen(_handleStatusBytes);
      final currentStatus = await _statusCharacteristic!.read();
      _handleStatusBytes(currentStatus);
      _setBleState(CompanionBleState.connected, 'BLE connected.');
    } catch (error) {
      _handleDisconnection();
      _setStatus('BLE connection failed: $error');
    }
  }

  void _handleStatusBytes(List<int> bytes) {
    if (bytes.isEmpty) return;
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

  Future<void> refreshDeviceStatus() async {
    if (!isBleConnected) return;
    await _runBusy(() async {
      await _sendBleCommand({'type': 'status'});
    });
  }

  Future<void> sendNote(
    String text, {
    required int fontSize,
    int border = 0,
    String icons = '',
    String flowerAccent = '',
  }) async {
    await _runBusy(() async {
      await _sendBleCommand({
        'type': 'set_note',
        'text': text,
        'fontSize': fontSize,
        'border': border,
        if (icons.isNotEmpty) 'icons': icons,
        if (flowerAccent.isNotEmpty) 'flowerAccent': flowerAccent,
      });
      _mode = 'note';
      _setStatus('Note delivered.');
    });
  }

  Future<void> sendBanner(String text, {required int speed}) async {
    await _runBusy(() async {
      await _sendBleCommand({'type': 'set_banner', 'text': text, 'speed': speed});
      _mode = 'banner';
      _setStatus('Banner started.');
    });
  }

  Future<void> sendImage(CompanionImagePayload payload) async {
    await _runBusy(() async {
      await _sendBitmap(payload.bitmap, silent: false);
    });
  }

  Future<void> sendExpression(String expression) async {
    await _runBusy(() async {
      await _sendBleCommand({'type': 'set_expression', 'expression': expression});
      _mode = 'expression';
      _setStatus('Expression sent.');
    });
  }

  Future<void> sendFlower(String type) async {
    await _runBusy(() async {
      await _sendBleCommand({'type': 'set_flower', 'flower': type});
      _mode = 'flower';
      _setStatus('Flower sent.');
    });
  }

  Future<void> sendLiveBitmap(Uint8List bitmap) async {
    _queuedLiveBitmap = Uint8List.fromList(bitmap);
    if (_liveSendInFlight) return;
    _liveSendInFlight = true;
    try {
      while (_queuedLiveBitmap != null) {
        final nextBitmap = _queuedLiveBitmap!;
        _queuedLiveBitmap = null;
        await _sendBitmap(nextBitmap, silent: true);
      }
    } finally {
      _liveSendInFlight = false;
    }
  }

  Future<void> clearDisplay() async {
    await _runBusy(() async {
      await _sendBleCommand({'type': 'clear'});
      _mode = 'idle';
      _setStatus('Display cleared.');
    });
  }

  void _applyStatusMap(Map<String, dynamic> payload) {
    _mode = (payload['mode'] as String? ?? _mode).trim();
    final status = (payload['status'] as String? ?? '').trim();
    if (status.isNotEmpty) _statusMessage = status;
    notifyListeners();
  }

  Future<void> _sendBitmap(Uint8List bitmap, {required bool silent}) async {
    if (!isBleConnected ||
        _imageCharacteristic == null ||
        _commandCharacteristic == null) {
      throw Exception('BLE is not connected.');
    }

    await _sendBleCommand({'type': 'begin_image', 'total': bitmap.length});

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
    if (!silent) _setStatus('Image sent over BLE.');
  }

  Future<void> _sendBleCommand(Map<String, dynamic> body) async {
    if (_commandCharacteristic == null || !isBleConnected) {
      throw Exception('BLE is not connected.');
    }
    await _commandCharacteristic!.write(
      utf8.encode(jsonEncode(body)),
      withoutResponse: false,
    );
  }

  Future<void> disconnect() async {
    _intentionalDisconnect = true;
    await _scanSub?.cancel();
    await _notifySub?.cancel();
    await _connectionSub?.cancel();
    await _device?.disconnect();
    _handleDisconnection();
    _intentionalDisconnect = false;
  }

  void _handleDisconnection() {
    final wasConnected = _bleState == CompanionBleState.connected;
    _bleState = CompanionBleState.disconnected;
    _device = null;
    _commandCharacteristic = null;
    _statusCharacteristic = null;
    _imageCharacteristic = null;
    _deviceName = '';
    notifyListeners();

    if (wasConnected && !_intentionalDisconnect) {
      _setStatus('BLE disconnected. Reconnecting in 5s...');
      Future.delayed(const Duration(seconds: 5), () {
        if (_bleState == CompanionBleState.disconnected) {
          scanAndConnect();
        }
      });
    }
  }

  Future<T?> _runBusy<T>(Future<T> Function() action) async {
    if (_busy) {
      return null;
    }

    _busy = true;
    notifyListeners();
    try {
      return await action();
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
    super.dispose();
  }
}
