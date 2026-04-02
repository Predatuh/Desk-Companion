#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <BLE2902.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <ctype.h>
#include <mbedtls/base64.h>
#include <string>

#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 128
#endif

#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 64
#endif

#ifndef OLED_RESET
#define OLED_RESET -1
#endif

#ifndef OLED_ADDRESS
#define OLED_ADDRESS 0x3D
#endif

#ifndef OLED_SDA_PIN
#define OLED_SDA_PIN -1
#endif

#ifndef OLED_SCL_PIN
#define OLED_SCL_PIN -1
#endif

#ifndef BTN_NEXT_PIN
#define BTN_NEXT_PIN 4   // short press → next note
#endif

#ifndef BTN_CLEAR_PIN
#define BTN_CLEAR_PIN 5   // 5-second hold → clear display
#endif

#ifndef BTN_HOLD_MS
#define BTN_HOLD_MS 5000UL
#endif

#ifndef NOTE_QUEUE_MAX
#define NOTE_QUEUE_MAX 5
#endif

#ifndef NOTE_TEXT_MAX
#define NOTE_TEXT_MAX 80
#endif

#ifndef BOOT_WIFI_DELAY_MS
#define BOOT_WIFI_DELAY_MS 4000UL
#endif

#ifndef DESK_COMPANION_DEVICE_NAME
#define DESK_COMPANION_DEVICE_NAME "Desk Companion S3"
#endif

static const char* DEVICE_NAME = DESK_COMPANION_DEVICE_NAME;
static const char* SERVICE_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f001";
static const char* COMMAND_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f002";
static const char* STATUS_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f003";
static const char* IMAGE_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f004";

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Preferences preferences;
bool displayAvailable = false;
int activeOledSdaPin = OLED_SDA_PIN;
int activeOledSclPin = OLED_SCL_PIN;
uint8_t activeOledAddress = OLED_ADDRESS;
bool buttonsAvailable = false;

BLEServer* bleServer = nullptr;
BLECharacteristic* commandCharacteristic = nullptr;
BLECharacteristic* statusCharacteristic = nullptr;
BLECharacteristic* imageCharacteristic = nullptr;
WiFiClient relayHttpClient;
WiFiClientSecure relaySecureClient;

enum DisplayMode {
  MODE_IDLE,
  MODE_NOTE,
  MODE_BANNER,
  MODE_IMAGE,
  MODE_EXPRESSION,
  MODE_FLOWER,
};

DisplayMode currentMode = MODE_IDLE;
String statusText = "Booting";
String currentFlower = "rose";
String currentNote = "hi honey";
String currentBanner = "hello from your desk buddy";
String currentExpression = "happy";
String currentSsid = "";
String ipAddress = "";
String relayUrl = "";
String deviceToken = "";
String petPersonality = "curious";
String activePetMode = "hangout";
String activeCareAction = "";
String companionHair = "none";
String companionEars = "none";
String companionMustache = "none";
String companionGlasses = "none";
String companionHeadwear = "none";
String companionPiercing = "none";
int companionHairSize = 100;
int companionMustacheSize = 100;
int companionHairWidth = 100;
int companionHairHeight = 100;
int companionHairThickness = 100;
int companionHairOffsetX = 0;
int companionHairOffsetY = 0;
int companionEyeOffsetY = 0;
int companionMouthOffsetY = 0;
int companionMustacheWidth = 100;
int companionMustacheHeight = 100;
int companionMustacheThickness = 100;
int companionMustacheOffsetX = 0;
int companionMustacheOffsetY = 0;
String currentNoteFlowerAccent = "";
int currentNoteFontSize = 1;
int currentNoteBorder = 0;      // 0=none 1=rounded 2=stitched 3=hearts 4=dots
String currentNoteIcons = "";   // comma-separated: heart,star,flower,note,moon
int bannerSpeed = 35;
int bannerOffset = SCREEN_WIDTH;
unsigned long lastBannerTickMs = 0;
unsigned long lastDecorTickMs = 0;
unsigned long lastExpressionTickMs = 0;
unsigned long lastRelayPollMs = 0;
unsigned long lastRelayStatusPushMs = 0;
unsigned long lastRelaySuccessMs = 0;
unsigned long lastWifiCheckMs = 0;
unsigned long lastWifiBeginMs = 0;
unsigned long lastCompanionTickMs = 0;
unsigned long lastPetBeatMs = 0;
bool wifiJoinActive = false;
bool relayStatusDirty = true;
uint8_t idleOrbit = 0;
uint8_t expressionPhase = 0;
uint8_t petCycleStep = 0;
String availableWifiNetworks[10];
int availableWifiNetworkCount = 0;
DisplayMode transientResumeMode = MODE_IDLE;
String transientResumeStatus = "Ready";
String transientResumeExpression = "happy";
String transientResumeFlower = "rose";
bool transientActive = false;
unsigned long transientEndsAt = 0;
int bondLevel = 50;
int energyLevel = 72;
int boredomLevel = 28;

bool wifiConnectPending = false;
String pendingWifiSsid = "";
String pendingWifiPass = "";
bool wifiScanPending = false;
bool wifiWasConnected = false;
String storedWifiPass = "";
bool bootWifiRestorePending = false;
unsigned long bootCompletedAtMs = 0;

uint8_t imageBuffer[SCREEN_WIDTH * SCREEN_HEIGHT / 8] = {0};
size_t expectedImageBytes = 0;
size_t receivedImageBytes = 0;
bool imageTransferActive = false;

// Note queue
String noteQueue[NOTE_QUEUE_MAX];
int noteFontSizeQueue[NOTE_QUEUE_MAX] = {0};
int noteQueueCount = 0;
int noteQueueIndex = 0;

// Button state
bool btnNextLast = HIGH;
bool btnClearLast = HIGH;
bool btnClearHoldFired = false;
unsigned long btnNextDownMs = 0;
unsigned long btnClearDownMs = 0;

const char* modeName(DisplayMode mode);
bool beginHttpClient(HTTPClient& client, const String& url, uint16_t timeoutMs = 3000);
void clearImageBuffer();
bool decodeBase64IntoImage(const String& input);
void publishStatus();
void publishStatusWithNetworks();
String buildSlimStatusJson();
void drawWrappedText(const String& text, int fontSize, int border, const String& icons);
void renderBannerFrame();
void renderExpressionFrame();
void renderImage();
void renderIdle();
void renderCurrentMode();
String petDisplayLabel(const String& value);
String normalizePetPersonality(const String& value);
String normalizePetMode(const String& value);
String normalizeCareAction(const String& value);
String normalizeCompanionHair(const String& value);
String normalizeCompanionEars(const String& value);
String normalizeCompanionMustache(const String& value);
String normalizeCompanionGlasses(const String& value);
String normalizeCompanionHeadwear(const String& value);
String normalizeCompanionPiercing(const String& value);
String petAmbientStatus();
int clampLevel(int value);
int clampAppearancePercent(int value);
int clampAppearanceOffset(int value);
int scaleByPercent(int base, int percent);
String activePetBehavior();
String pickAutonomousPetExpression();
String pickReactionExpression(const String& trigger);
String currentAttentionStatus();
void persistPetState();
void setPetPersonality(const String& personality, bool persist = true);
void triggerPetMode(const String& petMode, bool persist = true);
void sendCareAction(const String& action);
void startTransientExpression(const String& expression, unsigned long durationMs, const String& nextStatus);
void restoreTransientMode();
void updateCompanionNeeds();
void updatePetBehavior();
void setIdleStatus(const String& value);
void setExpression(const String& expression);
void setNote(const String& text, int fontSize, int border, const String& icons, const String& flowerAccent = "");
void setBanner(const String& text, int speed);
void setImageReady();
void setFlower(const String& flowerType);
void saveRelaySettings(const String& nextRelayUrl, const String& nextDeviceToken);
bool connectToWifi(const String& ssid, const String& password);
bool wifiJoinInProgress();
void markWifiJoinStarted();
void markWifiJoinFinished();
void scanWifiNetworks();
void drawEye(int cx, int cy, int w, int h, int r, int pupilDx, int pupilDy);
void drawBlinkEye(int cx, int cy, int w, int h, int r);
void drawHappyArc(int cx, int cy, int w);
void drawSadArc(int cx, int cy, int w);
void drawSmile(int cx, int cy, int w);
void drawOvalMouth(int cx, int cy, int rw, int rh);
void drawIconHeart(int cx, int cy, int s);
void drawCompanionAccessories(int leftX, int rightX, int eyeY, int mouthY);
void renderFlowerFrame();
void drawNoteWithFlowerAccent(const String& text, int fontSize, int border, const String& icons, const String& flowerType);
void tryStoredPrefs();
void handleCommandJson(const String& body);
void pushRelayStatus();
void pollRelay();
void setupBle();
void setupDisplay();
void setupButtons();
void handleButtons();
bool probeDisplayOnPins(int sdaPin, int sclPin, uint8_t& foundAddr);
bool isButtonPinUsable(int pin);

String bleValueToString(const String& value) {
  return value;
}

String bleValueToString(const std::string& value) {
  return String(value.c_str());
}

size_t bleValueLength(const String& value) {
  return value.length();
}

size_t bleValueLength(const std::string& value) {
  return value.size();
}

const uint8_t* bleValueData(const String& value) {
  return reinterpret_cast<const uint8_t*>(value.c_str());
}

const uint8_t* bleValueData(const std::string& value) {
  return reinterpret_cast<const uint8_t*>(value.data());
}

String jsonEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (c == '\\' || c == '"') {
      escaped += '\\';
      escaped += c;
    } else if (c == '\n') {
      escaped += "\\n";
    } else if (c == '\r') {
      escaped += "\\r";
    } else if (c == '\t') {
      escaped += "\\t";
    } else {
      escaped += c;
    }
  }
  return escaped;
}

int findJsonValueStart(const String& body, const char* key) {
  const String needle = String("\"") + key + "\"";
  const int keyPos = body.indexOf(needle);
  if (keyPos < 0) {
    return -1;
  }

  int cursor = body.indexOf(':', keyPos + needle.length());
  if (cursor < 0) {
    return -1;
  }

  cursor++;
  while (cursor < body.length() && isspace(static_cast<unsigned char>(body[cursor]))) {
    cursor++;
  }
  return cursor;
}

String extractJsonStringField(const String& body, const char* key, const String& fallback = "") {
  int cursor = findJsonValueStart(body, key);
  if (cursor < 0 || cursor >= body.length() || body[cursor] != '"') {
    return fallback;
  }

  cursor++;
  String value;
  bool escaped = false;
  while (cursor < body.length()) {
    const char c = body[cursor++];
    if (escaped) {
      if (c == 'n') {
        value += '\n';
      } else if (c == 'r') {
        value += '\r';
      } else if (c == 't') {
        value += '\t';
      } else {
        value += c;
      }
      escaped = false;
      continue;
    }

    if (c == '\\') {
      escaped = true;
      continue;
    }

    if (c == '"') {
      return value;
    }

    value += c;
  }

  return fallback;
}

int extractJsonIntField(const String& body, const char* key, int fallback = 0) {
  int cursor = findJsonValueStart(body, key);
  if (cursor < 0 || cursor >= body.length()) {
    return fallback;
  }

  int sign = 1;
  if (body[cursor] == '-') {
    sign = -1;
    cursor++;
  }

  bool foundDigit = false;
  long value = 0;
  while (cursor < body.length() && isdigit(static_cast<unsigned char>(body[cursor]))) {
    foundDigit = true;
    value = (value * 10) + (body[cursor] - '0');
    cursor++;
  }

  return foundDigit ? static_cast<int>(value * sign) : fallback;
}

String buildStatusJson() {
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\",";
  json += "\"relayUrl\":\"" + jsonEscape(relayUrl) + "\",";
  json += "\"deviceToken\":\"" + jsonEscape(deviceToken) + "\",";
  json += "\"personality\":\"" + jsonEscape(petPersonality) + "\",";
  json += "\"petMode\":\"" + jsonEscape(activePetMode) + "\",";
  json += "\"hair\":\"" + jsonEscape(companionHair) + "\",";
  json += "\"ears\":\"" + jsonEscape(companionEars) + "\",";
  json += "\"mustache\":\"" + jsonEscape(companionMustache) + "\",";
  json += "\"glasses\":\"" + jsonEscape(companionGlasses) + "\",";
  json += "\"headwear\":\"" + jsonEscape(companionHeadwear) + "\",";
  json += "\"piercing\":\"" + jsonEscape(companionPiercing) + "\",";
  json += "\"hairSize\":" + String(companionHairSize) + ",";
  json += "\"mustacheSize\":" + String(companionMustacheSize) + ",";
  json += "\"hairWidth\":" + String(companionHairWidth) + ",";
  json += "\"hairHeight\":" + String(companionHairHeight) + ",";
  json += "\"hairThickness\":" + String(companionHairThickness) + ",";
  json += "\"hairOffsetX\":" + String(companionHairOffsetX) + ",";
  json += "\"hairOffsetY\":" + String(companionHairOffsetY) + ",";
  json += "\"eyeOffsetY\":" + String(companionEyeOffsetY) + ",";
  json += "\"mouthOffsetY\":" + String(companionMouthOffsetY) + ",";
  json += "\"mustacheWidth\":" + String(companionMustacheWidth) + ",";
  json += "\"mustacheHeight\":" + String(companionMustacheHeight) + ",";
  json += "\"mustacheThickness\":" + String(companionMustacheThickness) + ",";
  json += "\"mustacheOffsetX\":" + String(companionMustacheOffsetX) + ",";
  json += "\"mustacheOffsetY\":" + String(companionMustacheOffsetY) + ",";
  json += "\"bondLevel\":" + String(bondLevel) + ",";
  json += "\"energyLevel\":" + String(energyLevel) + ",";
  json += "\"boredomLevel\":" + String(boredomLevel);
  json += "}";
  return json;
}

String buildBleStatusJson() {
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\",";
  json += "\"personality\":\"" + jsonEscape(petPersonality) + "\",";
  json += "\"petMode\":\"" + jsonEscape(activePetMode) + "\",";
  json += "\"hair\":\"" + jsonEscape(companionHair) + "\",";
  json += "\"ears\":\"" + jsonEscape(companionEars) + "\",";
  json += "\"mustache\":\"" + jsonEscape(companionMustache) + "\",";
  json += "\"glasses\":\"" + jsonEscape(companionGlasses) + "\",";
  json += "\"headwear\":\"" + jsonEscape(companionHeadwear) + "\",";
  json += "\"piercing\":\"" + jsonEscape(companionPiercing) + "\",";
  json += "\"hairSize\":" + String(companionHairSize) + ",";
  json += "\"mustacheSize\":" + String(companionMustacheSize) + ",";
  json += "\"hairWidth\":" + String(companionHairWidth) + ",";
  json += "\"hairHeight\":" + String(companionHairHeight) + ",";
  json += "\"hairThickness\":" + String(companionHairThickness) + ",";
  json += "\"hairOffsetX\":" + String(companionHairOffsetX) + ",";
  json += "\"hairOffsetY\":" + String(companionHairOffsetY) + ",";
  json += "\"eyeOffsetY\":" + String(companionEyeOffsetY) + ",";
  json += "\"mouthOffsetY\":" + String(companionMouthOffsetY) + ",";
  json += "\"mustacheWidth\":" + String(companionMustacheWidth) + ",";
  json += "\"mustacheHeight\":" + String(companionMustacheHeight) + ",";
  json += "\"mustacheThickness\":" + String(companionMustacheThickness) + ",";
  json += "\"mustacheOffsetX\":" + String(companionMustacheOffsetX) + ",";
  json += "\"mustacheOffsetY\":" + String(companionMustacheOffsetY) + ",";
  json += "\"bondLevel\":" + String(bondLevel) + ",";
  json += "\"energyLevel\":" + String(energyLevel) + ",";
  json += "\"boredomLevel\":" + String(boredomLevel);
  json += "}";
  return json;
}

