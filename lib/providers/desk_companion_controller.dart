import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

import '../models/companion_image_payload.dart';
import '../models/companion_scene.dart';
import '../models/companion_visual_model.dart';

enum CompanionBleState { disconnected, scanning, connecting, connected }

class DeskCompanionController extends ChangeNotifier {
  DeskCompanionController() {
    unawaited(_loadRelayPreferences());
  }

  static const String serviceUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f001';
  static const String commandUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f002';
  static const String statusUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f003';
  static const String imageUuid = '63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f004';
  static const String targetName = 'Desk Companion';
  static const List<String> targetNames = [
    'Desk Companion S3',
    'Desk Companion Mini',
  ];
  static const String _relayBaseUrlKey = 'relayBaseUrl';
  static const String _deviceTokenKey = 'deviceToken';
  static const String _connectedSsidKey = 'lastSsid';
  static const String _modeKey = 'lastMode';
  static const String _wifiNetworksKey = 'lastWifiNetworks';
  static const String _petPersonalityKey = 'petPersonality';
  static const String _activePetModeKey = 'activePetMode';
  static const String _visualModelKey = 'companionVisualModel';
  static const String _sceneKey = 'companionScene';
  static const String _stickFigureScaleKey = 'stickFigureScale';
  static const String _stickFigureSpacingKey = 'stickFigureSpacing';
  static const String _stickFigureEnergyKey = 'stickFigureEnergy';
  static const String _bondLevelKey = 'bondLevel';
  static const String _energyLevelKey = 'energyLevel';
  static const String _boredomLevelKey = 'boredomLevel';
  static const String _hairKey = 'companionHair';
  static const String _earsKey = 'companionEars';
  static const String _mustacheKey = 'companionMustache';
  static const String _glassesKey = 'companionGlasses';
  static const String _headwearKey = 'companionHeadwear';
  static const String _piercingKey = 'companionPiercing';
  static const String _hairSizeKey = 'companionHairSize';
  static const String _mustacheSizeKey = 'companionMustacheSize';
  static const String _hairWidthKey = 'companionHairWidth';
  static const String _hairHeightKey = 'companionHairHeight';
  static const String _hairThicknessKey = 'companionHairThickness';
  static const String _hairOffsetXKey = 'companionHairOffsetX';
  static const String _hairOffsetYKey = 'companionHairOffsetY';
  static const String _eyeOffsetYKey = 'companionEyeOffsetY';
  static const String _mouthOffsetYKey = 'companionMouthOffsetY';
  static const String _mustacheWidthKey = 'companionMustacheWidth';
  static const String _mustacheHeightKey = 'companionMustacheHeight';
  static const String _mustacheThicknessKey = 'companionMustacheThickness';
  static const String _mustacheOffsetXKey = 'companionMustacheOffsetX';
  static const String _mustacheOffsetYKey = 'companionMustacheOffsetY';

  CompanionBleState _bleState = CompanionBleState.disconnected;
  String _statusMessage = 'Ready to connect.';
  String _mode = 'idle';
  String _connectedSsid = '';
  String _deviceName = '';
  String _relayBaseUrl = 'http://desk-companion-relay.fly.dev';
  String _deviceToken = '';
  String _petPersonality = 'curious';
  String _activePetMode = 'hangout';
  String _companionVisualModel = CompanionVisualModel.classic.command;
  String _companionScene = CompanionScene.none.command;
  String _companionHair = 'none';
  String _companionEars = 'none';
  String _companionMustache = 'none';
  String _companionGlasses = 'none';
  String _companionHeadwear = 'none';
  String _companionPiercing = 'none';
  int _companionHairSize = 100;
  int _companionMustacheSize = 100;
  int _companionHairWidth = 100;
  int _companionHairHeight = 100;
  int _companionHairThickness = 100;
  int _companionHairOffsetX = 0;
  int _companionHairOffsetY = 0;
  int _companionEyeOffsetY = 0;
  int _companionMouthOffsetY = 0;
  int _companionMustacheWidth = 100;
  int _companionMustacheHeight = 100;
  int _companionMustacheThickness = 100;
  int _companionMustacheOffsetX = 0;
  int _companionMustacheOffsetY = 0;
  int _stickFigureScale = 100;
  int _stickFigureSpacing = 100;
  int _stickFigureEnergy = 55;
  int _bondLevel = 50;
  int _energyLevel = 72;
  int _boredomLevel = 28;
  List<String> _availableWifiNetworks = const [];
  String _wifiIpAddress = '';
  String? _lastRelayError;
  int _relayPendingCount = 0;
  DateTime? _relayLastCommandAt;
  DateTime? _relayLastSeenAt;
  DateTime? _relayLastStatusAt;
  bool _relayOnline = false;
  bool _relayStatusKnown = false;
  bool _busy = false;
  bool _wifiScanPending = false;
  bool _wifiConnectPending = false;

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
  Timer? _relayPollTimer;