String buildSlimStatusJson() {
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\",";
  json += "\"personality\":\"" + jsonEscape(petPersonality) + "\",";
  json += "\"petMode\":\"" + jsonEscape(activePetMode) + "\",";
  json += "\"bondLevel\":" + String(bondLevel) + ",";
  json += "\"energyLevel\":" + String(energyLevel) + ",";
  json += "\"boredomLevel\":" + String(boredomLevel);
  json += "}";
  return json;
}

String buildBleStatusWithNetworksJson() {
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\",";
  json += "\"wifiNetworks\":[";
  for (int i = 0; i < availableWifiNetworkCount; i++) {
    if (i > 0) {
      json += ",";
    }
    json += "\"" + jsonEscape(availableWifiNetworks[i]) + "\"";
  }
  json += "]";
  json += "}";
  return json;
}

const char* modeName(DisplayMode mode) {
  switch (mode) {
    case MODE_NOTE:
      return "note";
    case MODE_BANNER:
      return "banner";
    case MODE_IMAGE:
      return "image";
    case MODE_EXPRESSION:
      return "expression";
    case MODE_FLOWER:
      return "flower";
    case MODE_IDLE:
    default:
      return "idle";
  }
}

bool beginHttpClient(HTTPClient& client, const String& url, uint16_t timeoutMs) {
  bool started = false;

  static bool secureInited = false;
  if (!secureInited) {
    relaySecureClient.setInsecure();
    secureInited = true;
  }

  // Always start a fresh outbound connection. Reused keep-alive sockets have
  // proven unreliable for long-lived relay polling on the ESP32.
  relaySecureClient.stop();
  relayHttpClient.stop();

  String finalUrl = url;
  if (finalUrl.startsWith("http://") && finalUrl.indexOf(".railway.app") != -1) {
    finalUrl = "https://" + finalUrl.substring(7);
  }

  if (finalUrl.startsWith("https://")) {
    started = client.begin(relaySecureClient, finalUrl);
  } else {
    started = client.begin(relayHttpClient, finalUrl);
  }

  if (started) {
    client.setReuse(false);
    client.setTimeout(timeoutMs);
  }
  return started;
}

void clearImageBuffer() {
  memset(imageBuffer, 0, sizeof(imageBuffer));
}

String petDisplayLabel(const String& value) {
  String label = value;
  label.replace("_", " ");
  if (label.length() > 0) {
    label.setCharAt(0, toupper(static_cast<unsigned char>(label[0])));
  }
  return label;
}

String normalizePetPersonality(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "playful" ||
      trimmed == "cuddly" ||
      trimmed == "sleepy" ||
      trimmed == "curious") {
    return trimmed;
  }
  return "curious";
}

String normalizePetMode(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "off" ||
      trimmed == "hangout" ||
      trimmed == "play" ||
      trimmed == "cuddle" ||
      trimmed == "nap" ||
      trimmed == "party" ||
      trimmed == "needy") {
    return trimmed;
  }
  return "hangout";
}

String normalizeCareAction(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "pet" ||
      trimmed == "cheer" ||
      trimmed == "comfort" ||
      trimmed == "dance" ||
      trimmed == "surprise") {
    return trimmed;
  }
  return "pet";
}

String normalizeCompanionHair(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" ||
      trimmed == "tuft" ||
      trimmed == "bangs" ||
      trimmed == "spiky" ||
      trimmed == "swoop" ||
      trimmed == "bob" ||
      trimmed == "messy") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionEars(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" ||
      trimmed == "cat" ||
      trimmed == "bear" ||
      trimmed == "bunny") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionMustache(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" ||
      trimmed == "classic" ||
      trimmed == "curled" ||
      trimmed == "handlebar" ||
      trimmed == "walrus" ||
      trimmed == "pencil" ||
      trimmed == "imperial") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionGlasses(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" ||
      trimmed == "round" ||
      trimmed == "square" ||
      trimmed == "visor") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionHeadwear(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" ||
      trimmed == "bow" ||
      trimmed == "beanie" ||
      trimmed == "crown") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionPiercing(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" ||
      trimmed == "brow" ||
      trimmed == "nose" ||
      trimmed == "lip") {
    return trimmed;
  }
  return "none";
}

String activePetBehavior() {
  if (activePetMode == "off") {
    return "off";
  }
  if (activePetMode != "hangout") {
    return activePetMode;
  }
  return petPersonality;
}

String petAmbientStatus() {
  if (!activeCareAction.isEmpty()) {
    return currentAttentionStatus();
  }
  if (activePetMode == "off") {
    return "Ready";
  }
  if (activePetMode == "play") {
    return "Bright-eyed";
  }
  if (activePetMode == "cuddle") {
    return "Close by";
  }
  if (activePetMode == "nap") {
    return "Quiet";
  }
  if (activePetMode == "party") {
    return "Sparked up";
  }
  if (activePetMode == "needy") {
    return "Looking your way";
  }
  return "Here with you";
}

int clampLevel(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return value;
}

int clampAppearancePercent(int value) {
  if (value < 70) {
    return 70;
  }
  if (value > 170) {
    return 170;
  }
  return value;
}

int clampAppearanceOffset(int value) {
  if (value < -24) {
    return -24;
  }
  if (value > 24) {
    return 24;
  }
  return value;
}

int scaleByPercent(int base, int percent) {
  const int clampedPercent = clampAppearancePercent(percent);
  const long scaled = (static_cast<long>(base) * clampedPercent) / 100L;
  if (scaled == 0 && base != 0) {
    return base > 0 ? 1 : -1;
  }
  return static_cast<int>(scaled);
}

String currentAttentionStatus() {
  if (activeCareAction == "pet") {
    return "Enjoying the moment";
  }
  if (activeCareAction == "cheer") {
    return "Lit up";
  }
  if (activeCareAction == "comfort") {
    return "Settled in";
  }
  if (activeCareAction == "dance") {
    return "All spark";
  }
  if (activeCareAction == "surprise") {
    return "Curious";
  }
  if (bondLevel < 30) {
    return "Nearby";
  }
  if (energyLevel < 25) {
    return "Taking it slow";
  }
  if (boredomLevel > 70) {
    return "Restless";
  }
  return "Here with you";
}

String pickAutonomousPetExpression() {
  const String behavior = activePetBehavior();
  const uint8_t cycle = petCycleStep++ % 4;
  if (behavior == "off") {
    return "happy";
  }
  if (behavior == "party") {
    if (cycle == 0) return "star_eyes";
    if (cycle == 1) return "laugh";
    if (cycle == 2) return "excited";
    return "tongue";
  }
  if (behavior == "play") {
    if (cycle == 0) return "excited";
    if (cycle == 1) return "laugh";
    if (cycle == 2) return "wink";
    return "tongue";
  }
  if (behavior == "cuddle") {
    if (cycle == 0) return "love";
    if (cycle == 1) return "kiss";
    if (cycle == 2) return "heart";
    return "smile";
  }
  if (behavior == "nap" || behavior == "sleepy") {
    if (cycle == 0) return "sleepy";
    if (cycle == 1) return "smile";
    if (cycle == 2) return "thinking";
    return "sleepy";
  }
  if (behavior == "needy") {
    if (cycle == 0) return "sad";
    if (cycle == 1) return "look_around";
    if (cycle == 2) return "kiss";
    return "surprised";
  }
  if (behavior == "playful") {
    if (cycle == 0) return "excited";
    if (cycle == 1) return "laugh";
    if (cycle == 2) return "wink";
    return "tongue";
  }
  if (behavior == "cuddly") {
    if (cycle == 0) return "love";
    if (cycle == 1) return "heart";
    if (cycle == 2) return "kiss";
    return "smile";
  }
  if (cycle == 0) return "look_around";
  if (cycle == 1) return "thinking";
  if (cycle == 2) return "confused";
  return "surprised";
}

String pickReactionExpression(const String& trigger) {
  if (trigger == "dance") {
    return "star_eyes";
  }
  if (trigger == "pet") {
    return petPersonality == "cuddly" ? "love" : "heart";
  }
  if (trigger == "cheer") {
    return "excited";
  }
  if (trigger == "comfort") {
    return petPersonality == "sleepy" ? "smile" : "kiss";
  }
  if (trigger == "surprise") {
    const uint8_t cycle = petCycleStep++ % 4;
    if (cycle == 0) return "surprised";
    if (cycle == 1) return "star_eyes";
    if (cycle == 2) return "laugh";
    return "tongue";
  }
  if (trigger == "button_clear") {
    return "sad";
  }
  if (trigger == "button_next") {
    return petPersonality == "playful" ? "wink" : "surprised";
  }
  if (trigger == "note") {
    return petPersonality == "cuddly" ? "love" : "thinking";
  }
  if (trigger == "banner") {
    return activePetMode == "party" ? "laugh" : "kiss";
  }
  if (trigger == "personality" || trigger == "pet_mode") {
    return pickAutonomousPetExpression();
  }
  return "happy";
}

void persistPetState() {
  preferences.begin("desk-cfg", false);
  preferences.putString("pet_personality", petPersonality);
  preferences.putString("pet_mode", activePetMode);
  preferences.putString("companion_hair", companionHair);
  preferences.putString("companion_ears", companionEars);
  preferences.putString("companion_mustache", companionMustache);
  preferences.putString("companion_glasses", companionGlasses);
  preferences.putString("companion_headwear", companionHeadwear);
  preferences.putString("companion_piercing", companionPiercing);
  preferences.putInt("companion_hair_size", companionHairSize);
  preferences.putInt("companion_mustache_size", companionMustacheSize);
  preferences.putInt("companion_hair_width", companionHairWidth);
  preferences.putInt("companion_hair_height", companionHairHeight);
  preferences.putInt("companion_hair_thickness", companionHairThickness);
  preferences.putInt("companion_hair_offset_x", companionHairOffsetX);
  preferences.putInt("companion_hair_offset_y", companionHairOffsetY);
  preferences.putInt("companion_eye_offset_y", companionEyeOffsetY);
  preferences.putInt("companion_mouth_offset_y", companionMouthOffsetY);
  preferences.putInt("companion_mustache_width", companionMustacheWidth);
  preferences.putInt("companion_mustache_height", companionMustacheHeight);
  preferences.putInt("companion_mustache_thickness", companionMustacheThickness);
  preferences.putInt("companion_mustache_offset_x", companionMustacheOffsetX);
  preferences.putInt("companion_mustache_offset_y", companionMustacheOffsetY);
  preferences.putInt("bond_level", bondLevel);
  preferences.putInt("energy_level", energyLevel);
  preferences.putInt("boredom_level", boredomLevel);
  preferences.end();
}

void restoreTransientMode() {
  if (!transientActive) {
    return;
  }

  transientActive = false;
  currentMode = transientResumeMode;
  currentExpression = transientResumeExpression;
  currentFlower = transientResumeFlower;
  statusText = currentMode == MODE_IDLE ? currentAttentionStatus() : transientResumeStatus;
  expressionPhase = 0;
  activeCareAction = "";
  renderCurrentMode();
  publishStatus();
}

void startTransientExpression(const String& expression, unsigned long durationMs, const String& nextStatus) {
  if (transientActive) {
    restoreTransientMode();
  }

  transientResumeMode = currentMode;
  transientResumeStatus = currentMode == MODE_IDLE ? currentAttentionStatus() : statusText;
  transientResumeExpression = currentExpression;
  transientResumeFlower = currentFlower;
  transientActive = true;
  transientEndsAt = millis() + durationMs;
  lastPetBeatMs = millis();

  currentExpression = expression;
  expressionPhase = 0;
  lastExpressionTickMs = 0;
  currentMode = MODE_EXPRESSION;
  statusText = nextStatus;
  renderCurrentMode();
  publishStatus();
}

void setPetPersonality(const String& personality, bool persist) {
  petPersonality = normalizePetPersonality(personality);
  if (persist) {
    persistPetState();
  }
  startTransientExpression(
    pickReactionExpression("personality"),
    2200,
    petAmbientStatus()
  );
}

void triggerPetMode(const String& petMode, bool persist) {
  activePetMode = normalizePetMode(petMode);
  activeCareAction = "";
  if (activePetMode == "party") {
    boredomLevel = clampLevel(boredomLevel - 20);
    energyLevel = clampLevel(energyLevel - 8);
  }
  if (persist) {
    persistPetState();
  }

  currentMode = MODE_IDLE;
  statusText = petAmbientStatus();
  lastPetBeatMs = 0;
  if (activePetMode == "off") {
    renderCurrentMode();
    publishStatus();
    return;
  }
  startTransientExpression(
    pickReactionExpression("pet_mode"),
    3200,
    petAmbientStatus()
  );
}

void sendCareAction(const String& action) {
  activeCareAction = normalizeCareAction(action);

  if (activeCareAction == "pet") {
    bondLevel = clampLevel(bondLevel + 8);
    boredomLevel = clampLevel(boredomLevel - 8);
  } else if (activeCareAction == "cheer") {
    energyLevel = clampLevel(energyLevel + 10);
    boredomLevel = clampLevel(boredomLevel - 6);
  } else if (activeCareAction == "comfort") {
    bondLevel = clampLevel(bondLevel + 5);
    energyLevel = clampLevel(energyLevel + 4);
  } else if (activeCareAction == "dance") {
    activePetMode = "party";
    energyLevel = clampLevel(energyLevel - 10);
    boredomLevel = clampLevel(boredomLevel - 18);
  } else if (activeCareAction == "surprise") {
    boredomLevel = clampLevel(boredomLevel - 12);
    energyLevel = clampLevel(energyLevel - 4);
  }

  persistPetState();
  startTransientExpression(
    pickReactionExpression(activeCareAction),
    activeCareAction == "dance" ? 3600 : 2200,
    currentAttentionStatus()
  );
}

void updateCompanionNeeds() {
  if (millis() - lastCompanionTickMs < 60000UL) {
    return;
  }

  lastCompanionTickMs = millis();
  boredomLevel = clampLevel(boredomLevel + 2);
  energyLevel = clampLevel(energyLevel - 1);
  if (bondLevel > 10) {
    bondLevel = clampLevel(bondLevel - 1);
  }

  if (energyLevel <= 20) {
    activePetMode = "nap";
  } else if (boredomLevel >= 80) {
    activePetMode = "needy";
  } else if (activePetMode == "needy" && boredomLevel <= 40) {
    activePetMode = "hangout";
  }

  persistPetState();
}

void updatePetBehavior() {
  updateCompanionNeeds();

  const unsigned long now = millis();
  if (transientActive && now >= transientEndsAt) {
    restoreTransientMode();
    return;
  }

  if (currentMode != MODE_IDLE || transientActive) {
    return;
  }

  if (activePetMode == "off") {
    return;
  }

  unsigned long intervalMs = 18000UL;
  if (activePetMode == "party") {
    intervalMs = 9000UL;
  } else if (activePetMode == "play") {
    intervalMs = 12000UL;
  } else if (activePetMode == "nap") {
    intervalMs = 26000UL;
  } else if (activePetMode == "needy") {
    intervalMs = 14000UL;
  } else if (petPersonality == "playful") {
    intervalMs = 13000UL;
  } else if (petPersonality == "sleepy") {
    intervalMs = 24000UL;
  }

  if (bondLevel < 30) {
    intervalMs = 12000UL;
  }
  if (boredomLevel > 75) {
    intervalMs = 10000UL;
  }

  if (now - lastPetBeatMs < intervalMs) {
    return;
  }

  startTransientExpression(pickAutonomousPetExpression(), 2200, currentAttentionStatus());
}

bool decodeBase64IntoImage(const String& input) {
  size_t decodedLength = 0;
  if (mbedtls_base64_decode(nullptr, 0, &decodedLength,
      reinterpret_cast<const unsigned char*>(input.c_str()), input.length()) != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
    return false;
  }

  if (decodedLength != sizeof(imageBuffer)) {
    return false;
  }

  size_t outputLength = 0;
  const int result = mbedtls_base64_decode(
    imageBuffer,
    sizeof(imageBuffer),
    &outputLength,
    reinterpret_cast<const unsigned char*>(input.c_str()),
    input.length()
  );

  return result == 0 && outputLength == sizeof(imageBuffer);
}

void publishStatus() {
  if (statusCharacteristic != nullptr) {
    const String slim = buildSlimStatusJson();
    statusCharacteristic->setValue(slim.c_str());
    statusCharacteristic->notify();
    const String full = buildBleStatusJson();
    statusCharacteristic->setValue(full.c_str());
  }
  relayStatusDirty = true;
}

void publishStatusWithNetworks() {
  const String payload = buildBleStatusWithNetworksJson();
  if (statusCharacteristic != nullptr) {
    statusCharacteristic->setValue(payload.c_str());
    statusCharacteristic->notify();
  }
  relayStatusDirty = true;
}

// ─── Native note decoration helpers ───

void drawIconHeart(int cx, int cy, int s) {
  display.fillCircle(cx - s, cy - s / 3, s, SH110X_WHITE);
  display.fillCircle(cx + s, cy - s / 3, s, SH110X_WHITE);
  display.fillTriangle(cx - s * 2, cy - s / 3 + 1, cx + s * 2, cy - s / 3 + 1, cx, cy + s * 2 - 1, SH110X_WHITE);
}

void drawIconStar(int cx, int cy, int r) {
  // Simple 6-point approximation using lines through center
  for (int i = 0; i < 6; i++) {
    float angle = i * 3.14159f / 3.0f;
    int x2 = cx + (int)(r * 0.97f * cos(angle));
    int y2 = cy + (int)(r * 0.97f * sin(angle));
    display.drawLine(cx, cy, x2, y2, SH110X_WHITE);
  }
  display.fillCircle(cx, cy, r / 3, SH110X_WHITE);
}

void drawIconFlower(int cx, int cy, int r) {
  for (int i = 0; i < 5; i++) {
    float angle = i * 3.14159f * 2.0f / 5.0f;
    int px = cx + (int)(r * cos(angle));
    int py = cy + (int)(r * sin(angle));
    display.fillCircle(px, py, r / 2, SH110X_WHITE);
  }
  display.fillCircle(cx, cy, r / 2 + 1, SH110X_WHITE);
}

void drawIconNote(int cx, int cy, int s) {
  display.fillCircle(cx - s / 2, cy + s - 1, s - 2, SH110X_WHITE);
  display.drawLine(cx - s / 2 + s - 3, cy + s - 1, cx - s / 2 + s - 3, cy - s, SH110X_WHITE);
  display.drawLine(cx - s / 2 + s - 3, cy - s, cx + s, cy - s + 2, SH110X_WHITE);
}

void drawIconMoon(int cx, int cy, int r) {
  display.fillCircle(cx, cy, r, SH110X_WHITE);
  display.fillCircle(cx + r / 2, cy - r / 3, r - 1, SH110X_BLACK);
}

void drawNoteBorder(int style) {
  switch (style) {
    case 1: // rounded outline
      display.drawRoundRect(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, 8, SH110X_WHITE);
      break;
    case 2: // stitched dashes
      for (int x = 6; x < SCREEN_WIDTH - 6; x += 7) {
        display.drawLine(x, 3, x + 3, 3, SH110X_WHITE);
        display.drawLine(x, SCREEN_HEIGHT - 4, x + 3, SCREEN_HEIGHT - 4, SH110X_WHITE);
      }
      for (int y = 6; y < SCREEN_HEIGHT - 6; y += 7) {
        display.drawLine(3, y, 3, y + 3, SH110X_WHITE);
        display.drawLine(SCREEN_WIDTH - 4, y, SCREEN_WIDTH - 4, y + 3, SH110X_WHITE);
      }
      break;
    case 3: // small hearts in corners
      drawIconHeart(9, 9, 3);
      drawIconHeart(SCREEN_WIDTH - 9, 9, 3);
      drawIconHeart(9, SCREEN_HEIGHT - 9, 3);
      drawIconHeart(SCREEN_WIDTH - 9, SCREEN_HEIGHT - 9, 3);
      break;
    case 4: // dot border
      for (int x = 5; x < SCREEN_WIDTH; x += 6) {
        display.fillCircle(x, 3, 1, SH110X_WHITE);
        display.fillCircle(x, SCREEN_HEIGHT - 4, 1, SH110X_WHITE);
      }
      for (int y = 9; y < SCREEN_HEIGHT - 6; y += 6) {
        display.fillCircle(3, y, 1, SH110X_WHITE);
        display.fillCircle(SCREEN_WIDTH - 4, y, 1, SH110X_WHITE);
      }
      break;
    default:
      break;
  }
}

void drawNoteIcons(const String& icons, int y, int count) {
  // Draw up to `count` icons centered on the given y, spaced 16px apart
  if (icons.length() == 0 || count == 0) return;
  // Parse comma-separated list into icon names
  String names[8];
  int nameCount = 0;
  String remaining = icons;
  while (remaining.length() > 0 && nameCount < 8) {
    int comma = remaining.indexOf(',');
    if (comma < 0) {
      names[nameCount++] = remaining;
      break;
    }
    names[nameCount++] = remaining.substring(0, comma);
    remaining = remaining.substring(comma + 1);
  }
  int drawCount = nameCount > count ? count : nameCount;
  int totalW = drawCount * 14;
  int startX = (SCREEN_WIDTH - totalW) / 2 + 7;
  for (int i = 0; i < drawCount; i++) {
    int cx = startX + i * 14;
    const String& name = names[i];
    if (name == "heart")  drawIconHeart(cx, y, 3);
    else if (name == "star")   drawIconStar(cx, y, 5);
    else if (name == "flower") drawIconFlower(cx, y, 4);
    else if (name == "note")   drawIconNote(cx, y, 5);
    else if (name == "moon")   drawIconMoon(cx, y, 4);
  }
}

// Draw a line of text with inline symbol replacement.
// Tokens: <3 = heart, <*> = star, <~> = flower, <n> = note, <m> = moon
void drawLineWithSymbols(const String& line, int startX, int startY, int fontSize) {
  int cx = startX;
  int cy = startY;
  int charW = 6 * fontSize;
  int charH = 8 * fontSize;
  int iconS = charH - 2; // icon size = roughly one character height
  int i = 0;
  while (i < (int)line.length()) {
    if (line[i] == '<' && i + 1 < (int)line.length()) {
      // Check for symbol tokens
      if (line[i+1] == '3') {
        // <3 = heart (2 chars consumed)
        drawIconHeart(cx + charW, cy + charH / 2, iconS / 3);
        cx += charW * 2;
        i += 2;
        continue;
      }
      if (i + 2 < (int)line.length() && line[i+2] == '>') {
        char sym = line[i+1];
        if (sym == '*') { drawIconStar(cx + charW, cy + charH / 2, iconS / 2); cx += charW * 2; i += 3; continue; }
        if (sym == '~') { drawIconFlower(cx + charW, cy + charH / 2, iconS / 3); cx += charW * 2; i += 3; continue; }
        if (sym == 'n') { drawIconNote(cx + charW, cy + charH / 2, iconS / 3); cx += charW * 2; i += 3; continue; }
        if (sym == 'm') { drawIconMoon(cx + charW, cy + charH / 2, iconS / 3); cx += charW * 2; i += 3; continue; }
      }
    }
    // Normal character
    display.setCursor(cx, cy);
    display.print(line[i]);
    cx += charW;
    i++;
  }
}

// Calculate the visual width of a line accounting for symbol tokens
int lineVisualWidth(const String& line, int fontSize) {
  int charW = 6 * fontSize;
  int w = 0;
  int i = 0;
  while (i < (int)line.length()) {
    if (line[i] == '<' && i + 1 < (int)line.length()) {
      if (line[i+1] == '3') { w += charW * 2; i += 2; continue; }
      if (i + 2 < (int)line.length() && line[i+2] == '>') {
        char sym = line[i+1];
        if (sym == '*' || sym == '~' || sym == 'n' || sym == 'm') { w += charW * 2; i += 3; continue; }
      }
    }
    w += charW;
    i++;
  }
  return w;
}

void drawWrappedText(const String& text, int fontSize, int border, const String& icons) {
  if (!displayAvailable) {
    return;
  }
  display.clearDisplay();
  drawNoteBorder(border);
  display.setTextColor(SH110X_WHITE);
  const int safeFontSize = fontSize < 1 ? 1 : (fontSize > 4 ? 4 : fontSize);
  const bool hasIcons = icons.length() > 0;
  const int topPad = hasIcons ? 14 : 4;
  const int horizontalPadding = (border > 0) ? 8 : 4;
  const int availH = SCREEN_HEIGHT - topPad - 4;
  const int lineHeight = (8 * safeFontSize) + 2;
  const int maxLines = availH / lineHeight;
  const int maxChars = (SCREEN_WIDTH - (horizontalPadding * 2)) / (6 * safeFontSize);
  String lines[maxLines];
  int lineCount = 0;
  String remaining = text;
  remaining.trim();

  while (remaining.length() > 0 && lineCount < maxLines) {
    int lineLength = (int)remaining.length() > maxChars ? maxChars : (int)remaining.length();
    if (lineLength < (int)remaining.length()) {
      const int split = remaining.lastIndexOf(' ', lineLength);
      if (split > 0) {
        lineLength = split;
      }
    }

    String line = remaining.substring(0, lineLength);
    line.trim();
    if (line.isEmpty()) {
      line = remaining.substring(0, maxChars);
      lineLength = line.length();
    }
    lines[lineCount++] = line;
    remaining = remaining.substring(lineLength);
    remaining.trim();
  }

  display.setTextSize(safeFontSize);
  display.setTextWrap(false);

  const int totalHeight = lineCount * lineHeight;
  int cursorY = topPad + (availH - totalHeight) / 2;
  if (cursorY < topPad) cursorY = topPad;

  for (int i = 0; i < lineCount; i++) {
    int w = lineVisualWidth(lines[i], safeFontSize);
    int startX = (SCREEN_WIDTH - w) / 2;
    drawLineWithSymbols(lines[i], startX, cursorY, safeFontSize);
    cursorY += lineHeight;
  }

  // Draw icons row centered at top
  if (hasIcons) drawNoteIcons(icons, 7, 7);

  display.display();
}

void renderBannerFrame() {
  if (!displayAvailable) {
    return;
  }
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setTextWrap(false);
  display.setCursor(bannerOffset, 25);
  display.print(currentBanner);
  display.display();
}

void renderImage() {
  if (!displayAvailable) {
    return;
  }
  display.clearDisplay();
  display.drawBitmap(0, 0, imageBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
  display.display();
}

void renderIdle() {
  if (!displayAvailable) {
    return;
  }
  display.clearDisplay();
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 10, SH110X_WHITE);

  const int leftX = 38;
  const int rightX = 90;
  const int eyeY = 31 + clampAppearanceOffset(companionEyeOffsetY);
  const int mouthY = 47 + clampAppearanceOffset(companionMouthOffsetY);
  const int pupilOffsets[4] = {-2, 0, 2, 0};
  const int pupilDx = pupilOffsets[idleOrbit % 4];

  if (activePetMode == "off") {
    drawEye(leftX, eyeY, 22, 14, 5, 0, 0);
    drawEye(rightX, eyeY, 22, 14, 5, 0, 0);
    display.drawLine(SCREEN_WIDTH / 2 - 7, mouthY, SCREEN_WIDTH / 2 + 7, mouthY, SH110X_WHITE);
  } else if (activePetMode == "nap" || petPersonality == "sleepy") {
    drawBlinkEye(leftX, eyeY, 24, 4, 4);
    drawBlinkEye(rightX, eyeY, 24, 4, 4);
    display.drawLine(SCREEN_WIDTH / 2 - 8, mouthY, SCREEN_WIDTH / 2 + 8, mouthY, SH110X_WHITE);
    display.setCursor(100, 23);
    display.print("z");
    display.setCursor(108, 18);
    display.print("z");
  } else if (activePetMode == "cuddle" || petPersonality == "cuddly") {
    drawEye(leftX, eyeY, 24, 16, 5, pupilDx / 2, 0);
    drawEye(rightX, eyeY, 24, 16, 5, pupilDx / 2, 0);
    drawSmile(SCREEN_WIDTH / 2, mouthY - 1, 24);
    drawIconHeart(64, 55, 3);
  } else if (activePetMode == "play" || petPersonality == "playful") {
    drawEye(leftX, eyeY, 26, 18, 5, pupilDx * 2, 0);
    drawEye(rightX, eyeY, 26, 18, 5, pupilDx * 2, 0);
    drawSmile(SCREEN_WIDTH / 2, mouthY - 2, 26);
    display.fillCircle(14 + (idleOrbit % 4) * 2, 50 - (idleOrbit % 2) * 2, 2, SH110X_WHITE);
  } else if (activePetMode == "party") {
    drawEye(leftX, eyeY, 26, 18, 5, pupilDx * 2, 0);
    drawEye(rightX, eyeY, 26, 18, 5, -pupilDx * 2, 0);
    drawSmile(SCREEN_WIDTH / 2, mouthY - 2, 28);
    drawIconStar(16, 14 + (idleOrbit % 2) * 2, 3);
    drawIconStar(112, 16 + ((idleOrbit + 1) % 2) * 2, 3);
  } else {
    drawEye(leftX, eyeY, 24, 16, 5, pupilDx * 2, 1);
    drawEye(rightX, eyeY, 24, 16, 5, -pupilDx, -1);
    drawOvalMouth(SCREEN_WIDTH / 2, mouthY, 5, 4);
  }

  drawCompanionAccessories(leftX, rightX, eyeY, mouthY);
  display.display();
}