  CompanionBleState get bleState => _bleState;
  String get statusMessage => _statusMessage;
  String get mode => _mode;
  String get connectedSsid => _connectedSsid;
  String get deviceName => _deviceName;
  String get relayBaseUrl => _relayBaseUrl;
  String get deviceToken => _deviceToken;
  String get petPersonality => _petPersonality;
  String get activePetMode => _activePetMode;
  String get companionVisualModel => _companionVisualModel;
  String get companionScene => _companionScene;
  String get companionHair => _companionHair;
  String get companionEars => _companionEars;
  String get companionMustache => _companionMustache;
  String get companionGlasses => _companionGlasses;
  String get companionHeadwear => _companionHeadwear;
  String get companionPiercing => _companionPiercing;
  int get companionHairSize => _companionHairSize;
  int get companionMustacheSize => _companionMustacheSize;
  int get companionHairWidth => _companionHairWidth;
  int get companionHairHeight => _companionHairHeight;
  int get companionHairThickness => _companionHairThickness;
  int get companionHairOffsetX => _companionHairOffsetX;
  int get companionHairOffsetY => _companionHairOffsetY;
  int get companionEyeOffsetY => _companionEyeOffsetY;
  int get companionMouthOffsetY => _companionMouthOffsetY;
  int get companionMustacheWidth => _companionMustacheWidth;
  int get companionMustacheHeight => _companionMustacheHeight;
  int get companionMustacheThickness => _companionMustacheThickness;
  int get companionMustacheOffsetX => _companionMustacheOffsetX;
  int get companionMustacheOffsetY => _companionMustacheOffsetY;
  int get stickFigureScale => _stickFigureScale;
  int get stickFigureSpacing => _stickFigureSpacing;
  int get stickFigureEnergy => _stickFigureEnergy;
  int get bondLevel => _bondLevel;
  int get energyLevel => _energyLevel;
  int get boredomLevel => _boredomLevel;
  List<String> get availableWifiNetworks => _availableWifiNetworks;
  String get wifiIpAddress => _wifiIpAddress;
  int get relayPendingCount => _relayPendingCount;
  DateTime? get relayLastCommandAt => _relayLastCommandAt;
  DateTime? get relayLastSeenAt => _relayLastSeenAt;
  DateTime? get relayLastStatusAt => _relayLastStatusAt;
  bool get isRelayOnline => _relayOnline;
  bool get isRemoteConnected => _relayOnline;
  bool get relayStatusKnown => _relayStatusKnown;
  bool get busy => _busy;
  bool get wifiScanPending => _wifiScanPending;
  bool get wifiConnectPending => _wifiConnectPending;
  bool get wifiConnected =>
      _connectedSsid.isNotEmpty && _wifiIpAddress.isNotEmpty;
  bool get isBleConnected => _bleState == CompanionBleState.connected;
  bool get hasRelayTarget =>
      _resolvedRelayUri != null && _deviceToken.trim().isNotEmpty;
  bool get canControlDevice => isBleConnected || _relayOnline;

  Uri? get _resolvedRelayUri {
    final sanitized = _sanitizeRelayBaseUrl(_relayBaseUrl);
    if (sanitized.isEmpty) {
      return null;
    }

    final withScheme =
        (sanitized.startsWith('http://') || sanitized.startsWith('https://'))
            ? sanitized
            : 'https://$sanitized';
    return Uri.tryParse('$withScheme/');
  }

  static String _sanitizeRelayBaseUrl(String value) {
    var url = value.trim();
    if (url.isEmpty) {
      return '';
    }

    final queryIndex = url.indexOf('?');
    if (queryIndex != -1) {
      url = url.substring(0, queryIndex);
    }

    final fragmentIndex = url.indexOf('#');
    if (fragmentIndex != -1) {
      url = url.substring(0, fragmentIndex);
    }

    final relayPathIndex = url.indexOf('/v1/device');
    if (relayPathIndex != -1) {
      url = url.substring(0, relayPathIndex);
    }

    final healthIndex = url.indexOf('/health');
    if (healthIndex != -1) {
      url = url.substring(0, healthIndex);
    }

    while (url.endsWith('/')) {
      url = url.substring(0, url.length - 1);
    }

    return url;
  }

  void updateRelayBaseUrl(String value) {
    _relayBaseUrl = _sanitizeRelayBaseUrl(value);
    unawaited(_persistRelayPreferences());
    _restartRelayPollingIfNeeded();
    notifyListeners();
  }

  void updateDeviceToken(String value) {
    _deviceToken = value.trim();
    unawaited(_persistRelayPreferences());
    _restartRelayPollingIfNeeded();
    notifyListeners();
  }

  Future<void> updateCompanionVisualModel(String value) async {
    _companionVisualModel = companionVisualModelFromCommand(value).command;
    await _persistRelayPreferences();
    notifyListeners();
  }