void renderCurrentMode() {
  if (!displayAvailable) {
    return;
  }
  switch (currentMode) {
    case MODE_NOTE:
      if (currentNoteFlowerAccent.length() > 0) {
        drawNoteWithFlowerAccent(currentNote, currentNoteFontSize, currentNoteBorder, currentNoteIcons, currentNoteFlowerAccent);
      } else {
        drawWrappedText(currentNote, currentNoteFontSize, currentNoteBorder, currentNoteIcons);
      }
      break;
    case MODE_BANNER:
      renderBannerFrame();
      break;
    case MODE_IMAGE:
      renderImage();
      break;
    case MODE_EXPRESSION:
      renderExpressionFrame();
      break;
    case MODE_FLOWER:
      renderFlowerFrame();
      break;
    case MODE_IDLE:
    default:
      renderIdle();
      break;
  }
}

// ─── EMO-style expression drawing helpers ───

// Smooth easing helper: fast sine approximation 0→1→0 over 0..max
static inline float ease(int phase, int max) {
  float t = (float)phase / (float)max;
  return t < 0.5f ? 2.0f * t : 2.0f * (1.0f - t);
}

// Large EMO-style rounded-rect eye with smooth pupil
void drawEye(int cx, int cy, int w, int h, int r, int pupilDx, int pupilDy) {
  int clampedH = h < 3 ? 3 : h;
  display.fillRoundRect(cx - w / 2, cy - clampedH / 2, w, clampedH, r, SH110X_WHITE);
  if (clampedH >= 10) {
    // Large 5px pupil for EMO look
    display.fillCircle(cx + pupilDx, cy + pupilDy, 5, SH110X_BLACK);
  } else if (clampedH >= 6) {
    // Smaller pupil when eye is squeezing
    display.fillCircle(cx + pupilDx, cy + pupilDy, 3, SH110X_BLACK);
  }
}

// Smooth gradual blink — draws eye at a specific blink height
void drawBlinkEye(int cx, int cy, int w, int h, int r) {
  int clampedH = h < 3 ? 3 : h;
  display.fillRoundRect(cx - w / 2, cy - clampedH / 2, w, clampedH, r, SH110X_WHITE);
}

// Happy arc eyes (^^) — thickness based
void drawHappyArc(int cx, int cy, int w) {
  for (int t = 0; t < 5; t++) {
    int hw = w / 2;
    display.drawLine(cx - hw, cy + 5 + t, cx, cy - 5 + t, SH110X_WHITE);
    display.drawLine(cx, cy - 5 + t, cx + hw, cy + 5 + t, SH110X_WHITE);
  }
}

// Sad arc frown
void drawSadArc(int cx, int cy, int w) {
  for (int t = 0; t < 3; t++) {
    int hw = w / 2;
    display.drawLine(cx - hw, cy - 3 + t, cx, cy + 5 + t, SH110X_WHITE);
    display.drawLine(cx, cy + 5 + t, cx + hw, cy - 3 + t, SH110X_WHITE);
  }
}

// Big round smile
void drawSmile(int cx, int cy, int w) {
  for (int t = 0; t < 3; t++) {
    int hw = w / 2;
    display.drawLine(cx - hw, cy + t, cx - hw / 3, cy + 7 + t, SH110X_WHITE);
    display.drawLine(cx - hw / 3, cy + 7 + t, cx + hw / 3, cy + 7 + t, SH110X_WHITE);
    display.drawLine(cx + hw / 3, cy + 7 + t, cx + hw, cy + t, SH110X_WHITE);
  }
}

// Open-O mouth
void drawOvalMouth(int cx, int cy, int rw, int rh) {
  for (int t = 0; t < 2; t++) {
    display.drawRoundRect(cx - rw, cy - rh + t, rw * 2, rh * 2, rh, SH110X_WHITE);
  }
}

// Puckered kiss lips
void drawKissLips(int cx, int cy) {
  display.fillCircle(cx, cy, 6, SH110X_WHITE);
  display.fillCircle(cx, cy, 3, SH110X_BLACK);
}

// Filled heart
void drawBigHeart(int cx, int cy, int s) {
  display.fillCircle(cx - s, cy - s / 3, s, SH110X_WHITE);
  display.fillCircle(cx + s, cy - s / 3, s, SH110X_WHITE);
  display.fillTriangle(
    cx - s * 2, cy - s / 3 + 1,
    cx + s * 2, cy - s / 3 + 1,
    cx, cy + s * 2, SH110X_WHITE);
}

// Teardrop
void drawTear(int cx, int cy, int s) {
  display.fillCircle(cx, cy + s, s, SH110X_WHITE);
  display.fillTriangle(cx - s, cy + s, cx + s, cy + s, cx, cy, SH110X_WHITE);
}

// Eyebrow
void drawBrow(int x1, int y1, int x2, int y2) {
  for (int t = 0; t < 3; t++) {
    display.drawLine(x1, y1 + t, x2, y2 + t, SH110X_WHITE);
  }
}

void drawCompanionAccessories(int leftX, int rightX, int eyeY, int mouthY) {
  const int faceCenterX = (leftX + rightX) / 2;
  const int hairSize = clampAppearancePercent(companionHairSize);
  const int mustacheSize = clampAppearancePercent(companionMustacheSize);
  const int hairWidth = scaleByPercent(hairSize, companionHairWidth);
  const int hairHeight = scaleByPercent(hairSize, companionHairHeight);
  const int hairThickness = clampAppearancePercent(companionHairThickness);
  const int hairStroke = 1 + scaleByPercent(2, hairThickness);
  const int hairCenterX = faceCenterX + clampAppearanceOffset(companionHairOffsetX);
  const int hairCenterY = eyeY + clampAppearanceOffset(companionHairOffsetY);
  const int mustacheWidth = scaleByPercent(mustacheSize, companionMustacheWidth);
  const int mustacheHeight = scaleByPercent(mustacheSize, companionMustacheHeight);
  const int mustacheThickness = clampAppearancePercent(companionMustacheThickness);
  const int mustacheStroke = 1 + scaleByPercent(2, mustacheThickness);
  const int mustacheCenterX = faceCenterX + clampAppearanceOffset(companionMustacheOffsetX);
  const int mustacheCenterY = mouthY + clampAppearanceOffset(companionMustacheOffsetY);

  if (companionEars == "cat") {
    display.drawTriangle(leftX - 16, eyeY - 18, leftX - 8, eyeY - 30, leftX + 2, eyeY - 18, SH110X_WHITE);
    display.drawTriangle(rightX - 2, eyeY - 18, rightX + 8, eyeY - 30, rightX + 16, eyeY - 18, SH110X_WHITE);
  } else if (companionEars == "bear") {
    display.drawCircle(leftX - 12, eyeY - 20, 6, SH110X_WHITE);
    display.drawCircle(rightX + 12, eyeY - 20, 6, SH110X_WHITE);
  } else if (companionEars == "bunny") {
    display.drawRoundRect(leftX - 15, eyeY - 34, 8, 16, 4, SH110X_WHITE);
    display.drawRoundRect(rightX + 7, eyeY - 34, 8, 16, 4, SH110X_WHITE);
  }

  if (companionHair == "tuft") {
    const int lift = scaleByPercent(12, hairHeight);
    const int spread = scaleByPercent(5, hairWidth);
    for (int stroke = 0; stroke < hairStroke; stroke++) {
      display.drawLine(hairCenterX - spread, hairCenterY - 12 + stroke, hairCenterX, hairCenterY - 12 - lift + stroke, SH110X_WHITE);
      display.drawLine(hairCenterX, hairCenterY - 12 - lift + stroke, hairCenterX + spread, hairCenterY - 12 + stroke, SH110X_WHITE);
      display.drawLine(hairCenterX, hairCenterY - 12 - lift + stroke, hairCenterX + 1, hairCenterY - 8 + stroke, SH110X_WHITE);
    }
  } else if (companionHair == "bangs") {
    const int topY = hairCenterY - 12 - scaleByPercent(3, hairHeight);
    const int leftEdge = hairCenterX - (scaleByPercent(36, hairWidth) / 2);
    const int rightEdge = hairCenterX + (scaleByPercent(36, hairWidth) / 2);
    for (int stroke = 0; stroke < hairStroke; stroke++) {
      display.drawLine(leftEdge, topY + stroke, rightEdge, topY + stroke, SH110X_WHITE);
    }
    for (int x = leftX - 14; x <= rightX + 14; x += 8) {
      const int shiftedX = hairCenterX + (x - faceCenterX);
      for (int stroke = 0; stroke < hairStroke; stroke++) {
        display.drawLine(shiftedX, topY + 1 + stroke, shiftedX + scaleByPercent(3, hairWidth), hairCenterY - 7 + stroke, SH110X_WHITE);
      }
    }
  } else if (companionHair == "spiky") {
    for (int x = leftX - 14; x <= rightX + 14; x += 10) {
      const int shiftedX = hairCenterX + (x - faceCenterX);
      const int peak = hairCenterY - 10 - scaleByPercent(9, hairHeight);
      for (int stroke = 0; stroke < hairStroke; stroke++) {
        display.drawLine(shiftedX, hairCenterY - 10 + stroke, shiftedX + scaleByPercent(4, hairWidth), peak + stroke, SH110X_WHITE);
        display.drawLine(shiftedX + scaleByPercent(4, hairWidth), peak + stroke, shiftedX + scaleByPercent(8, hairWidth), hairCenterY - 10 + stroke, SH110X_WHITE);
      }
    }
  } else if (companionHair == "swoop") {
    const int topY = hairCenterY - 12 - scaleByPercent(6, hairHeight);
    for (int stroke = 0; stroke < hairStroke; stroke++) {
      display.drawLine(hairCenterX - scaleByPercent(18, hairWidth), hairCenterY - 9 + stroke, hairCenterX + scaleByPercent(12, hairWidth), topY + stroke, SH110X_WHITE);
      display.drawLine(hairCenterX + scaleByPercent(12, hairWidth), topY + stroke, hairCenterX + scaleByPercent(28, hairWidth), hairCenterY - 5 + stroke, SH110X_WHITE);
      display.drawLine(hairCenterX - scaleByPercent(10, hairWidth), hairCenterY - 10 + stroke, hairCenterX + scaleByPercent(4, hairWidth), topY + 2 + stroke, SH110X_WHITE);
    }
  } else if (companionHair == "bob") {
    const int topY = hairCenterY - 11 - scaleByPercent(4, hairHeight);
    const int width = scaleByPercent((rightX - leftX) + 36, hairWidth);
    display.drawRoundRect(hairCenterX - width / 2, topY, width, 10 + scaleByPercent(4, hairHeight), 5, SH110X_WHITE);
    for (int stroke = 0; stroke < hairStroke; stroke++) {
      display.drawLine(hairCenterX - width / 2, hairCenterY - 1 + stroke, hairCenterX - width / 2 + scaleByPercent(6, hairWidth), hairCenterY + 7 + stroke, SH110X_WHITE);
      display.drawLine(hairCenterX + width / 2, hairCenterY - 1 + stroke, hairCenterX + width / 2 - scaleByPercent(6, hairWidth), hairCenterY + 7 + stroke, SH110X_WHITE);
    }
  } else if (companionHair == "messy") {
    for (int x = leftX - 10; x <= rightX + 12; x += 9) {
      const int shiftedX = hairCenterX + (x - faceCenterX);
      const int peak = hairCenterY - 8 - scaleByPercent(7, hairHeight) + ((x / 9) % 2 == 0 ? 0 : 3);
      for (int stroke = 0; stroke < hairStroke; stroke++) {
        display.drawLine(shiftedX, hairCenterY - 8 + stroke, shiftedX + scaleByPercent(3, hairWidth), peak + stroke, SH110X_WHITE);
        display.drawLine(shiftedX + scaleByPercent(3, hairWidth), peak + stroke, shiftedX + scaleByPercent(6, hairWidth), hairCenterY - 9 + stroke, SH110X_WHITE);
      }
    }
  }

  if (companionHeadwear == "bow") {
    display.drawTriangle(faceCenterX - 4, eyeY - 24, faceCenterX - 16, eyeY - 18, faceCenterX - 8, eyeY - 12, SH110X_WHITE);
    display.drawTriangle(faceCenterX + 4, eyeY - 24, faceCenterX + 16, eyeY - 18, faceCenterX + 8, eyeY - 12, SH110X_WHITE);
    display.drawCircle(faceCenterX, eyeY - 18, 2, SH110X_WHITE);
  } else if (companionHeadwear == "beanie") {
    display.drawRoundRect(faceCenterX - 24, eyeY - 28, 48, 12, 5, SH110X_WHITE);
    display.drawLine(faceCenterX - 20, eyeY - 15, faceCenterX + 20, eyeY - 15, SH110X_WHITE);
    display.drawCircle(faceCenterX, eyeY - 30, 3, SH110X_WHITE);
  } else if (companionHeadwear == "crown") {
    display.drawLine(faceCenterX - 20, eyeY - 18, faceCenterX + 20, eyeY - 18, SH110X_WHITE);
    display.drawLine(faceCenterX - 20, eyeY - 18, faceCenterX - 12, eyeY - 30, SH110X_WHITE);
    display.drawLine(faceCenterX - 12, eyeY - 30, faceCenterX - 2, eyeY - 18, SH110X_WHITE);
    display.drawLine(faceCenterX - 2, eyeY - 18, faceCenterX + 6, eyeY - 32, SH110X_WHITE);
    display.drawLine(faceCenterX + 6, eyeY - 32, faceCenterX + 14, eyeY - 18, SH110X_WHITE);
    display.drawLine(faceCenterX + 14, eyeY - 18, faceCenterX + 20, eyeY - 28, SH110X_WHITE);
  }

  if (companionGlasses == "round") {
    display.drawCircle(leftX, eyeY, 12, SH110X_WHITE);
    display.drawCircle(rightX, eyeY, 12, SH110X_WHITE);
    display.drawLine(leftX + 12, eyeY, rightX - 12, eyeY, SH110X_WHITE);
  } else if (companionGlasses == "square") {
    display.drawRoundRect(leftX - 14, eyeY - 11, 28, 22, 4, SH110X_WHITE);
    display.drawRoundRect(rightX - 14, eyeY - 11, 28, 22, 4, SH110X_WHITE);
    display.drawLine(leftX + 14, eyeY, rightX - 14, eyeY, SH110X_WHITE);
  } else if (companionGlasses == "visor") {
    display.drawRoundRect(leftX - 18, eyeY - 12, (rightX - leftX) + 36, 20, 6, SH110X_WHITE);
  }

  if (companionMustache == "classic") {
    const int wing = scaleByPercent(12, mustacheWidth);
    const int inner = scaleByPercent(4, mustacheWidth);
    const int rise = scaleByPercent(4, mustacheHeight);
    for (int stroke = 0; stroke < mustacheStroke; stroke++) {
      display.drawLine(mustacheCenterX - wing, mustacheCenterY - rise + stroke, mustacheCenterX - inner, mustacheCenterY - 1 + stroke, SH110X_WHITE);
      display.drawLine(mustacheCenterX - wing, mustacheCenterY - rise + 1 + stroke, mustacheCenterX - inner, mustacheCenterY + 1 + stroke, SH110X_WHITE);
      display.drawLine(mustacheCenterX + inner, mustacheCenterY - 1 + stroke, mustacheCenterX + wing, mustacheCenterY - rise + stroke, SH110X_WHITE);
      display.drawLine(mustacheCenterX + inner, mustacheCenterY + 1 + stroke, mustacheCenterX + wing, mustacheCenterY - rise + 1 + stroke, SH110X_WHITE);
    }
  } else if (companionMustache == "curled") {
    const int wing = scaleByPercent(12, mustacheWidth);
    const int rise = scaleByPercent(2, mustacheHeight);
    for (int stroke = 0; stroke < mustacheStroke; stroke++) {
      display.drawLine(mustacheCenterX - wing, mustacheCenterY - rise + stroke, mustacheCenterX - 2, mustacheCenterY - 1 + stroke, SH110X_WHITE);
      display.drawLine(mustacheCenterX + 2, mustacheCenterY - 1 + stroke, mustacheCenterX + wing, mustacheCenterY - rise + stroke, SH110X_WHITE);
    }
    display.drawCircle(mustacheCenterX - wing - 2, mustacheCenterY - rise - 1, 2, SH110X_WHITE);
    display.drawCircle(mustacheCenterX + wing + 2, mustacheCenterY - rise - 1, 2, SH110X_WHITE);
  } else if (companionMustache == "handlebar") {
    const int wing = scaleByPercent(14, mustacheWidth);
    const int curl = scaleByPercent(5, mustacheHeight);
    for (int stroke = 0; stroke < mustacheStroke; stroke++) {
      display.drawLine(mustacheCenterX - wing, mustacheCenterY - 1 + stroke, mustacheCenterX - 2, mustacheCenterY + stroke, SH110X_WHITE);
      display.drawLine(mustacheCenterX + 2, mustacheCenterY + stroke, mustacheCenterX + wing, mustacheCenterY - 1 + stroke, SH110X_WHITE);
      display.drawLine(mustacheCenterX - wing, mustacheCenterY - 1 + stroke, mustacheCenterX - wing - scaleByPercent(4, mustacheWidth), mustacheCenterY - curl + stroke, SH110X_WHITE);
      display.drawLine(mustacheCenterX + wing, mustacheCenterY - 1 + stroke, mustacheCenterX + wing + scaleByPercent(4, mustacheWidth), mustacheCenterY - curl + stroke, SH110X_WHITE);
    }
  } else if (companionMustache == "walrus") {
    const int width = scaleByPercent(14, mustacheWidth);
    const int height = scaleByPercent(4, mustacheHeight);
    display.fillRoundRect(mustacheCenterX - width, mustacheCenterY - 6, width * 2, height + 2, 3, SH110X_WHITE);
    display.fillRect(mustacheCenterX - scaleByPercent(2, mustacheThickness), mustacheCenterY - 3, scaleByPercent(4, mustacheThickness), height + 4, SH110X_WHITE);
  } else if (companionMustache == "pencil") {
    const int width = scaleByPercent(13, mustacheWidth);
    for (int stroke = 0; stroke < mustacheStroke; stroke++) {
      display.drawLine(mustacheCenterX - width, mustacheCenterY - 2 + stroke, mustacheCenterX + width, mustacheCenterY - 2 + stroke, SH110X_WHITE);
      display.drawLine(mustacheCenterX - width + scaleByPercent(2, mustacheThickness), mustacheCenterY - 1 + stroke, mustacheCenterX + width - scaleByPercent(2, mustacheThickness), mustacheCenterY - 1 + stroke, SH110X_WHITE);
    }
  } else if (companionMustache == "imperial") {
    const int wing = scaleByPercent(12, mustacheWidth);
    const int rise = scaleByPercent(9, mustacheHeight);
    for (int stroke = 0; stroke < mustacheStroke; stroke++) {
      display.drawLine(mustacheCenterX - wing, mustacheCenterY - 2 + stroke, mustacheCenterX - 1, mustacheCenterY - 1 + stroke, SH110X_WHITE);
      display.drawLine(mustacheCenterX + 1, mustacheCenterY - 1 + stroke, mustacheCenterX + wing, mustacheCenterY - 2 + stroke, SH110X_WHITE);
      display.drawLine(mustacheCenterX - wing, mustacheCenterY - 2 + stroke, mustacheCenterX - wing - scaleByPercent(2, mustacheWidth), mustacheCenterY - rise + stroke, SH110X_WHITE);
      display.drawLine(mustacheCenterX + wing, mustacheCenterY - 2 + stroke, mustacheCenterX + wing + scaleByPercent(2, mustacheWidth), mustacheCenterY - rise + stroke, SH110X_WHITE);
    }
  }

  if (companionPiercing == "brow") {
    display.drawLine(rightX + 6, eyeY - 14, rightX + 14, eyeY - 12, SH110X_WHITE);
    display.drawPixel(rightX + 8, eyeY - 13, SH110X_WHITE);
  } else if (companionPiercing == "nose") {
    display.drawCircle(faceCenterX + 4, mouthY - 10, 2, SH110X_WHITE);
  } else if (companionPiercing == "lip") {
    display.drawCircle(faceCenterX + 8, mouthY + 2, 2, SH110X_WHITE);
  }
}

// ZZZ floating up
void drawZzz(int x, int y, int phase) {
  int p = phase % 32;
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  int drift = p / 2;
  if (p >= 4)  { display.setCursor(x,      y + 12 - drift);     display.print('z'); }
  if (p >= 12) { display.setCursor(x + 7,  y + 6 - drift / 2);  display.print('z'); }
  if (p >= 20) { display.setCursor(x + 14, y - drift / 3);       display.print('Z'); }
}

// ─── The main 32-phase expression renderer ───

void renderExpressionFrame() {
  if (!displayAvailable) {
    return;
  }
  display.clearDisplay();

  const int LX = 36;
  const int RX = 92;
  const int eyeYShift = clampAppearanceOffset(companionEyeOffsetY);
  const int mouthYShift = clampAppearanceOffset(companionMouthOffsetY);
  const int EY = 24 + eyeYShift;
  const int MY = 52 + mouthYShift;
  const int EW = 28;
  const int EH = 22;
  const int ER = 7;

  const int ph = expressionPhase % 32;
  const float t = (float)ph / 31.0f;

  if (currentExpression == "heart") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int s = 8 + (int)(6.0f * wave);
    drawBigHeart(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 2, s);
  } else if (currentExpression == "love") {
    drawEye(LX, EY, EW, EH, ER, 0, 0);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    drawBigHeart(LX, EY, 4);
    drawBigHeart(RX, EY, 4);
    drawSmile(SCREEN_WIDTH / 2, MY - 2, 28);

    float r1 = fmod(t * 2.0f, 1.0f);
    float r2 = fmod(t * 2.0f + 0.5f, 1.0f);
    if (r1 < 0.8f) {
      int y1 = MY - 4 - (int)(r1 * 40.0f);
      drawBigHeart(16 + (int)(r1 * 10.0f), y1 + eyeYShift, 3);
    }
    if (r2 < 0.8f) {
      int y2 = MY - 4 - (int)(r2 * 36.0f);
      drawBigHeart(112 - (int)(r2 * 10.0f), y2 + eyeYShift, 2);
    }
  } else if (currentExpression == "surprised") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int eyeH = EH + (int)(wave * 8.0f);
    int browLift = (int)(wave * 4.0f);
    int mouthR = 5 + (int)(wave * 3.0f);
    drawEye(LX, EY, EW + 4, eyeH, ER + 2, 0, 0);
    drawEye(RX, EY, EW + 4, eyeH, ER + 2, 0, 0);
    drawBrow(LX - 14, EY - 17 - browLift, LX + 14, EY - 17 - browLift);
    drawBrow(RX - 14, EY - 17 - browLift, RX + 14, EY - 17 - browLift);
    drawOvalMouth(SCREEN_WIDTH / 2, MY, mouthR, mouthR - 1);
  } else if (currentExpression == "angry") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int lidH = 4 + (int)(wave * 3.0f);
    int twitch = (int)(wave * 2.0f);
    int mouthShift = (int)(sin(t * 3.14159f * 6.0f) * 2.0f);
    drawEye(LX, EY + 2, EW, EH - 2, ER, 0, 2);
    drawEye(RX, EY + 2, EW, EH - 2, ER, 0, 2);
    display.fillRect(LX - EW / 2, EY + 2 - (EH - 2) / 2, EW, lidH, SH110X_BLACK);
    display.fillRect(RX - EW / 2, EY + 2 - (EH - 2) / 2, EW, lidH, SH110X_BLACK);
    drawBrow(LX - 14, EY - 16, LX + 8, EY - 6 - twitch);
    drawBrow(RX + 14, EY - 16, RX - 8, EY - 6 - twitch);
    for (int line = 0; line < 3; line++) {
      display.drawLine(SCREEN_WIDTH / 2 - 10, MY + line + mouthShift, SCREEN_WIDTH / 2 + 10, MY + line + mouthShift, SH110X_WHITE);
    }
  } else if (currentExpression == "sad") {
    drawEye(LX, EY + 3, EW, EH - 4, ER, 0, 4);
    drawEye(RX, EY + 3, EW, EH - 4, ER, 0, 4);
    drawBrow(LX - 14, EY - 6, LX + 10, EY - 16);
    drawBrow(RX + 14, EY - 6, RX - 10, EY - 16);
    drawSadArc(SCREEN_WIDTH / 2, MY - 2, 16);
    if (ph >= 12) {
      float tearT = (float)(ph - 12) / 19.0f;
      int tearY = EY + 14 + (int)(tearT * 24.0f);
      int tearS = tearT < 0.5f ? 3 : 2;
      drawTear(LX + EW / 2 + 2, tearY, tearS);
    }
    if (ph >= 20) {
      float tearT = (float)(ph - 20) / 11.0f;
      int tearY = EY + 14 + (int)(tearT * 22.0f);
      drawTear(RX + EW / 2 + 2, tearY, 2);
    }
  } else if (currentExpression == "sleepy") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int eyeH = 5 + (int)((1.0f - wave) * 5.0f);
    drawEye(LX, EY, EW, eyeH, ER, 0, 0);
    drawEye(RX, EY, EW, eyeH, ER, 0, 0);
    for (int line = 0; line < 2; line++) {
      display.drawLine(SCREEN_WIDTH / 2 - 8, MY + line, SCREEN_WIDTH / 2 + 8, MY + line, SH110X_WHITE);
    }
    drawZzz(98, 12 + eyeYShift, expressionPhase);
  } else if (currentExpression == "thinking") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int px = 2 + (int)(wave * 5.0f);
    int py = -2 + (int)(wave * 3.0f);
    int bubble = 2 + (int)(wave * 2.0f);
    drawEye(LX, EY, EW - 6, 12, ER, px, py);
    drawEye(RX, EY, EW, EH, ER, px, py);
    display.fillCircle(108, 38 + eyeYShift, bubble, SH110X_WHITE);
    display.fillCircle(114, 28 + eyeYShift, bubble + 1, SH110X_WHITE);
    display.fillCircle(118, 16 + eyeYShift, bubble + 2, SH110X_WHITE);
    for (int line = 0; line < 2; line++) {
      display.drawLine(SCREEN_WIDTH / 2 - 8, MY + line, SCREEN_WIDTH / 2 + 6, MY - 2 + line, SH110X_WHITE);
    }
  } else if (currentExpression == "happy") {
    int eyeH = EH;
    int py = (int)(sin(t * 3.14159f * 2.0f) * 2.0f);
    if (ph >= 24 && ph <= 27) {
      float blinkT = (float)(ph - 24) / 3.0f;
      if (ph <= 25) {
        eyeH = EH - (int)(blinkT * (EH - 3));
      } else {
        eyeH = 3 + (int)(blinkT * (EH - 3));
      }
    }
    drawEye(LX, EY, EW, eyeH, ER, 0, py);
    drawEye(RX, EY, EW, eyeH, ER, 0, py);
    drawSmile(SCREEN_WIDTH / 2, MY - 3, 24);
  } else if (currentExpression == "smile") {
    drawHappyArc(LX, EY, EW);
    drawHappyArc(RX, EY, EW);
    drawSmile(SCREEN_WIDTH / 2, MY - 2, 24);
  } else if (currentExpression == "confused") {
    float wave = sin(t * 3.14159f * 2.0f);
    int browTwitch = (int)(wave * 3.0f);
    drawEye(LX, EY - 2, EW, EH + 2, ER, -2, 0);
    drawEye(RX, EY + 3, EW - 6, EH - 6, ER - 2, 2, 0);
    drawBrow(LX - 14, EY - 15 - browTwitch, LX + 10, EY - 11);
    drawBrow(RX - 8, EY - 9, RX + 14, EY - 13 + browTwitch);
    for (int line = 0; line < 3; line++) {
      display.drawLine(SCREEN_WIDTH / 2 - 12, MY + 4 + line, SCREEN_WIDTH / 2 + 12, MY - 2 + line, SH110X_WHITE);
    }
  } else if (currentExpression == "look_around") {
    float sx = sin(t * 3.14159f * 2.0f);
    float sy = cos(t * 3.14159f * 4.0f) * 0.4f;
    int px = (int)(sx * 5.0f);
    int py = (int)(sy * 3.0f);
    drawEye(LX, EY, EW, EH, ER, px, py);
    drawEye(RX, EY, EW, EH, ER, px, py);
    for (int line = 0; line < 2; line++) {
      display.drawLine(SCREEN_WIDTH / 2 - 8, MY + line, SCREEN_WIDTH / 2 + 8, MY + line, SH110X_WHITE);
    }
  } else if (currentExpression == "kiss") {
    float r1 = fmod(t * 1.5f, 1.0f);
    float r2 = fmod(t * 1.5f + 0.45f, 1.0f);
    drawBlinkEye(LX, EY, EW, 4, ER);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    drawKissLips(SCREEN_WIDTH / 2, MY);
    if (r1 < 0.85f) {
      int hx = SCREEN_WIDTH / 2 - 10 + (int)(sin(r1 * 3.14159f) * 8.0f);
      int hy = MY - 10 - (int)(r1 * 38.0f);
      drawBigHeart(hx, hy, 3);
    }
    if (r2 < 0.85f) {
      int hx = SCREEN_WIDTH / 2 + 12 - (int)(sin(r2 * 3.14159f) * 6.0f);
      int hy = MY - 8 - (int)(r2 * 34.0f);
      drawBigHeart(hx, hy, 2);
    }
  } else if (currentExpression == "wink") {
    for (int line = 0; line < 3; line++) {
      display.drawLine(LX - EW / 2, EY + line, LX + EW / 2, EY + line, SH110X_WHITE);
    }
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    drawSmile(SCREEN_WIDTH / 2, MY - 2, 20);
  } else if (currentExpression == "laugh") {
    float wave = sin(t * 3.14159f * 4.0f) * 0.5f + 0.5f;
    int shakeX = (int)(wave * 3.0f) - 1;
    int mouthW = 20 + (int)(wave * 4.0f);
    drawHappyArc(LX + shakeX, EY, EW);
    drawHappyArc(RX + shakeX, EY, EW);
    display.fillRoundRect(SCREEN_WIDTH / 2 - mouthW / 2, MY - 5, mouthW, 12, 4, SH110X_WHITE);
    display.fillRoundRect(SCREEN_WIDTH / 2 - mouthW / 2 + 2, MY - 3, mouthW - 4, 8, 3, SH110X_BLACK);
  } else if (currentExpression == "star_eyes") {
    float twinkle = sin(t * 3.14159f * 4.0f) * 0.5f + 0.5f;
    int starR = 5 + (int)(twinkle * 2.0f);
    drawEye(LX, EY, EW, EH, ER, 0, 0);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    drawIconStar(LX, EY, starR);
    drawIconStar(RX, EY, starR);
    drawSmile(SCREEN_WIDTH / 2, MY - 2, 24);
  } else if (currentExpression == "excited") {
    float bounce = sin(t * 3.14159f * 4.0f) * 0.5f + 0.5f;
    int eyeShift = (int)(bounce * 4.0f);
    drawEye(LX, EY - eyeShift, EW + 4, EH + 4, ER, 0, 0);
    drawEye(RX, EY - eyeShift, EW + 4, EH + 4, ER, 0, 0);
    drawBrow(LX - 14, EY - 20 - eyeShift, LX + 14, EY - 20 - eyeShift);
    drawBrow(RX - 14, EY - 20 - eyeShift, RX + 14, EY - 20 - eyeShift);
    drawSmile(SCREEN_WIDTH / 2, MY - 2 - (int)(bounce * 2.0f), 28);
  } else if (currentExpression == "tongue") {
    for (int line = 0; line < 3; line++) {
      display.drawLine(LX - EW / 2, EY + line, LX + EW / 2, EY + line, SH110X_WHITE);
    }
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    for (int line = 0; line < 2; line++) {
      display.drawLine(SCREEN_WIDTH / 2 - 10, MY - 2 + line, SCREEN_WIDTH / 2, MY + 2 + line, SH110X_WHITE);
      display.drawLine(SCREEN_WIDTH / 2, MY + 2 + line, SCREEN_WIDTH / 2 + 10, MY - 2 + line, SH110X_WHITE);
    }
    float wobble = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int tongueH = 8 + (int)(wobble * 3.0f);
    display.fillRoundRect(SCREEN_WIDTH / 2 - 6, MY + 3, 12, tongueH, 4, SH110X_WHITE);
    display.fillCircle(SCREEN_WIDTH / 2, MY + 3 + tongueH - 3, 2, SH110X_BLACK);
  } else {
    float drift = sin(t * 3.14159f * 2.0f);
    int px = (int)(drift * 2.0f);
    int py = (int)(cos(t * 3.14159f * 3.0f) * 1.0f);
    int eyeH = EH;
    if (ph >= 28 && ph <= 30) {
      float blinkT = ph == 29 ? 1.0f : 0.5f;
      eyeH = EH - (int)(blinkT * (EH - 3));
    }
    drawEye(LX, EY, EW, eyeH, ER, px, py);
    drawEye(RX, EY, EW, eyeH, ER, px, py);
    for (int line = 0; line < 2; line++) {
      display.drawLine(SCREEN_WIDTH / 2 - 7, MY + line, SCREEN_WIDTH / 2 + 7, MY + line, SH110X_WHITE);
    }
  }

  drawCompanionAccessories(LX, RX, EY, MY);
  display.display();
}