  Future<void> updateStudioPreviewSettings({
    String? visualModel,
    String? scene,
    int? stickFigureScale,
    int? stickFigureSpacing,
    int? stickFigureEnergy,
  }) async {
    if (visualModel != null) {
      _companionVisualModel = companionVisualModelFromCommand(visualModel).command;
    }
    if (scene != null) {
      _companionScene = companionSceneFromCommand(scene).command;
    }
    if (stickFigureScale != null) {
      _stickFigureScale = stickFigureScale;
    }
    if (stickFigureSpacing != null) {
      _stickFigureSpacing = stickFigureSpacing;
    }
    if (stickFigureEnergy != null) {
      _stickFigureEnergy = stickFigureEnergy;
    }

    await _persistRelayPreferences();
    notifyListeners();
  }

  Future<void> _loadRelayPreferences() async {
    final prefs = await SharedPreferences.getInstance();
    _relayBaseUrl = _sanitizeRelayBaseUrl(
      prefs.getString(_relayBaseUrlKey) ?? _relayBaseUrl,
    );
    _deviceToken = (prefs.getString(_deviceTokenKey) ?? '').trim();
    _connectedSsid = (prefs.getString(_connectedSsidKey) ?? '').trim();
    _mode = (prefs.getString(_modeKey) ?? _mode).trim();
    _petPersonality =
      (prefs.getString(_petPersonalityKey) ?? _petPersonality).trim();
    _activePetMode =
      (prefs.getString(_activePetModeKey) ?? _activePetMode).trim();
    _companionVisualModel = companionVisualModelFromCommand(
      prefs.getString(_visualModelKey),
    ).command;
    _companionScene = companionSceneFromCommand(
      prefs.getString(_sceneKey),
    ).command;
    _companionHair = (prefs.getString(_hairKey) ?? _companionHair).trim();
    _companionEars = (prefs.getString(_earsKey) ?? _companionEars).trim();
    _companionMustache =
        (prefs.getString(_mustacheKey) ?? _companionMustache).trim();
    _companionGlasses =
      (prefs.getString(_glassesKey) ?? _companionGlasses).trim();
    _companionHeadwear =
      (prefs.getString(_headwearKey) ?? _companionHeadwear).trim();
    _companionPiercing =
      (prefs.getString(_piercingKey) ?? _companionPiercing).trim();
    _companionHairSize = prefs.getInt(_hairSizeKey) ?? _companionHairSize;
    _companionMustacheSize =
        prefs.getInt(_mustacheSizeKey) ?? _companionMustacheSize;
    _companionHairWidth = prefs.getInt(_hairWidthKey) ?? _companionHairWidth;
    _companionHairHeight = prefs.getInt(_hairHeightKey) ?? _companionHairHeight;
    _companionHairThickness =
      prefs.getInt(_hairThicknessKey) ?? _companionHairThickness;
    _companionHairOffsetX =
      prefs.getInt(_hairOffsetXKey) ?? _companionHairOffsetX;
    _companionHairOffsetY =
      prefs.getInt(_hairOffsetYKey) ?? _companionHairOffsetY;
    _companionEyeOffsetY =
      prefs.getInt(_eyeOffsetYKey) ?? _companionEyeOffsetY;
    _companionMouthOffsetY =
      prefs.getInt(_mouthOffsetYKey) ?? _companionMouthOffsetY;
    _companionMustacheWidth =
      prefs.getInt(_mustacheWidthKey) ?? _companionMustacheWidth;
    _companionMustacheHeight =
      prefs.getInt(_mustacheHeightKey) ?? _companionMustacheHeight;
    _companionMustacheThickness =
      prefs.getInt(_mustacheThicknessKey) ?? _companionMustacheThickness;
    _companionMustacheOffsetX =
      prefs.getInt(_mustacheOffsetXKey) ?? _companionMustacheOffsetX;
    _companionMustacheOffsetY =
      prefs.getInt(_mustacheOffsetYKey) ?? _companionMustacheOffsetY;
    _stickFigureScale = prefs.getInt(_stickFigureScaleKey) ?? _stickFigureScale;
    _stickFigureSpacing = prefs.getInt(_stickFigureSpacingKey) ?? _stickFigureSpacing;
    _stickFigureEnergy = prefs.getInt(_stickFigureEnergyKey) ?? _stickFigureEnergy;
    _bondLevel = prefs.getInt(_bondLevelKey) ?? _bondLevel;
    _energyLevel = prefs.getInt(_energyLevelKey) ?? _energyLevel;
    _boredomLevel = prefs.getInt(_boredomLevelKey) ?? _boredomLevel;
    _availableWifiNetworks = const [];

    notifyListeners();

    if (hasRelayTarget) {
      _relayStatusKnown = true;
      _statusMessage = _connectedSsid.isNotEmpty
          ? 'Last known Wi-Fi: $_connectedSsid'
          : 'Checking relay status...';
      notifyListeners();
      _startRelayPollTimer();
      try {
        await refreshDeviceStatus();
      } catch (_) {
        // Keep the app usable if the initial relay check fails.
      }
    }
  }