void setIdleStatus(const String& value) {
  statusText = value;
  currentMode = MODE_IDLE;
  transientActive = false;
  activeCareAction = "";
  renderCurrentMode();
  publishStatus();
}

void setNote(const String& text, int fontSize, int border, const String& icons, const String& flowerAccent) {
  const String boundedText = text.length() > NOTE_TEXT_MAX ? text.substring(0, NOTE_TEXT_MAX) : text;
  const int boundedFontSize = fontSize < 1 ? 1 : (fontSize > 4 ? 4 : fontSize);
  currentNoteBorder = border < 0 ? 0 : (border > 4 ? 4 : border);
  currentNoteIcons = icons;
  // Persist so the note survives reboots
  preferences.begin("desk-cfg", false);
  preferences.putString("note_text", boundedText);
  preferences.putInt("note_fs", boundedFontSize);
  preferences.putInt("note_border", currentNoteBorder);
  preferences.putString("note_icons", currentNoteIcons);
  preferences.putString("note_flower", flowerAccent);
  preferences.end();
  bondLevel = clampLevel(bondLevel + 4);
  boredomLevel = clampLevel(boredomLevel - 10);
  persistPetState();

  // Push into circular queue, evicting oldest when full
  if (noteQueueCount < NOTE_QUEUE_MAX) {
    noteQueue[noteQueueCount++] = boundedText;
    noteFontSizeQueue[noteQueueCount - 1] = boundedFontSize;
  } else {
    for (int i = 0; i < NOTE_QUEUE_MAX - 1; i++) noteQueue[i] = noteQueue[i + 1];
    for (int i = 0; i < NOTE_QUEUE_MAX - 1; i++) noteFontSizeQueue[i] = noteFontSizeQueue[i + 1];
    noteQueue[NOTE_QUEUE_MAX - 1] = boundedText;
    noteFontSizeQueue[NOTE_QUEUE_MAX - 1] = boundedFontSize;
    if (noteQueueIndex > 0) noteQueueIndex--;
  }
  // Display the newest note
  noteQueueIndex = noteQueueCount - 1;
  currentNote = noteQueue[noteQueueIndex];
  currentNoteFontSize = noteFontSizeQueue[noteQueueIndex];
  currentNoteFlowerAccent = flowerAccent;
  currentMode = MODE_NOTE;
  statusText = "Showing note";
  renderCurrentMode();
  publishStatus();
  startTransientExpression(pickReactionExpression("note"), 2200, "Loved your note");
}

void setBanner(const String& text, int speed) {
  currentBanner = text;
  bannerSpeed = speed;
  bannerOffset = SCREEN_WIDTH;
  boredomLevel = clampLevel(boredomLevel - 6);
  energyLevel = clampLevel(energyLevel - 2);
  persistPetState();
  currentMode = MODE_BANNER;
  statusText = "Banner running";
  renderCurrentMode();
  publishStatus();
  startTransientExpression(pickReactionExpression("banner"), 1800, "Showing off");
}

void setExpression(const String& expression) {
  currentExpression = expression;
  expressionPhase = 0;
  lastExpressionTickMs = 0;
  currentMode = MODE_EXPRESSION;
  statusText = "Expression active";
  renderCurrentMode();
  publishStatus();
}

void setImageReady() {
  currentMode = MODE_IMAGE;
  statusText = "Image ready";
  renderCurrentMode();
  publishStatus();
}

void setFlower(const String& flowerType) {
  currentFlower = flowerType;
  expressionPhase = 0;
  lastExpressionTickMs = 0;
  currentMode = MODE_FLOWER;
  statusText = "Flower animation";
  renderCurrentMode();
  publishStatus();
}

void saveRelaySettings(const String& nextRelayUrl, const String& nextDeviceToken) {
  relayUrl = nextRelayUrl;
  deviceToken = nextDeviceToken;
  preferences.begin("desk-cfg", false);
  preferences.putString("relay_url", relayUrl);
  preferences.putString("device_token", deviceToken);
  preferences.end();
  statusText = relayUrl.isEmpty() ? "Relay cleared" : "Relay configured";
  publishStatus();
  relayStatusDirty = true;
}

bool connectToWifi(const String& ssid, const String& password) {
  WiFi.disconnect(true, false);
  delay(500);

  storedWifiPass = password;
  preferences.begin("desk-cfg", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  if (!relayUrl.isEmpty()) {
    preferences.putString("relay_url", relayUrl);
  }
  if (!deviceToken.isEmpty()) {
    preferences.putString("device_token", deviceToken);
  }
  preferences.end();

  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), password.c_str());
  markWifiJoinStarted();

  statusText = "Joining Wi-Fi";
  currentSsid = ssid;
  ipAddress = "";
  publishStatus();

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
    delay(500);
  }

  const int finalStatus = WiFi.status();
  markWifiJoinFinished();
  if (finalStatus == WL_CONNECTED) {
    ipAddress = WiFi.localIP().toString();
    wifiWasConnected = true;
    statusText = "Wi-Fi connected";
    publishStatus();
    pushRelayStatus();
    return true;
  }

  wifiWasConnected = false;
  statusText = String("Wi-Fi failed (") + String(finalStatus) + ")";
  publishStatus();
  return false;
}

bool wifiJoinInProgress() {
  return wifiJoinActive && millis() - lastWifiBeginMs < 30000;
}

void markWifiJoinStarted() {
  wifiJoinActive = true;
  lastWifiBeginMs = millis();
}

void markWifiJoinFinished() {
  wifiJoinActive = false;
}

void scanWifiNetworks() {
  statusText = "Scanning Wi-Fi";
  publishStatus();

  const bool shouldRestoreWifi =
      !currentSsid.isEmpty() && storedWifiPass.length() > 0;
  if (wifiJoinInProgress()) {
    markWifiJoinFinished();
  }
  if (bootWifiRestorePending) {
    bootWifiRestorePending = false;
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(false, false);
    delay(300);
  } else {
    WiFi.disconnect(false, false);
    delay(150);
  }

  WiFi.mode(WIFI_STA);
  delay(120);
  WiFi.scanDelete();
  availableWifiNetworkCount = 0;

  int foundNetworks = WiFi.scanNetworks(false, false, false, 300);
  if (foundNetworks < 0) {
    Serial.printf("[WIFI] Scan failed with code %d, retrying once.\n", foundNetworks);
    statusText = String("Retrying Wi-Fi scan (") + String(foundNetworks) + ")";
    publishStatus();
    WiFi.scanDelete();
    delay(250);
    WiFi.mode(WIFI_STA);
    delay(120);
    foundNetworks = WiFi.scanNetworks(false, false, false, 300);
  }

  for (int i = 0; i < foundNetworks && availableWifiNetworkCount < 10; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) {
      continue;
    }
    bool duplicate = false;
    for (int existing = 0; existing < availableWifiNetworkCount; existing++) {
      if (availableWifiNetworks[existing] == ssid) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      availableWifiNetworks[availableWifiNetworkCount++] = ssid;
    }
  }
  WiFi.scanDelete();

  if (shouldRestoreWifi) {
    WiFi.mode(WIFI_STA);
    delay(100);
    WiFi.setAutoReconnect(true);
    WiFi.begin(currentSsid.c_str(), storedWifiPass.c_str());
    markWifiJoinStarted();
    lastWifiCheckMs = millis();
  }

  if (foundNetworks < 0) {
    statusText = String("Wi-Fi scan failed (") + String(foundNetworks) + ")";
  } else {
    statusText =
        availableWifiNetworkCount > 0 ? "Wi-Fi list updated" : "No Wi-Fi found";
  }
  publishStatusWithNetworks();
}

// ─── Flower drawing helpers ───

// Draw a rose: layered petal circles spiraling from center
// cx,cy = center; scale = 1 for full-screen (28px outer radius), smaller for accents
void drawFlowerRose(int cx, int cy, int scale, int phase) {
  float spiralT = (float)phase / 31.f;
  float spiralOff = spiralT * 0.4f; // gentle rotation
  int outerR   = 8  * scale / 8;
  int outerDist = 16 * scale / 8;
  int midR     = 6  * scale / 8;
  int midDist  = 9  * scale / 8;
  int centerR  = 4  * scale / 8;
  if (outerR    < 2) outerR    = 2;
  if (outerDist < 4) outerDist = 4;
  if (midR      < 2) midR      = 2;
  if (midDist   < 3) midDist   = 3;
  if (centerR   < 1) centerR   = 1;

  // 5 outer petals
  for (int i = 0; i < 5; i++) {
    float a = i * 3.14159f * 2.f / 5.f + spiralOff;
    int px = cx + (int)(outerDist * cosf(a));
    int py = cy + (int)(outerDist * sinf(a));
    display.fillCircle(px, py, outerR, SH110X_WHITE);
  }
  // 5 inner petals (offset to interleave)
  for (int i = 0; i < 5; i++) {
    float a = i * 3.14159f * 2.f / 5.f + spiralOff + 0.314f;
    int px = cx + (int)(midDist * cosf(a));
    int py = cy + (int)(midDist * sinf(a));
    display.fillCircle(px, py, midR, SH110X_WHITE);
  }
  // Center dot
  display.fillCircle(cx, cy, centerR + 1, SH110X_WHITE);
  display.fillCircle(cx, cy, centerR - 1, SH110X_BLACK);
}

// Draw a sunflower: long petals + seed-spiral center
void drawFlowerSunflower(int cx, int cy, int scale, int phase) {
  float t = (float)phase / 31.f;
  int petalLen = 18 * scale / 8;
  int petalW   = 4  * scale / 8;
  int centerR  = 12 * scale / 8;
  if (petalLen < 5)  petalLen = 5;
  if (petalW   < 1)  petalW   = 1;
  if (centerR  < 3)  centerR  = 3;

  // 16 thin oval petals radiating out
  for (int i = 0; i < 16; i++) {
    float a = i * 3.14159f * 2.f / 16.f + t * 0.2f;
    int tip_x = cx + (int)((centerR + petalLen) * cosf(a));
    int tip_y = cy + (int)((centerR + petalLen) * sinf(a));
    int base_x = cx + (int)(centerR * cosf(a));
    int base_y = cy + (int)(centerR * sinf(a));
    // Fat line for each petal
    for (int w = -petalW/2; w <= petalW/2; w++) {
      float perp_a = a + 3.14159f / 2.f;
      int dx = (int)(w * cosf(perp_a));
      int dy = (int)(w * sinf(perp_a));
      display.drawLine(base_x + dx, base_y + dy, tip_x + dx, tip_y + dy, SH110X_WHITE);
    }
  }
  // Large center disc
  display.fillCircle(cx, cy, centerR, SH110X_WHITE);
  // Sunflower seed spiral: Fibonacci-ish dots
  int numSeeds = 21 + (int)(t * 11.f);
  for (int i = 0; i < numSeeds; i++) {
    float angle = i * 2.399f + t * 0.5f; // golden angle
    float r     = (centerR - 2) * sqrtf((float)i / 34.f);
    int sx = cx + (int)(r * cosf(angle));
    int sy = cy + (int)(r * sinf(angle));
    if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT) {
      display.fillCircle(sx, sy, 1, SH110X_BLACK);
    }
  }
}

// Draw a king protea: bold spiky bracts around dense center
void drawFlowerKingProtea(int cx, int cy, int scale, int phase) {
  float t = (float)phase / 31.f;
  float growT = t < 0.1f ? t * 10.f : 1.0f; // bracts extend in first phase
  int bractLen = (int)(20 * scale / 8 * growT);
  int bractW   = 3  * scale / 8;
  int centerR  = 14 * scale / 8;
  if (bractLen < 3)  bractLen = 3;
  if (bractW   < 1)  bractW   = 1;
  if (centerR  < 4)  centerR  = 4;

  // 20 elongated bracts (like spiky petals)
  for (int i = 0; i < 20; i++) {
    float a = i * 3.14159f * 2.f / 20.f + t * 0.1f;
    int base_x = cx + (int)(centerR * cosf(a));
    int base_y = cy + (int)(centerR * sinf(a));
    int tip_x  = cx + (int)((centerR + bractLen) * cosf(a));
    int tip_y  = cy + (int)((centerR + bractLen) * sinf(a));
    // Sharp tip — draw filled triangle-ish
    float side_a = a + 3.14159f / 2.f;
    int sx = (int)(bractW * cosf(side_a));
    int sy = (int)(bractW * sinf(side_a));
    display.fillTriangle(
      base_x + sx, base_y + sy,
      base_x - sx, base_y - sy,
      tip_x, tip_y,
      SH110X_WHITE
    );
  }
  // Dense textured center
  display.fillCircle(cx, cy, centerR, SH110X_WHITE);
  // Inner ring detail
  display.fillCircle(cx, cy, centerR - 3, SH110X_BLACK);
  display.fillCircle(cx, cy, centerR - 6, SH110X_WHITE);
  // Center pip
  display.fillCircle(cx, cy, 2, SH110X_BLACK);
}

// Dispatch based on currentFlower
void renderFlowerFrame() {
  if (!displayAvailable) {
    return;
  }
  display.clearDisplay();
  int cx = 64, cy = 27;
  // Stem
  display.drawLine(cx,     cy + 22, cx,     63, SH110X_WHITE);
  display.drawLine(cx + 1, cy + 22, cx + 1, 63, SH110X_WHITE);
  // Simple leaf
  for (int i = 0; i < 8; i++) {
    display.drawLine(cx + 1, cy + 40 + i, cx + 1 + 8 - i, cy + 40 + i, SH110X_WHITE);
  }

  if (currentFlower == "sunflower") {
    drawFlowerSunflower(cx, cy, 8, expressionPhase);
  } else if (currentFlower == "king_protea") {
    drawFlowerKingProtea(cx, cy, 8, expressionPhase);
  } else {
    drawFlowerRose(cx, cy, 8, expressionPhase);
  }
  display.display();
}

// ─── Note with flower accent ───
// Draws a note with a prominent flower on the left (pixels ~0-40),
// and the text in the right 86 pixels.
void drawNoteWithFlowerAccent(const String& text, int fontSize, int border, const String& icons, const String& flowerType) {
  if (!displayAvailable) {
    return;
  }
  display.clearDisplay();

  // Flower on the left side (cx=19, cy=32, scale=4)
  const int flowerCX = 20;
  const int flowerCY = 30;
  const int flowerScale = 4;
  if (flowerType == "sunflower") {
    drawFlowerSunflower(flowerCX, flowerCY, flowerScale, expressionPhase);
  } else if (flowerType == "king_protea") {
    drawFlowerKingProtea(flowerCX, flowerCY, flowerScale, expressionPhase);
  } else {
    drawFlowerRose(flowerCX, flowerCY, flowerScale, expressionPhase);
  }

  // Vertical divider
  display.drawLine(42, 2, 42, SCREEN_HEIGHT - 3, SH110X_WHITE);

  // Text on the right side
  const int safeFontSize = 1;
  const int textLeft = 46;
  const int textAreaW = SCREEN_WIDTH - textLeft - 4;
  const int maxChars = textAreaW / (6 * safeFontSize);
  const int lineHeight = (8 * safeFontSize) + 2;
  const int maxLines = (SCREEN_HEIGHT - 8) / lineHeight;

  String lines[maxLines];
  int lineCount = 0;
  String remaining = text;
  remaining.trim();
  while (remaining.length() > 0 && lineCount < maxLines) {
    int lineLength = (int)remaining.length() > maxChars ? maxChars : (int)remaining.length();
    if (lineLength < (int)remaining.length()) {
      int split = remaining.lastIndexOf(' ', lineLength);
      if (split > 0) lineLength = split;
    }
    String line = remaining.substring(0, lineLength);
    line.trim();
    if (line.isEmpty()) { line = remaining.substring(0, maxChars); lineLength = line.length(); }
    lines[lineCount++] = line;
    remaining = remaining.substring(lineLength);
    remaining.trim();
  }

  display.setTextSize(safeFontSize);
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  int totalH = lineCount * lineHeight;
  int startY = 4 + (SCREEN_HEIGHT - 8 - totalH) / 2;
  if (startY < 4) startY = 4;
  for (int i = 0; i < lineCount; i++) {
    display.setCursor(textLeft, startY + i * lineHeight);
    display.print(lines[i]);
  }
  display.display();
}

void tryStoredPrefs() {
  preferences.begin("desk-cfg", true);
  currentSsid = preferences.getString("ssid", "");
  storedWifiPass = preferences.getString("pass", "");
  relayUrl = preferences.getString("relay_url", "");
  deviceToken = preferences.getString("device_token", "");
  petPersonality = normalizePetPersonality(
    preferences.getString("pet_personality", petPersonality)
  );
  activePetMode = normalizePetMode(
    preferences.getString("pet_mode", activePetMode)
  );
  companionHair = normalizeCompanionHair(
    preferences.getString("companion_hair", companionHair)
  );
  companionEars = normalizeCompanionEars(
    preferences.getString("companion_ears", companionEars)
  );
  companionMustache = normalizeCompanionMustache(
    preferences.getString("companion_mustache", companionMustache)
  );
  companionGlasses = normalizeCompanionGlasses(
    preferences.getString("companion_glasses", companionGlasses)
  );
  companionHeadwear = normalizeCompanionHeadwear(
    preferences.getString("companion_headwear", companionHeadwear)
  );
  companionPiercing = normalizeCompanionPiercing(
    preferences.getString("companion_piercing", companionPiercing)
  );
  companionHairSize = clampAppearancePercent(
    preferences.getInt("companion_hair_size", companionHairSize)
  );
  companionMustacheSize = clampAppearancePercent(
    preferences.getInt("companion_mustache_size", companionMustacheSize)
  );
  companionHairWidth = clampAppearancePercent(
    preferences.getInt("companion_hair_width", companionHairWidth)
  );
  companionHairHeight = clampAppearancePercent(
    preferences.getInt("companion_hair_height", companionHairHeight)
  );
  companionHairThickness = clampAppearancePercent(
    preferences.getInt("companion_hair_thickness", companionHairThickness)
  );
  companionHairOffsetX = clampAppearanceOffset(
    preferences.getInt("companion_hair_offset_x", companionHairOffsetX)
  );
  companionHairOffsetY = clampAppearanceOffset(
    preferences.getInt("companion_hair_offset_y", companionHairOffsetY)
  );
  companionEyeOffsetY = clampAppearanceOffset(
    preferences.getInt("companion_eye_offset_y", companionEyeOffsetY)
  );
  companionMouthOffsetY = clampAppearanceOffset(
    preferences.getInt("companion_mouth_offset_y", companionMouthOffsetY)
  );
  companionMustacheWidth = clampAppearancePercent(
    preferences.getInt("companion_mustache_width", companionMustacheWidth)
  );
  companionMustacheHeight = clampAppearancePercent(
    preferences.getInt("companion_mustache_height", companionMustacheHeight)
  );
  companionMustacheThickness = clampAppearancePercent(
    preferences.getInt("companion_mustache_thickness", companionMustacheThickness)
  );
  companionMustacheOffsetX = clampAppearanceOffset(
    preferences.getInt("companion_mustache_offset_x", companionMustacheOffsetX)
  );
  companionMustacheOffsetY = clampAppearanceOffset(
    preferences.getInt("companion_mustache_offset_y", companionMustacheOffsetY)
  );
  bondLevel = clampLevel(preferences.getInt("bond_level", bondLevel));
  energyLevel = clampLevel(preferences.getInt("energy_level", energyLevel));
  boredomLevel = clampLevel(preferences.getInt("boredom_level", boredomLevel));
  // Restore last note
  const String savedNote = preferences.getString("note_text", "");
  if (!savedNote.isEmpty()) {
    currentNote = savedNote;
    currentNoteFontSize = preferences.getInt("note_fs", 1);
    currentNoteBorder = preferences.getInt("note_border", 0);
    currentNoteIcons = preferences.getString("note_icons", "");
    currentNoteFlowerAccent = preferences.getString("note_flower", "");
    noteQueue[0] = currentNote;
    noteFontSizeQueue[0] = currentNoteFontSize;
    noteQueueCount = 1;
    noteQueueIndex = 0;
    currentMode = MODE_NOTE;
  } else {
    statusText = petAmbientStatus();
  }
  preferences.end();
}

void handleCommandJson(const String& body) {
  const String type = extractJsonStringField(body, "type");
  if (type.isEmpty()) {
    statusText = "Bad command JSON";
    publishStatus();
    return;
  }

  if (type == "connect_wifi") {
    pendingWifiSsid = extractJsonStringField(body, "ssid");
    pendingWifiPass = extractJsonStringField(body, "password");
    wifiConnectPending = true;
    statusText = "Wi-Fi queued";
    publishStatus();
    return;
  }

  if (type == "scan_wifi") {
    wifiScanPending = true;
    statusText = "Scan queued";
    publishStatus();
    return;
  }

  if (type == "forget_wifi") {
    WiFi.disconnect(true, true);
    WiFi.setAutoReconnect(false);
    currentSsid = "";
    ipAddress = "";
    wifiWasConnected = false;
    availableWifiNetworkCount = 0;
    preferences.begin("desk-cfg", false);
    preferences.remove("ssid");
    preferences.remove("pass");
    preferences.end();
    statusText = "Wi-Fi forgotten";
    publishStatus();
    return;
  }

  if (type == "set_note") {
    setNote(
      extractJsonStringField(body, "text"),
      extractJsonIntField(body, "fontSize", 1),
      extractJsonIntField(body, "border", 0),
      extractJsonStringField(body, "icons", ""),
      extractJsonStringField(body, "flowerAccent", "")
    );
    return;
  }

  if (type == "set_banner") {
    setBanner(
      extractJsonStringField(body, "text"),
      extractJsonIntField(body, "speed", 35)
    );
    return;
  }

  if (type == "set_expression") {
    setExpression(
      extractJsonStringField(body, "expression", "happy")
    );
    return;
  }

  if (type == "set_personality") {
    setPetPersonality(extractJsonStringField(body, "personality", "curious"));
    return;
  }

  if (type == "trigger_pet_mode") {
    triggerPetMode(extractJsonStringField(body, "petMode", "hangout"));
    return;
  }

  if (type == "set_companion_style") {
    companionHair = normalizeCompanionHair(
      extractJsonStringField(body, "hair", companionHair)
    );
    companionEars = normalizeCompanionEars(
      extractJsonStringField(body, "ears", companionEars)
    );
    companionMustache = normalizeCompanionMustache(
      extractJsonStringField(body, "mustache", companionMustache)
    );
    companionGlasses = normalizeCompanionGlasses(
      extractJsonStringField(body, "glasses", companionGlasses)
    );
    companionHeadwear = normalizeCompanionHeadwear(
      extractJsonStringField(body, "headwear", companionHeadwear)
    );
    companionPiercing = normalizeCompanionPiercing(
      extractJsonStringField(body, "piercing", companionPiercing)
    );
    companionHairSize = clampAppearancePercent(
      extractJsonIntField(body, "hairSize", companionHairSize)
    );
    companionMustacheSize = clampAppearancePercent(
      extractJsonIntField(body, "mustacheSize", companionMustacheSize)
    );
    companionHairWidth = clampAppearancePercent(
      extractJsonIntField(body, "hairWidth", companionHairWidth)
    );
    companionHairHeight = clampAppearancePercent(
      extractJsonIntField(body, "hairHeight", companionHairHeight)
    );
    companionHairThickness = clampAppearancePercent(
      extractJsonIntField(body, "hairThickness", companionHairThickness)
    );
    companionHairOffsetX = clampAppearanceOffset(
      extractJsonIntField(body, "hairOffsetX", companionHairOffsetX)
    );
    companionHairOffsetY = clampAppearanceOffset(
      extractJsonIntField(body, "hairOffsetY", companionHairOffsetY)
    );
    companionEyeOffsetY = clampAppearanceOffset(
      extractJsonIntField(body, "eyeOffsetY", companionEyeOffsetY)
    );
    companionMouthOffsetY = clampAppearanceOffset(
      extractJsonIntField(body, "mouthOffsetY", companionMouthOffsetY)
    );
    companionMustacheWidth = clampAppearancePercent(
      extractJsonIntField(body, "mustacheWidth", companionMustacheWidth)
    );
    companionMustacheHeight = clampAppearancePercent(
      extractJsonIntField(body, "mustacheHeight", companionMustacheHeight)
    );
    companionMustacheThickness = clampAppearancePercent(
      extractJsonIntField(body, "mustacheThickness", companionMustacheThickness)
    );
    companionMustacheOffsetX = clampAppearanceOffset(
      extractJsonIntField(body, "mustacheOffsetX", companionMustacheOffsetX)
    );
    companionMustacheOffsetY = clampAppearanceOffset(
      extractJsonIntField(body, "mustacheOffsetY", companionMustacheOffsetY)
    );
    persistPetState();
    if (currentMode == MODE_IDLE) {
      renderCurrentMode();
    }
    publishStatus();
    return;
  }

  if (type == "care_action") {
    sendCareAction(extractJsonStringField(body, "action", "pet"));
    return;
  }

  if (type == "set_relay") {
    saveRelaySettings(
      extractJsonStringField(body, "relayUrl"),
      extractJsonStringField(body, "deviceToken")
    );
    return;
  }

  if (type == "set_flower") {
    setFlower(extractJsonStringField(body, "flower", "rose"));
    return;
  }

  if (type == "clear") {
    clearImageBuffer();
    setIdleStatus("Display cleared");
    return;
  }

  if (type == "status") {
    publishStatus();
    return;
  }

  if (type == "begin_image") {
    clearImageBuffer();
    expectedImageBytes = extractJsonIntField(body, "total", 0);
    receivedImageBytes = 0;
    imageTransferActive = expectedImageBytes == sizeof(imageBuffer);
    statusText = imageTransferActive ? "Receiving image" : "Bad image size";
    publishStatus();
    return;
  }

  if (type == "commit_image") {
    if (imageTransferActive && receivedImageBytes == expectedImageBytes) {
      setImageReady();
    } else {
      statusText = "Image incomplete";
      publishStatus();
    }
    imageTransferActive = false;
    return;
  }

  if (type == "set_image") {
    const String encoded = extractJsonStringField(body, "data");
    if (decodeBase64IntoImage(encoded)) {
      setImageReady();
    } else {
      statusText = "Bad image payload";
      publishStatus();
    }
  }
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    (void)server;
    publishStatus();
  }

  void onDisconnect(BLEServer* server) override {
    (void)server;
    BLEDevice::startAdvertising();
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    const auto value = characteristic->getValue();
    const String body = bleValueToString(value);
    if (!body.isEmpty()) {
      handleCommandJson(body);
    }
  }
};

class ImageCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    if (!imageTransferActive) {
      return;
    }

    const auto value = characteristic->getValue();
    const size_t remaining = sizeof(imageBuffer) - receivedImageBytes;
    const size_t chunkSize = bleValueLength(value) < remaining ? bleValueLength(value) : remaining;
    memcpy(imageBuffer + receivedImageBytes, bleValueData(value), chunkSize);
    receivedImageBytes += chunkSize;
  }
};

void pushRelayStatus() {
  if (WiFi.status() != WL_CONNECTED || relayUrl.isEmpty() || deviceToken.isEmpty()) {
    return;
  }

  const String url = relayUrl + "/v1/device/" + deviceToken + "/status";
  HTTPClient client;
  if (!beginHttpClient(client, url, 5000)) {
    return;
  }

  client.addHeader("Content-Type", "application/json");
  client.addHeader("Connection", "close");

  const String body = buildStatusJson();
  const int code = client.POST(body);
  if (code > 0) {
    client.getString();
    lastRelaySuccessMs = millis();
  } else {
    relaySecureClient.stop();
    relayHttpClient.stop();
  }

  client.end();
  relayStatusDirty = false;
  lastRelayStatusPushMs = millis();
}

void pollRelay() {
  if (WiFi.status() != WL_CONNECTED || relayUrl.isEmpty() || deviceToken.isEmpty()) {
    return;
  }

  const unsigned long pollInterval = currentMode == MODE_BANNER ? 8000UL : 4000UL;
  if (millis() - lastRelayPollMs < pollInterval) {
    return;
  }

  lastRelayPollMs = millis();

  const String url = relayUrl + "/v1/device/" + deviceToken + "/pull";
  HTTPClient client;
  if (!beginHttpClient(client, url, 5000)) {
    return;
  }

  client.addHeader("Connection", "close");

  const int code = client.GET();
  if (code == 200) {
    const String command = client.getString();
    lastRelaySuccessMs = millis();
    handleCommandJson(command);
  } else if (code > 0) {
    client.getString();
    lastRelaySuccessMs = millis();
  } else {
    relaySecureClient.stop();
    relayHttpClient.stop();
  }

  client.end();
}

void setupBle() {
  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(517);  // Request max MTU for larger payloads
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService* service = bleServer->createService(SERVICE_UUID);

  commandCharacteristic = service->createCharacteristic(
    COMMAND_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  commandCharacteristic->setCallbacks(new CommandCallbacks());

  statusCharacteristic = service->createCharacteristic(
    STATUS_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  statusCharacteristic->addDescriptor(new BLE2902());

  imageCharacteristic = service->createCharacteristic(
    IMAGE_UUID,
    BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_WRITE
  );
  imageCharacteristic->setCallbacks(new ImageCallbacks());

  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

bool probeDisplayOnPins(int sdaPin, int sclPin, uint8_t& foundAddr) {
  foundAddr = 0;

  Wire.end();
  delay(20);

  if (sdaPin >= 0 && sclPin >= 0) {
    Wire.begin(sdaPin, sclPin);
    Serial.print("[OLED] Probing pins SDA=");
    Serial.print(sdaPin);
    Serial.print(", SCL=");
    Serial.println(sclPin);
  } else {
    Wire.begin();
    Serial.println("[OLED] Probing board default I2C pins");
  }

  delay(60);

  for (uint8_t addr : {0x3C, 0x3D}) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      foundAddr = addr;
      return true;
    }
  }

  return false;
}

bool isButtonPinUsable(int pin) {
  if (pin < 0) {
    return false;
  }
  if (pin == activeOledSdaPin || pin == activeOledSclPin) {
    return false;
  }
  return true;
}

void setupDisplay() {
  struct I2cProbeCandidate {
    int sda;
    int scl;
  };

  const I2cProbeCandidate candidates[] = {
    {OLED_SDA_PIN, OLED_SCL_PIN},
    {-1, -1},
    {8, 9},
    {5, 6},
    {6, 7},
    {4, 5},
  };

  displayAvailable = false;
  activeOledSdaPin = OLED_SDA_PIN;
  activeOledSclPin = OLED_SCL_PIN;
  activeOledAddress = OLED_ADDRESS;

  int lastTriedSda = -999;
  int lastTriedScl = -999;

  for (const I2cProbeCandidate& candidate : candidates) {
    if (candidate.sda == lastTriedSda && candidate.scl == lastTriedScl) {
      continue;
    }
    lastTriedSda = candidate.sda;
    lastTriedScl = candidate.scl;

    uint8_t foundAddr = 0;
    if (!probeDisplayOnPins(candidate.sda, candidate.scl, foundAddr)) {
      continue;
    }

    Serial.print("[OLED] Found I2C device at 0x");
    Serial.print(foundAddr, HEX);
    if (candidate.sda >= 0 && candidate.scl >= 0) {
      Serial.print(" using SDA=");
      Serial.print(candidate.sda);
      Serial.print(", SCL=");
      Serial.println(candidate.scl);
    } else {
      Serial.println(" using board default I2C pins");
    }

    if (display.begin(foundAddr, true)) {
      displayAvailable = true;
      activeOledSdaPin = candidate.sda;
      activeOledSclPin = candidate.scl;
      activeOledAddress = foundAddr;
      break;
    }

    Serial.print("[OLED] display.begin() failed at 0x");
    Serial.println(foundAddr, HEX);
  }

  if (!displayAvailable) {
    statusText = "OLED offline";
    Serial.println("[OLED] No supported OLED detected. Continuing headless so BLE and Wi-Fi can still start.");
    return;
  }

  Serial.print("[OLED] display.begin() OK at 0x");
  Serial.println(activeOledAddress, HEX);

  display.clearDisplay();
  display.display();
}

void setupButtons() {
  buttonsAvailable = isButtonPinUsable(BTN_NEXT_PIN) && isButtonPinUsable(BTN_CLEAR_PIN);
  if (!buttonsAvailable) {
    Serial.println("[BTN] Buttons disabled because the configured pins are unavailable or conflict with OLED wiring.");
    return;
  }

  pinMode(BTN_NEXT_PIN, INPUT_PULLUP);
  pinMode(BTN_CLEAR_PIN, INPUT_PULLUP);
}

void handleButtons() {
  if (!buttonsAvailable) {
    return;
  }

  bool nextNow  = digitalRead(BTN_NEXT_PIN);
  bool clearNow = digitalRead(BTN_CLEAR_PIN);
  unsigned long now = millis();

  // --- BTN_NEXT: short press advances through note queue ---
  if (nextNow == LOW && btnNextLast == HIGH) {
    // falling edge (press)
    btnNextDownMs = now;
  }
  if (nextNow == HIGH && btnNextLast == LOW) {
    // rising edge (release) — treat as short press
    if (noteQueueCount > 1 && (now - btnNextDownMs) < 2000) {
      noteQueueIndex = (noteQueueIndex + 1) % noteQueueCount;
      currentNote = noteQueue[noteQueueIndex];
      currentNoteFontSize = noteFontSizeQueue[noteQueueIndex];
      currentMode = MODE_NOTE;
      statusText = "Showing note";
      renderCurrentMode();
      publishStatus();
      bondLevel = clampLevel(bondLevel + 2);
      boredomLevel = clampLevel(boredomLevel - 4);
      persistPetState();
      startTransientExpression(pickReactionExpression("button_next"), 1200, "Thanks for the tap");
    }
  }
  btnNextLast = nextNow;

  // --- BTN_CLEAR: 5-second hold clears display ---
  if (clearNow == LOW && btnClearLast == HIGH) {
    // falling edge
    btnClearDownMs = now;
    btnClearHoldFired = false;
  }
  if (clearNow == LOW && !btnClearHoldFired) {
    if ((now - btnClearDownMs) >= BTN_HOLD_MS) {
      btnClearHoldFired = true;
      noteQueueCount = 0;
      noteQueueIndex = 0;
      currentMode = MODE_IDLE;
      currentNote = "";
      // Clear persisted note
      preferences.begin("desk-cfg", false);
      preferences.remove("note_text");
      preferences.end();
      setIdleStatus("Ready");
      renderCurrentMode();
      publishStatus();
      energyLevel = clampLevel(energyLevel + 4);
      boredomLevel = clampLevel(boredomLevel + 6);
      persistPetState();
      startTransientExpression(pickReactionExpression("button_clear"), 1600, "Miss my notes");
    }
  }
  if (clearNow == HIGH) {
    btnClearHoldFired = false;
  }
  btnClearLast = clearNow;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Desk Companion S3 boot ===");

  WiFi.persistent(false);

  setupDisplay();
  setupButtons();
  clearImageBuffer();

  tryStoredPrefs();
  setupBle();

  if (!currentSsid.isEmpty() && storedWifiPass.length() > 0) {
    bootWifiRestorePending = true;
    statusText = "Wi-Fi queued";
    Serial.println("[BOOT] Deferring Wi-Fi reconnect until boot settles.");
  }

  bootCompletedAtMs = millis();
  lastWifiCheckMs = millis();

  renderCurrentMode();
  publishStatus();
}

void loop() {
  if (millis() - lastDecorTickMs >= 500) {
    lastDecorTickMs = millis();
    idleOrbit = (idleOrbit + 1) % 4;
    if (currentMode == MODE_IDLE) {
      renderIdle();
    }
  }

  if (currentMode == MODE_BANNER) {
    const unsigned long now = millis();
    const unsigned long interval = bannerSpeed <= 0 ? 40 : 1000UL / bannerSpeed;
    if (now - lastBannerTickMs >= interval) {
      lastBannerTickMs = now;
      bannerOffset--;
      const int textWidth = currentBanner.length() * 12;
      if (bannerOffset < -textWidth) {
        bannerOffset = SCREEN_WIDTH;
      }
      renderBannerFrame();
    }
  }

  if (currentMode == MODE_EXPRESSION) {
    const unsigned long now = millis();
    if (now - lastExpressionTickMs >= 50) {
      lastExpressionTickMs = now;
      expressionPhase = (expressionPhase + 1) % 32;
      renderExpressionFrame();
    }
  }

  if (currentMode == MODE_FLOWER) {
    const unsigned long now = millis();
    if (now - lastExpressionTickMs >= 60) {
      lastExpressionTickMs = now;
      expressionPhase = (expressionPhase + 1) % 32;
      renderFlowerFrame();
    }
  }

  updatePetBehavior();

  if (bootWifiRestorePending &&
      !currentSsid.isEmpty() &&
      storedWifiPass.length() > 0 &&
      !wifiJoinInProgress() &&
      WiFi.status() != WL_CONNECTED &&
      millis() - bootCompletedAtMs >= BOOT_WIFI_DELAY_MS) {
    bootWifiRestorePending = false;
    statusText = "Starting Wi-Fi";
    publishStatus();
    Serial.println("[BOOT] Starting deferred Wi-Fi reconnect.");
    WiFi.mode(WIFI_STA);
    delay(100);
    WiFi.setAutoReconnect(true);
    WiFi.begin(currentSsid.c_str(), storedWifiPass.c_str());
    markWifiJoinStarted();
    lastWifiCheckMs = millis();
  }

  const bool wifiNow = WiFi.status() == WL_CONNECTED;
  if (wifiNow) {
    ipAddress = WiFi.localIP().toString();
    markWifiJoinFinished();
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      statusText = "Wi-Fi connected";
      publishStatus();
    }
    lastWifiCheckMs = millis();
  } else {
    ipAddress = "";
    if (wifiJoinActive && millis() - lastWifiBeginMs >= 30000) {
      markWifiJoinFinished();
      statusText = "Wi-Fi connect failed";
      publishStatus();
    }
    if (wifiWasConnected) {
      wifiWasConnected = false;
      statusText = "Wi-Fi reconnecting";
      lastWifiCheckMs = millis();
      publishStatus();
    }

    if (!currentSsid.isEmpty() &&
        storedWifiPass.length() > 0 &&
        !wifiJoinInProgress() &&
        millis() - lastWifiCheckMs >= 60000) {
      statusText = "Retrying Wi-Fi";
      publishStatus();
      WiFi.mode(WIFI_STA);
      delay(100);
      WiFi.setAutoReconnect(true);
      WiFi.begin(currentSsid.c_str(), storedWifiPass.c_str());
      markWifiJoinStarted();
      lastWifiCheckMs = millis();
    }
  }

  if (wifiScanPending) {
    wifiScanPending = false;
    scanWifiNetworks();
  }

  if (wifiConnectPending) {
    wifiConnectPending = false;
    connectToWifi(pendingWifiSsid, pendingWifiPass);
    pendingWifiSsid = "";
    pendingWifiPass = "";
  }

  if (relayStatusDirty || (millis() - lastRelayStatusPushMs >= 30000)) {
    pushRelayStatus();
  }

  if (WiFi.status() == WL_CONNECTED &&
      !relayUrl.isEmpty() &&
      !deviceToken.isEmpty() &&
      lastRelaySuccessMs > 0 &&
      millis() - lastRelaySuccessMs >= 45000) {
    relaySecureClient.stop();
    relayHttpClient.stop();
    relayStatusDirty = true;
    if (millis() - lastRelaySuccessMs >= 90000 &&
        !currentSsid.isEmpty() &&
        storedWifiPass.length() > 0 &&
        !wifiJoinInProgress()) {
      statusText = "Relay stalled, reconnecting Wi-Fi";
      publishStatus();
      WiFi.disconnect(false, false);
      delay(100);
      WiFi.begin(currentSsid.c_str(), storedWifiPass.c_str());
      markWifiJoinStarted();
      lastWifiCheckMs = millis();
      lastRelaySuccessMs = millis();
    }
  }

  pollRelay();

  handleButtons();

  delay(1);
}