  Future<void> _persistRelayPreferences() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_relayBaseUrlKey, _relayBaseUrl);
    await prefs.setString(_deviceTokenKey, _deviceToken);
    await prefs.setString(_connectedSsidKey, _connectedSsid);
    await prefs.setString(_modeKey, _mode);
    await prefs.setString(_petPersonalityKey, _petPersonality);
    await prefs.setString(_activePetModeKey, _activePetMode);
    await prefs.setString(_visualModelKey, _companionVisualModel);
    await prefs.setString(_sceneKey, _companionScene);
    await prefs.setString(_hairKey, _companionHair);
    await prefs.setString(_earsKey, _companionEars);
    await prefs.setString(_mustacheKey, _companionMustache);
    await prefs.setString(_glassesKey, _companionGlasses);
    await prefs.setString(_headwearKey, _companionHeadwear);
    await prefs.setString(_piercingKey, _companionPiercing);
    await prefs.setInt(_hairSizeKey, _companionHairSize);
    await prefs.setInt(_mustacheSizeKey, _companionMustacheSize);
    await prefs.setInt(_hairWidthKey, _companionHairWidth);
    await prefs.setInt(_hairHeightKey, _companionHairHeight);
    await prefs.setInt(_hairThicknessKey, _companionHairThickness);
    await prefs.setInt(_hairOffsetXKey, _companionHairOffsetX);
    await prefs.setInt(_hairOffsetYKey, _companionHairOffsetY);
    await prefs.setInt(_eyeOffsetYKey, _companionEyeOffsetY);
    await prefs.setInt(_mouthOffsetYKey, _companionMouthOffsetY);
    await prefs.setInt(_mustacheWidthKey, _companionMustacheWidth);
    await prefs.setInt(_mustacheHeightKey, _companionMustacheHeight);
    await prefs.setInt(_mustacheThicknessKey, _companionMustacheThickness);
    await prefs.setInt(_mustacheOffsetXKey, _companionMustacheOffsetX);
    await prefs.setInt(_mustacheOffsetYKey, _companionMustacheOffsetY);
    await prefs.setInt(_stickFigureScaleKey, _stickFigureScale);
    await prefs.setInt(_stickFigureSpacingKey, _stickFigureSpacing);
    await prefs.setInt(_stickFigureEnergyKey, _stickFigureEnergy);
    await prefs.setInt(_bondLevelKey, _bondLevel);
    await prefs.setInt(_energyLevelKey, _energyLevel);
    await prefs.setInt(_boredomLevelKey, _boredomLevel);
    await prefs.remove(_wifiNetworksKey);
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
      _setBleState(
        CompanionBleState.disconnected,
        'Desk Companion not found over BLE.',
      );
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
    _setBleState(
      CompanionBleState.connecting,
      'Connecting to ${device.platformName.isEmpty ? targetName : device.platformName}...',
    );
    await _scanSub?.cancel();

    try {
      await device.connect(autoConnect: false);
      _device = device;
      _deviceName = device.platformName.isEmpty ? targetName : device.platformName;

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
      _requireBleProvisioning();
      _connectedSsid = ssid.trim();
      _wifiIpAddress = '';
      _wifiConnectPending = true;
      _wifiScanPending = false;
      notifyListeners();
      await _sendBleCommand({
        'type': 'connect_wifi',
        'ssid': ssid,
        'password': password,
      });
      _setStatus('Sent Wi-Fi credentials over BLE. Waiting for the device to join...');
    });
  }

  Future<void> scanWifiNetworks() async {
    await _runBusy(() async {
      _requireBleProvisioning();
      _wifiScanPending = true;
      _wifiConnectPending = false;
      _availableWifiNetworks = const [];
      notifyListeners();
      unawaited(_persistRelayPreferences());
      await _sendBleCommand({'type': 'scan_wifi'});
      _setStatus('Scanning for Wi-Fi networks over BLE...');
    });
  }

  Future<void> forgetWifi() async {
    await _runBusy(() async {
      _requireBleProvisioning();
      await _sendBleCommand({'type': 'forget_wifi'});
      _connectedSsid = '';
      _wifiIpAddress = '';
      _availableWifiNetworks = const [];
      _wifiScanPending = false;
      _wifiConnectPending = false;
      await _persistRelayPreferences();
      _setStatus('Wi-Fi credentials cleared on device.');
    });
  }

  Future<void> configureRelay({
    required String relayUrl,
    required String token,
  }) async {
    await _runBusy(() async {
      _requireBleProvisioning();
      final sanitizedRelayUrl = _sanitizeRelayBaseUrl(relayUrl);
      final trimmedToken = token.trim();
      await _sendBleCommand({
        'type': 'set_relay',
        'relayUrl': sanitizedRelayUrl,
        'deviceToken': trimmedToken,
      });
      _relayBaseUrl = sanitizedRelayUrl;
      _deviceToken = trimmedToken;
      await _persistRelayPreferences();
      _restartRelayPollingIfNeeded();
      _setStatus('Relay configuration sent over BLE.');
    });
  }

  Future<bool> refreshDeviceStatus() async {
    final result = await _runBusy<bool>(() async {
      if (isBleConnected) {
        await _sendBleCommand({'type': 'status'});
        return true;
      }
      if (hasRelayTarget) {
        return _fetchRelayStatus();
      }
      return false;
    });
    return result ?? false;
  }

  Future<bool> connectRemoteDevice() async {
    final result = await _runBusy<bool>(() async {
      if (!hasRelayTarget) {
        _setStatus('Relay URL and device token are required first.');
        return false;
      }
      return _fetchRelayStatus();
    });
    return result ?? false;
  }

  Future<void> sendNote(
    String text, {
    required int fontSize,
    int border = 0,
    String icons = '',
    String flowerAccent = '',
  }) async {
    await _runBusy(() async {
      await _sendCommand(
        {
          'type': 'set_note',
          'text': text,
          'fontSize': fontSize,
          'border': border,
          if (icons.isNotEmpty) 'icons': icons,
          if (flowerAccent.isNotEmpty) 'flowerAccent': flowerAccent,
        },
        mode: 'note',
        bleLabel: 'Note sent over BLE.',
        relayLabel: 'Note queued through relay.',
      );
    });
  }

  Future<void> sendBanner(String text, {required int speed}) async {
    await _runBusy(() async {
      await _sendCommand(
        {'type': 'set_banner', 'text': text, 'speed': speed},
        mode: 'banner',
        bleLabel: 'Banner sent over BLE.',
        relayLabel: 'Banner queued through relay.',
      );
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

  Future<void> sendExpression(String expression) async {
    await _runBusy(() async {
      await _sendCommand(
        {'type': 'set_expression', 'expression': expression},
        mode: 'expression',
        bleLabel: 'Expression sent over BLE.',
        relayLabel: 'Expression queued through relay.',
      );
    });
  }

  Future<void> sendFlower(String type) async {
    await _runBusy(() async {
      await _sendCommand(
        {'type': 'set_flower', 'flower': type},
        mode: 'flower',
        bleLabel: 'Flower sent over BLE.',
        relayLabel: 'Flower queued through relay.',
      );
    });
  }

  Future<void> setPetPersonality(String personality) async {
    await _runBusy(() async {
      await _sendCommand(
        {'type': 'set_personality', 'personality': personality},
        mode: _mode,
        bleLabel: 'Personality sent over BLE.',
        relayLabel: 'Personality queued through relay.',
      );
      _petPersonality = personality.trim();
      await _persistRelayPreferences();
      notifyListeners();
    });
  }

  Future<void> triggerPetMode(String petMode) async {
    await _runBusy(() async {
      await _sendCommand(
        {'type': 'trigger_pet_mode', 'petMode': petMode},
        mode: _mode,
        bleLabel: 'Pet mode sent over BLE.',
        relayLabel: 'Pet mode queued through relay.',
      );
      _activePetMode = petMode.trim();
      await _persistRelayPreferences();
      notifyListeners();
    });
  }

  Future<void> sendCareAction(String action) async {
    await _runBusy(() async {
      await _sendCommand(
        {'type': 'care_action', 'action': action},
        mode: _mode,
        bleLabel: 'Care action sent over BLE.',
        relayLabel: 'Care action queued through relay.',
      );
    });
  }

  Future<void> setCompanionStyle({
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
    required int eyeOffsetY,
    required int mouthOffsetY,
    required int mustacheWidth,
    required int mustacheHeight,
    required int mustacheThickness,
    required int mustacheOffsetX,
    required int mustacheOffsetY,
  }) async {
    await _runBusy(() async {
      await _sendCommand(
        {
          'type': 'set_companion_style',
          'hair': hair,
          'ears': ears,
          'mustache': mustache,
          'glasses': glasses,
          'headwear': headwear,
          'piercing': piercing,
          'hairSize': hairSize,
          'mustacheSize': mustacheSize,
          'hairWidth': hairWidth,
          'hairHeight': hairHeight,
          'hairThickness': hairThickness,
          'hairOffsetX': hairOffsetX,
          'hairOffsetY': hairOffsetY,
          'eyeOffsetY': eyeOffsetY,
          'mouthOffsetY': mouthOffsetY,
          'mustacheWidth': mustacheWidth,
          'mustacheHeight': mustacheHeight,
          'mustacheThickness': mustacheThickness,
          'mustacheOffsetX': mustacheOffsetX,
          'mustacheOffsetY': mustacheOffsetY,
        },
        mode: _mode,
        bleLabel: 'Companion style sent over BLE.',
        relayLabel: 'Companion style queued through relay.',
      );
      _companionHair = hair.trim();
      _companionEars = ears.trim();
      _companionMustache = mustache.trim();
      _companionGlasses = glasses.trim();
      _companionHeadwear = headwear.trim();
      _companionPiercing = piercing.trim();
      _companionHairSize = hairSize;
      _companionMustacheSize = mustacheSize;
      _companionHairWidth = hairWidth;
      _companionHairHeight = hairHeight;
      _companionHairThickness = hairThickness;
      _companionHairOffsetX = hairOffsetX;
      _companionHairOffsetY = hairOffsetY;
      _companionEyeOffsetY = eyeOffsetY;
      _companionMouthOffsetY = mouthOffsetY;
      _companionMustacheWidth = mustacheWidth;
      _companionMustacheHeight = mustacheHeight;
      _companionMustacheThickness = mustacheThickness;
      _companionMustacheOffsetX = mustacheOffsetX;
      _companionMustacheOffsetY = mustacheOffsetY;
      await _persistRelayPreferences();
      notifyListeners();
    });
  }

  Future<void> sendLiveBitmap(Uint8List bitmap) async {
    if (!isBleConnected) {
      return;
    }

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
      await _sendCommand(
        {'type': 'clear'},
        mode: 'idle',
        bleLabel: 'Display cleared over BLE.',
        relayLabel: 'Display clear queued through relay.',
      );
    });
  }

  Future<void> _sendCommand(
    Map<String, dynamic> payload, {
    required String mode,
    required String bleLabel,
    required String relayLabel,
  }) async {
    if (isBleConnected) {
      await _sendBleCommand(payload);
      _mode = mode;
      _setStatus(bleLabel);
      return;
    }

    if (hasRelayTarget) {
      final sent = await _postRelay(payload);
      if (sent) {
        _mode = mode;
        _scheduleRelayDeliveryCheck(relayLabel);
        notifyListeners();
        return;
      }
      throw HttpException(_lastRelayError ?? 'Relay send failed.');
    }

    throw const HttpException('Not connected. Pair over BLE or configure a relay.');
  }

  Future<void> _sendBitmap(
    Uint8List bitmap, {
    required bool allowRelay,
    required bool silent,
  }) async {
    final canSendOverBle = isBleConnected &&
        _imageCharacteristic != null &&
        _commandCharacteristic != null;

    if (!canSendOverBle && allowRelay && hasRelayTarget) {
      if (await _postRelay({'type': 'set_image', 'data': base64Encode(bitmap)})) {
        _mode = 'image';
        if (!silent) {
          _scheduleRelayDeliveryCheck('Image queued through relay.');
        }
        return;
      }
      throw HttpException(_lastRelayError ?? 'Relay send failed.');
    }

    if (!canSendOverBle) {
      throw const HttpException('BLE is not connected and relay image send is unavailable.');
    }

    await _sendBleCommand({'type': 'begin_image', 'total': bitmap.length});

    final chunkSize = Platform.isAndroid ? 244 : 180;
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

  Future<bool> _postRelay(Map<String, dynamic> command) async {
    final base = _sanitizeRelayBaseUrl(_relayBaseUrl);
    final token = _deviceToken.trim();
    _lastRelayError = null;
    if (base.isEmpty || token.isEmpty) {
      _lastRelayError = 'Relay URL or token is missing.';
      return false;
    }

    final url = '$base/v1/device/${Uri.encodeComponent(token)}/command';
    try {
      final response = await http.post(
        Uri.parse(url),
        headers: const {'content-type': 'application/json'},
        body: jsonEncode({'command': command}),
      );
      if (response.statusCode >= 200 && response.statusCode < 300) {
        return true;
      }
      _lastRelayError = 'Relay returned ${response.statusCode}.';
      _setStatus(_lastRelayError!);
      return false;
    } catch (error) {
      _lastRelayError = 'Relay error: $error';
      _setStatus(_lastRelayError!);
      return false;
    }
  }

  Future<bool> _fetchRelayStatus() async {
    final base = _sanitizeRelayBaseUrl(_relayBaseUrl);
    final token = _deviceToken.trim();
    if (base.isEmpty || token.isEmpty) {
      _setStatus('Relay URL or token is empty.');
      return false;
    }

    final url = '$base/v1/device/${Uri.encodeComponent(token)}/status';
    try {
      final response = await http.get(Uri.parse(url));
      if (response.statusCode < 200 || response.statusCode >= 300) {
        _relayOnline = false;
        _relayStatusKnown = true;
        _setStatus('Relay returned ${response.statusCode}.');
        return false;
      }

      final payload = jsonDecode(response.body);
      if (payload is! Map<String, dynamic>) {
        _relayOnline = false;
        _relayStatusKnown = true;
        return false;
      }

      _relayPendingCount = (payload['pending'] as num?)?.toInt() ?? 0;
      final lastCommandAtValue = payload['lastCommandAt'] as String?;
      _relayLastCommandAt = lastCommandAtValue == null
          ? null
          : DateTime.tryParse(lastCommandAtValue)?.toLocal();

      final lastPullAtValue = payload['lastPullAt'] as String?;
      _relayLastSeenAt = lastPullAtValue == null
          ? null
          : DateTime.tryParse(lastPullAtValue)?.toLocal();
      final lastStatusAtValue = payload['lastStatusAt'] as String?;
      _relayLastStatusAt = lastStatusAtValue == null
          ? null
          : DateTime.tryParse(lastStatusAtValue)?.toLocal();
      _relayOnline = _relayLastSeenAt != null &&
          DateTime.now().difference(_relayLastSeenAt!) <=
              const Duration(seconds: 30);
      _relayStatusKnown = true;

      final lastStatus = payload['lastStatus'];
      if (lastStatus is Map<String, dynamic>) {
        _applyStatusMap(lastStatus);
      }

      if (!_relayOnline) {
        if (_relayLastStatusAt != null) {
          _setStatus(
            'Device reached the relay, but it is not polling for commands yet.',
          );
        } else {
          _setStatus('Device is not checking in to the relay.');
        }
      }

      notifyListeners();
      return _relayOnline;
    } catch (error) {
      _relayOnline = false;
      _relayStatusKnown = true;
      final message = error.toString();
      if (message.contains('host lookup') || message.contains('SocketException')) {
        _setStatus('Cannot reach relay. Check your internet connection.');
      } else {
        _setStatus('Relay error: $message');
      }
      return false;
    }
  }

  void _applyStatusMap(Map<String, dynamic> payload) {
    _mode = (payload['mode'] as String? ?? _mode).trim();
    _connectedSsid = (payload['ssid'] as String? ?? '').trim();
    _wifiIpAddress = (payload['ip'] as String? ?? '').trim();
    final status = (payload['status'] as String? ?? '').trim();
    if (status.isNotEmpty) {
      _statusMessage = status;
    }

    _updateWifiActivity(status);

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

    final incomingPersonality =
        (payload['personality'] as String? ?? '').trim();
    if (incomingPersonality.isNotEmpty) {
      _petPersonality = incomingPersonality;
    }

    final incomingPetMode = (payload['petMode'] as String? ?? '').trim();
    if (incomingPetMode.isNotEmpty) {
      _activePetMode = incomingPetMode;
    }

    final incomingHair = (payload['hair'] as String? ?? '').trim();
    if (incomingHair.isNotEmpty) {
      _companionHair = incomingHair;
    }

    final incomingEars = (payload['ears'] as String? ?? '').trim();
    if (incomingEars.isNotEmpty) {
      _companionEars = incomingEars;
    }

    final incomingMustache = (payload['mustache'] as String? ?? '').trim();
    if (incomingMustache.isNotEmpty) {
      _companionMustache = incomingMustache;
    }

    final incomingGlasses = (payload['glasses'] as String? ?? '').trim();
    if (incomingGlasses.isNotEmpty) {
      _companionGlasses = incomingGlasses;
    }

    final incomingHeadwear = (payload['headwear'] as String? ?? '').trim();
    if (incomingHeadwear.isNotEmpty) {
      _companionHeadwear = incomingHeadwear;
    }

    final incomingPiercing = (payload['piercing'] as String? ?? '').trim();
    if (incomingPiercing.isNotEmpty) {
      _companionPiercing = incomingPiercing;
    }

    final incomingHairSize = (payload['hairSize'] as num?)?.toInt();
    if (incomingHairSize != null) {
      _companionHairSize = incomingHairSize;
    }

    final incomingMustacheSize = (payload['mustacheSize'] as num?)?.toInt();
    if (incomingMustacheSize != null) {
      _companionMustacheSize = incomingMustacheSize;
    }

    final incomingHairWidth = (payload['hairWidth'] as num?)?.toInt();
    if (incomingHairWidth != null) {
      _companionHairWidth = incomingHairWidth;
    }

    final incomingHairHeight = (payload['hairHeight'] as num?)?.toInt();
    if (incomingHairHeight != null) {
      _companionHairHeight = incomingHairHeight;
    }

    final incomingHairThickness =
        (payload['hairThickness'] as num?)?.toInt();
    if (incomingHairThickness != null) {
      _companionHairThickness = incomingHairThickness;
    }

    final incomingHairOffsetX = (payload['hairOffsetX'] as num?)?.toInt();
    if (incomingHairOffsetX != null) {
      _companionHairOffsetX = incomingHairOffsetX;
    }

    final incomingHairOffsetY = (payload['hairOffsetY'] as num?)?.toInt();
    if (incomingHairOffsetY != null) {
      _companionHairOffsetY = incomingHairOffsetY;
    }

    final incomingEyeOffsetY = (payload['eyeOffsetY'] as num?)?.toInt();
    if (incomingEyeOffsetY != null) {
      _companionEyeOffsetY = incomingEyeOffsetY;
    }

    final incomingMouthOffsetY = (payload['mouthOffsetY'] as num?)?.toInt();
    if (incomingMouthOffsetY != null) {
      _companionMouthOffsetY = incomingMouthOffsetY;
    }

    final incomingMustacheWidth =
        (payload['mustacheWidth'] as num?)?.toInt();
    if (incomingMustacheWidth != null) {
      _companionMustacheWidth = incomingMustacheWidth;
    }

    final incomingMustacheHeight =
        (payload['mustacheHeight'] as num?)?.toInt();
    if (incomingMustacheHeight != null) {
      _companionMustacheHeight = incomingMustacheHeight;
    }

    final incomingMustacheThickness =
        (payload['mustacheThickness'] as num?)?.toInt();
    if (incomingMustacheThickness != null) {
      _companionMustacheThickness = incomingMustacheThickness;
    }

    final incomingMustacheOffsetX =
        (payload['mustacheOffsetX'] as num?)?.toInt();
    if (incomingMustacheOffsetX != null) {
      _companionMustacheOffsetX = incomingMustacheOffsetX;
    }

    final incomingMustacheOffsetY =
        (payload['mustacheOffsetY'] as num?)?.toInt();
    if (incomingMustacheOffsetY != null) {
      _companionMustacheOffsetY = incomingMustacheOffsetY;
    }

    final incomingBondLevel = (payload['bondLevel'] as num?)?.toInt();
    if (incomingBondLevel != null) {
      _bondLevel = incomingBondLevel;
    }

    final incomingEnergyLevel = (payload['energyLevel'] as num?)?.toInt();
    if (incomingEnergyLevel != null) {
      _energyLevel = incomingEnergyLevel;
    }

    final incomingBoredomLevel = (payload['boredomLevel'] as num?)?.toInt();
    if (incomingBoredomLevel != null) {
      _boredomLevel = incomingBoredomLevel;
    }

    final wifiNetworks = payload['wifiNetworks'];
    if (wifiNetworks is List) {
      final incoming = wifiNetworks
          .whereType<String>()
          .map((value) => value.trim())
          .where((value) => value.isNotEmpty)
          .toSet()
          .toList(growable: false);
      _availableWifiNetworks = incoming;
      _wifiScanPending = false;
    }

    unawaited(_persistRelayPreferences());
    notifyListeners();
  }

  void _updateWifiActivity(String status) {
    final normalized = status.toLowerCase();

    if (normalized.contains('scan queued') ||
        normalized.contains('scanning wi-fi')) {
      _wifiScanPending = true;
      _wifiConnectPending = false;
    }

    if (normalized.contains('wi-fi list updated') ||
        normalized.contains('no wi-fi found') ||
        normalized.contains('wi-fi scan failed')) {
      _wifiScanPending = false;
    }

    if (normalized.contains('wi-fi queued') ||
        normalized.contains('starting wi-fi') ||
        normalized.contains('joining wi-fi')) {
      _wifiConnectPending = true;
      _wifiScanPending = false;
    }

    if (normalized.contains('wi-fi connected') || wifiConnected) {
      _wifiConnectPending = false;
      _wifiScanPending = false;
    }

    if (normalized.contains('wi-fi failed') ||
        normalized.contains('wi-fi forgotten')) {
      _wifiConnectPending = false;
      _wifiScanPending = false;
    }
  }

  Future<void> _sendBleCommand(Map<String, dynamic> body) async {
    if (_commandCharacteristic == null || !isBleConnected) {
      throw const HttpException('BLE is not connected.');
    }
    await _commandCharacteristic!.write(
      utf8.encode(jsonEncode(body)),
      withoutResponse: false,
    );
  }

  void _requireBleProvisioning() {
    if (!isBleConnected) {
      throw const HttpException('BLE is required for Wi-Fi and relay setup.');
    }
  }

  void _scheduleRelayDeliveryCheck(String successLabel) {
    _setStatus('Sent via relay. Checking delivery...');
    Future.delayed(const Duration(seconds: 6), () async {
      if (isBleConnected || !hasRelayTarget) {
        return;
      }
      try {
        final base = _sanitizeRelayBaseUrl(_relayBaseUrl);
        final token = _deviceToken.trim();
        final url = '$base/v1/device/${Uri.encodeComponent(token)}/status';
        final response = await http.get(Uri.parse(url));
        if (response.statusCode < 200 || response.statusCode >= 300) {
          return;
        }
        final payload = jsonDecode(response.body);
        if (payload is! Map<String, dynamic>) {
          return;
        }
        final pending = (payload['pending'] as int?) ?? 0;
        final lastPullAtValue = payload['lastPullAt'] as String?;
        final lastPullAt = lastPullAtValue == null
            ? null
            : DateTime.tryParse(lastPullAtValue)?.toLocal();
        final isActivelyPolling = lastPullAt != null &&
            DateTime.now().difference(lastPullAt) <=
                const Duration(seconds: 30);
        if (pending == 0) {
          _setStatus('Delivered. $successLabel');
        } else if (!isActivelyPolling) {
          _setStatus('Queued on relay. Device is not polling remotely.');
        } else {
          _setStatus('Queued on relay. Waiting for the next device poll.');
        }
      } catch (_) {
        // Ignore follow-up failures; the last send status is good enough.
      }
    });
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

  void _restartRelayPollingIfNeeded() {
    if (hasRelayTarget) {
      _startRelayPollTimer();
    } else {
      _relayPollTimer?.cancel();
      _relayPendingCount = 0;
      _relayLastCommandAt = null;
      _relayStatusKnown = false;
      _relayOnline = false;
      _relayLastSeenAt = null;
      _relayLastStatusAt = null;
    }
  }

  void _startRelayPollTimer() {
    _relayPollTimer?.cancel();
    _relayPollTimer = Timer.periodic(const Duration(seconds: 30), (_) {
      if (!isBleConnected && hasRelayTarget && !_busy) {
        unawaited(_fetchRelayStatus());
      }
    });
  }

  @override
  void dispose() {
    _relayPollTimer?.cancel();
    _scanSub?.cancel();
    _notifySub?.cancel();
    _connectionSub?.cancel();
    super.dispose();
  }
}