// ─── Desk Companion TFT 2.8" (ST7789 240×320, Freenove FNK0104) ───
// Adapted from mini.ino for the ST7789 240×320 IPS TFT with FT6336U capacitive touch.
// Target board: Freenove ESP32-S3 Display (FNK0104) — all-in-one CYD.
// All logic (BLE, Wi-Fi, relay, pet, commands) is identical to the Mini OLED.
// Display calls adapted: SPI TFT, colors, scaled coordinates.

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Wire.h>
#include <BLE2902.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <SPI.h>
#include <esp_system.h>
#include <ctype.h>
#include <mbedtls/base64.h>
#include <string>
#include <time.h>

// ─── TFT SPI pin configuration (Freenove FNK0104 Touch variant) ───
#define TFT_CS    10
#define TFT_DC    46
#define TFT_MOSI  11
#define TFT_SCK   12
#define TFT_MISO  13
#define TFT_BL    45   // backlight, active HIGH
#define TFT_RST   -1   // no dedicated reset

// ─── FT6336U capacitive touch I2C pins ───
#define TOUCH_SDA  16
#define TOUCH_SCL  15
#define TOUCH_RST  18
#define TOUCH_INT  17

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// Color palette
#define COL_BG       ST77XX_BLACK
#define COL_FG       ST77XX_WHITE
#define COL_ACCENT   ST77XX_CYAN
// Extended RGB565 color palette
#define COL_ROSE     0xFB56  // Hot pink / rose  (255, 106, 176)
#define COL_GOLD     0xFFE0  // Yellow-gold      (255, 224,   0)
#define COL_PINK     0xFDB8  // Soft light pink  (255, 182, 193)
#define COL_MINT     0x07E0  // Fresh mint green (  0, 255,   0)
#define COL_LAVENDER 0x92BD  // Soft purple      (148,  87, 235)
#define COL_PEACH    0xFED6  // Warm peach       (255, 218, 181)
#define COL_SKYBLUE  0x867D  // Sky blue         (135, 206, 235)

#ifndef BTN_NEXT_PIN
#define BTN_NEXT_PIN -1  // touch replaces physical buttons
#endif

#ifndef BTN_CLEAR_PIN
#define BTN_CLEAR_PIN -1
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
#define DESK_COMPANION_DEVICE_NAME "Desk Companion TFT"
#endif

static const char* DEVICE_NAME = DESK_COMPANION_DEVICE_NAME;
static const char* SERVICE_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f001";
static const char* COMMAND_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f002";
static const char* STATUS_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f003";
static const char* IMAGE_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f004";

Adafruit_ST7789* pTft = nullptr;
Preferences preferences;
bool displayAvailable = false;
bool touchAvailable = false;

// ─── Inline FT6336U I2C touch driver (no external library needed) ───
#define FT6336U_ADDR 0x38
struct TouchPoint { uint16_t x; uint16_t y; };

bool ft6336u_init() {
  Wire.beginTransmission(FT6336U_ADDR);
  return (Wire.endTransmission() == 0);
}

uint8_t ft6336u_read_reg(uint8_t reg) {
  Wire.beginTransmission(FT6336U_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)FT6336U_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

uint8_t ft6336u_touched() {
  return ft6336u_read_reg(0x02) & 0x0F;  // TD_STATUS register, lower nibble = touch count
}

TouchPoint ft6336u_getPoint() {
  TouchPoint pt;
  Wire.beginTransmission(FT6336U_ADDR);
  Wire.write(0x03);  // start of first touch data
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)FT6336U_ADDR, (uint8_t)4);
  uint8_t xH = Wire.read();
  uint8_t xL = Wire.read();
  uint8_t yH = Wire.read();
  uint8_t yL = Wire.read();
  pt.x = ((xH & 0x0F) << 8) | xL;
  pt.y = ((yH & 0x0F) << 8) | yL;
  return pt;
}

// Convenience reference for display (set once in setupDisplay)
#define tft (*pTft)

BLEServer* bleServer = nullptr;
BLECharacteristic* commandCharacteristic = nullptr;
BLECharacteristic* statusCharacteristic = nullptr;
BLECharacteristic* imageCharacteristic = nullptr;

enum DisplayMode {
  MODE_IDLE,
  MODE_NOTE,
  MODE_BANNER,
  MODE_IMAGE,
  MODE_EXPRESSION,
  MODE_FLOWER,
  MODE_SCENE,
  MODE_FIREWORKS,
  MODE_HEART_RAIN,
  MODE_SNOWFALL,
  MODE_STARFIELD,
  MODE_COUNTDOWN,
  MODE_WEATHER,
  MODE_SLEEP,
};

DisplayMode currentMode = MODE_IDLE;
String statusText = "Booting";
String currentFlower = "rose";
String currentScene  = "wave";
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
// User-configurable display colors (RGB565, saved to flash)
uint16_t userEyeColor    = COL_FG;       // eye fill
uint16_t userFaceColor   = COL_FG;       // face border / mouth / outline
uint16_t userAccentColor = COL_ACCENT;   // clock text, highlight elements
uint16_t userBodyColor   = COL_ROSE;     // stick-figure second body, cheek blush
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
int currentNoteBorder = 0;
String currentNoteIcons = "";
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

// Image buffer for 240x320 monochrome bitmap (9600 bytes)
uint8_t imageBuffer[SCREEN_WIDTH * SCREEN_HEIGHT / 8] = {0};
size_t expectedImageBytes = 0;
size_t receivedImageBytes = 0;
bool imageTransferActive = false;

// Touch state for virtual buttons
bool touchActive = false;
unsigned long touchStartMs = 0;
int touchStartX = 0;
int touchStartY = 0;

// Weather
float weatherLat = 0.f;
float weatherLon = 0.f;
int   weatherCode       = -1;   // WMO code; -1 = no data
int   weatherTempTenths = 0;    // temp_2m * 10, e.g. 215 = 21.5 C
unsigned long lastWeatherFetchMs = 0;
// Touch double-tap
unsigned long lastTapReleaseMs = 0;
// Daily greetings (Phase 9)
int  lastGreetingDay   = -1;
int   lastEveningDay    = -1;
unsigned long lastTimeCheckMs       = 0;
unsigned long lastIdleInteractionMs = 0;
// Emoji note reaction state
String lastNoteEmoji = "";
unsigned long emojiReactionEndsMs = 0;

// Particle system
struct Ptcl { int16_t x, y; int8_t vx, vy; uint8_t life; uint16_t color; };
Ptcl gPtcl[48];
uint8_t  gPtclCount        = 0;
unsigned long lastParticleTickMs = 0;
// Countdown
long     countdownSeconds  = 0;
unsigned long countdownStartMs  = 0;
bool     countdownExpired  = false;
// NTP
int      ntpUtcOffsetSeconds = 0;

// Note queue
String noteQueue[NOTE_QUEUE_MAX];
int noteFontSizeQueue[NOTE_QUEUE_MAX] = {0};
int noteQueueCount = 0;
int noteQueueIndex = 0;

// Forward declarations
const char* modeName(DisplayMode mode);
int relayRequest(const char* method, const String& url, const String& body, String& response);
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
void handleTouch();
void drawStickFigure(int cx, int cy, int sc, float armLA, float armRA, float legLA, float legRA, uint16_t color);
void renderSceneFrame();
void setScene(const String& name);
void initFireworks();
void initHeartRain();
void initSnowfall();
void initStarfield();
void renderFireworksFrame();
void renderHeartRainFrame();
void renderSnowfallFrame();
void renderStarfieldFrame();
void setParticleMode(const String& name);
void renderCountdownFrame();
void setCountdown(long seconds);
void syncNtp();
float extractJsonFloatField(const String& body, const char* key, float fallback = 0.f);
void fetchWeather();
void drawWeatherBadge(int x, int y);
void renderWeatherFrame();
void checkTimeGreetings();
void renderSleepFrame();
void setSleepMode();
void drawStatusBar();
void detectEmojiReaction(const String& text);

// ─── BLE value helpers ───

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

// ─── JSON helpers ───

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

float extractJsonFloatField(const String& body, const char* key, float fallback) {
  int cursor = findJsonValueStart(body, key);
  if (cursor < 0 || cursor >= (int)body.length()) return fallback;
  int start = cursor;
  if (body[cursor] == '-') cursor++;
  bool hasDigit = false;
  while (cursor < (int)body.length() &&
         (isdigit((unsigned char)body[cursor]) || body[cursor] == '.')) {
    hasDigit = true;
    cursor++;
  }
  return hasDigit ? body.substring(start, cursor).toFloat() : fallback;
}

// ─── Status JSON builders ───

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
    case MODE_SCENE:
      return "scene";
    case MODE_FIREWORKS:
      return "fireworks";
    case MODE_HEART_RAIN:
      return "heart_rain";
    case MODE_SNOWFALL:
      return "snowfall";
    case MODE_STARFIELD:
      return "starfield";
    case MODE_COUNTDOWN:
      return "countdown";
    case MODE_WEATHER:
      return "weather";
    case MODE_SLEEP:
      return "sleep";
    case MODE_IDLE:
    default:
      return "idle";
  }
}

// ─── Relay (identical to mini) ───

static const char* RELAY_HTTP_HOST = "desk-companion-relay.fly.dev";
static const uint16_t RELAY_HTTP_PORT = 80;

int relayRequest(const char* method, const String& url, const String& body, String& response) {
  String finalUrl = url;
  int protoEnd = finalUrl.indexOf("://");
  String hostAndPath = (protoEnd > 0) ? finalUrl.substring(protoEnd + 3) : finalUrl;
  int pathStart = hostAndPath.indexOf('/');
  String path = (pathStart > 0) ? hostAndPath.substring(pathStart) : "/";

  Serial.printf("[RELAY-HTTP] %s %s (host=%s:%u path=%s) heap=%u\n",
    method, url.c_str(), RELAY_HTTP_HOST, RELAY_HTTP_PORT,
    path.c_str(), ESP.getFreeHeap());

  WiFiClient tcp;
  tcp.setTimeout(15);
  tcp.setConnectionTimeout(10000);

  unsigned long t0 = millis();
  if (!tcp.connect(RELAY_HTTP_HOST, RELAY_HTTP_PORT)) {
    unsigned long elapsed = millis() - t0;
    Serial.printf("[RELAY-HTTP] TCP connect FAILED in %lums heap=%u\n",
      elapsed, ESP.getFreeHeap());
    response = String("TCP fail ") + String(elapsed) + "ms";
    return -1;
  }
  unsigned long elapsed = millis() - t0;
  Serial.printf("[RELAY-HTTP] TCP connected in %lums\n", elapsed);

  String req = String(method) + " " + path + " HTTP/1.1\r\n";
  req += String("Host: ") + RELAY_HTTP_HOST + "\r\n";
  req += "Connection: close\r\n";
  if (body.length() > 0) {
    req += "Content-Type: application/json\r\n";
    req += "Content-Length: " + String(body.length()) + "\r\n";
  }
  req += "\r\n";
  if (body.length() > 0) {
    req += body;
  }

  tcp.print(req);

  unsigned long respStart = millis();
  String statusLine = "";
  while (millis() - respStart < 10000) {
    if (tcp.available()) {
      statusLine = tcp.readStringUntil('\n');
      statusLine.trim();
      break;
    }
    if (!tcp.connected()) break;
    delay(10);
  }

  if (statusLine.length() == 0) {
    Serial.println("[RELAY-HTTP] No response received");
    response = "No response";
    tcp.stop();
    return -2;
  }

  Serial.printf("[RELAY-HTTP] Status: %s\n", statusLine.c_str());

  int httpCode = 0;
  int spaceIdx = statusLine.indexOf(' ');
  if (spaceIdx > 0) {
    httpCode = statusLine.substring(spaceIdx + 1, spaceIdx + 4).toInt();
  }

  while (millis() - respStart < 10000) {
    if (tcp.available()) {
      String line = tcp.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) break;
    }
    if (!tcp.connected() && !tcp.available()) break;
    delay(1);
  }

  response = "";
  while (millis() - respStart < 10000) {
    while (tcp.available()) {
      response += (char)tcp.read();
    }
    if (!tcp.connected() && !tcp.available()) break;
    delay(10);
  }

  Serial.printf("[RELAY-HTTP] Code=%d Body=%s\n", httpCode, response.c_str());

  tcp.stop();
  return httpCode;
}

void clearImageBuffer() {
  memset(imageBuffer, 0, sizeof(imageBuffer));
}

// ─── Pet logic (identical to mini) ───

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
  if (trimmed == "playful" || trimmed == "cuddly" || trimmed == "sleepy" || trimmed == "curious") {
    return trimmed;
  }
  return "curious";
}

String normalizePetMode(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "off" || trimmed == "hangout" || trimmed == "play" || trimmed == "cuddle" || trimmed == "nap" || trimmed == "party" || trimmed == "needy") {
    return trimmed;
  }
  return "hangout";
}

String normalizeCareAction(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "pet" || trimmed == "cheer" || trimmed == "comfort" || trimmed == "dance" || trimmed == "surprise") {
    return trimmed;
  }
  return "pet";
}

String normalizeCompanionHair(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "tuft" || trimmed == "bangs" || trimmed == "spiky" || trimmed == "swoop" || trimmed == "bob" || trimmed == "messy") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionEars(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "cat" || trimmed == "bear" || trimmed == "bunny") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionMustache(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "classic" || trimmed == "curled" || trimmed == "handlebar" || trimmed == "walrus" || trimmed == "pencil" || trimmed == "imperial") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionGlasses(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "round" || trimmed == "square" || trimmed == "visor") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionHeadwear(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "bow" || trimmed == "beanie" || trimmed == "crown") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionPiercing(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "brow" || trimmed == "nose" || trimmed == "lip") {
    return trimmed;
  }
  return "none";
}

String activePetBehavior() {
  if (activePetMode == "off") return "off";
  if (activePetMode != "hangout") return activePetMode;
  return petPersonality;
}

String petAmbientStatus() {
  if (!activeCareAction.isEmpty()) return currentAttentionStatus();
  if (activePetMode == "off") return "Ready";
  if (activePetMode == "play") return "Bright-eyed";
  if (activePetMode == "cuddle") return "Close by";
  if (activePetMode == "nap") return "Quiet";
  if (activePetMode == "party") return "Sparked up";
  if (activePetMode == "needy") return "Looking your way";
  return "Here with you";
}

int clampLevel(int value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return value;
}

int clampAppearancePercent(int value) {
  if (value < 70) return 70;
  if (value > 170) return 170;
  return value;
}

int clampAppearanceOffset(int value) {
  if (value < -24) return -24;
  if (value > 24) return 24;
  return value;
}

int scaleByPercent(int base, int percent) {
  const int clampedPercent = clampAppearancePercent(percent);
  const long scaled = (static_cast<long>(base) * clampedPercent) / 100L;
  if (scaled == 0 && base != 0) return base > 0 ? 1 : -1;
  return static_cast<int>(scaled);
}

String currentAttentionStatus() {
  if (activeCareAction == "pet") return "Enjoying the moment";
  if (activeCareAction == "cheer") return "Lit up";
  if (activeCareAction == "comfort") return "Settled in";
  if (activeCareAction == "dance") return "All spark";
  if (activeCareAction == "surprise") return "Curious";
  if (bondLevel < 30) return "Nearby";
  if (energyLevel < 25) return "Taking it slow";
  if (boredomLevel > 70) return "Restless";
  return "Here with you";
}

String pickAutonomousPetExpression() {
  const String behavior = activePetBehavior();
  const uint8_t cycle = petCycleStep++ % 4;
  if (behavior == "off") return "happy";
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
  if (trigger == "dance") return "star_eyes";
  if (trigger == "pet") return petPersonality == "cuddly" ? "love" : "heart";
  if (trigger == "cheer") return "excited";
  if (trigger == "comfort") return petPersonality == "sleepy" ? "smile" : "kiss";
  if (trigger == "surprise") {
    const uint8_t cycle = petCycleStep++ % 4;
    if (cycle == 0) return "surprised";
    if (cycle == 1) return "star_eyes";
    if (cycle == 2) return "laugh";
    return "tongue";
  }
  if (trigger == "button_clear") return "sad";
  if (trigger == "button_next") return petPersonality == "playful" ? "wink" : "surprised";
  if (trigger == "note") return petPersonality == "cuddly" ? "love" : "thinking";
  if (trigger == "banner") return activePetMode == "party" ? "laugh" : "kiss";
  if (trigger == "personality" || trigger == "pet_mode") return pickAutonomousPetExpression();
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
  if (!transientActive) return;
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
  if (transientActive) restoreTransientMode();
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
  if (persist) persistPetState();
  startTransientExpression(pickReactionExpression("personality"), 2200, petAmbientStatus());
}

void triggerPetMode(const String& petMode, bool persist) {
  activePetMode = normalizePetMode(petMode);
  activeCareAction = "";
  if (activePetMode == "party") {
    boredomLevel = clampLevel(boredomLevel - 20);
    energyLevel = clampLevel(energyLevel - 8);
  }
  if (persist) persistPetState();
  currentMode = MODE_IDLE;
  statusText = petAmbientStatus();
  lastPetBeatMs = 0;
  if (activePetMode == "off") {
    renderCurrentMode();
    publishStatus();
    return;
  }
  startTransientExpression(pickReactionExpression("pet_mode"), 3200, petAmbientStatus());
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
  startTransientExpression(pickReactionExpression(activeCareAction), activeCareAction == "dance" ? 3600 : 2200, currentAttentionStatus());
}

void updateCompanionNeeds() {
  if (millis() - lastCompanionTickMs < 60000UL) return;
  lastCompanionTickMs = millis();
  boredomLevel = clampLevel(boredomLevel + 2);
  energyLevel = clampLevel(energyLevel - 1);
  if (bondLevel > 10) bondLevel = clampLevel(bondLevel - 1);
  if (energyLevel <= 20) activePetMode = "nap";
  else if (boredomLevel >= 80) activePetMode = "needy";
  else if (activePetMode == "needy" && boredomLevel <= 40) activePetMode = "hangout";
  persistPetState();
}

void updatePetBehavior() {
  updateCompanionNeeds();
  const unsigned long now = millis();
  if (transientActive && now >= transientEndsAt) {
    restoreTransientMode();
    return;
  }
  if (currentMode != MODE_IDLE || transientActive) return;
  if (activePetMode == "off") return;

  unsigned long intervalMs = 18000UL;
  if (activePetMode == "party") intervalMs = 9000UL;
  else if (activePetMode == "play") intervalMs = 12000UL;
  else if (activePetMode == "nap") intervalMs = 26000UL;
  else if (activePetMode == "needy") intervalMs = 14000UL;
  else if (petPersonality == "playful") intervalMs = 13000UL;
  else if (petPersonality == "sleepy") intervalMs = 24000UL;

  if (bondLevel < 30) intervalMs = 12000UL;
  if (boredomLevel > 75) intervalMs = 10000UL;
  if (now - lastPetBeatMs < intervalMs) return;

  startTransientExpression(pickAutonomousPetExpression(), 2200, currentAttentionStatus());
}

bool decodeBase64IntoImage(const String& input) {
  size_t decodedLength = 0;
  if (mbedtls_base64_decode(nullptr, 0, &decodedLength,
      reinterpret_cast<const unsigned char*>(input.c_str()), input.length()) != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
    return false;
  }
  if (decodedLength != sizeof(imageBuffer)) return false;
  size_t outputLength = 0;
  const int result = mbedtls_base64_decode(
    imageBuffer, sizeof(imageBuffer), &outputLength,
    reinterpret_cast<const unsigned char*>(input.c_str()), input.length()
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

// ─── TFT rendering helpers ───
// All coordinates are scaled ~2x from the 128×64 OLED version.
// Face occupies the center 240×160 area of the 240×320 screen.
// STATUS_Y is used for a status bar at the bottom.

static const int FACE_OFFSET_Y = 40;  // vertical offset to center the face area
static const int STATUS_BAR_Y = 222;  // y position for status text (within 240px screen)

void drawIconHeart(int cx, int cy, int s) {
  tft.fillCircle(cx - s, cy - s / 3, s, COL_ROSE);
  tft.fillCircle(cx + s, cy - s / 3, s, COL_ROSE);
  tft.fillTriangle(cx - s * 2, cy - s / 3 + 1, cx + s * 2, cy - s / 3 + 1, cx, cy + s * 2 - 1, COL_ROSE);
}

void drawIconStar(int cx, int cy, int r) {
  for (int i = 0; i < 6; i++) {
    float angle = i * 3.14159f / 3.0f;
    int x2 = cx + (int)(r * 0.97f * cos(angle));
    int y2 = cy + (int)(r * 0.97f * sin(angle));
    tft.drawLine(cx, cy, x2, y2, COL_GOLD);
  }
  tft.fillCircle(cx, cy, r / 3, COL_GOLD);
}

void drawIconFlower(int cx, int cy, int r) {
  for (int i = 0; i < 5; i++) {
    float angle = i * 3.14159f * 2.0f / 5.0f;
    int px = cx + (int)(r * cos(angle));
    int py = cy + (int)(r * sin(angle));
    tft.fillCircle(px, py, r / 2, userAccentColor);
  }
  tft.fillCircle(cx, cy, r / 2 + 1, userAccentColor);
}

void drawIconNote(int cx, int cy, int s) {
  tft.fillCircle(cx - s / 2, cy + s - 1, s - 2, userAccentColor);
  tft.drawLine(cx - s / 2 + s - 3, cy + s - 1, cx - s / 2 + s - 3, cy - s, userAccentColor);
  tft.drawLine(cx - s / 2 + s - 3, cy - s, cx + s, cy - s + 2, userAccentColor);
}

void drawIconMoon(int cx, int cy, int r) {
  tft.fillCircle(cx, cy, r, userAccentColor);
  tft.fillCircle(cx + r / 2, cy - r / 3, r - 1, COL_BG);
}

void drawNoteBorder(int style) {
  switch (style) {
    case 1:
      tft.drawRoundRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, 12, userFaceColor);
      break;
    case 2:
      for (int x = 10; x < SCREEN_WIDTH - 10; x += 12) {
        tft.drawLine(x, 6, x + 6, 6, userFaceColor);
        tft.drawLine(x, SCREEN_HEIGHT - 7, x + 6, SCREEN_HEIGHT - 7, userFaceColor);
      }
      for (int y = 10; y < SCREEN_HEIGHT - 10; y += 12) {
        tft.drawLine(6, y, 6, y + 6, userFaceColor);
        tft.drawLine(SCREEN_WIDTH - 7, y, SCREEN_WIDTH - 7, y + 6, userFaceColor);
      }
      break;
    case 3:
      drawIconHeart(16, 16, 5);
      drawIconHeart(SCREEN_WIDTH - 16, 16, 5);
      drawIconHeart(16, SCREEN_HEIGHT - 16, 5);
      drawIconHeart(SCREEN_WIDTH - 16, SCREEN_HEIGHT - 16, 5);
      break;
    case 4:
      for (int x = 8; x < SCREEN_WIDTH; x += 10) {
        tft.fillCircle(x, 5, 2, COL_LAVENDER);
        tft.fillCircle(x, SCREEN_HEIGHT - 6, 2, COL_LAVENDER);
      }
      for (int y = 14; y < SCREEN_HEIGHT - 10; y += 10) {
        tft.fillCircle(5, y, 2, COL_LAVENDER);
        tft.fillCircle(SCREEN_WIDTH - 6, y, 2, COL_LAVENDER);
      }
      break;
    default:
      break;
  }
}

void drawNoteIcons(const String& icons, int y, int count) {
  if (icons.length() == 0 || count == 0) return;
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
  int totalW = drawCount * 24;
  int startX = (SCREEN_WIDTH - totalW) / 2 + 12;
  for (int i = 0; i < drawCount; i++) {
    int cx = startX + i * 24;
    const String& name = names[i];
    if (name == "heart")  drawIconHeart(cx, y, 5);
    else if (name == "star")   drawIconStar(cx, y, 8);
    else if (name == "flower") drawIconFlower(cx, y, 7);
    else if (name == "note")   drawIconNote(cx, y, 8);
    else if (name == "moon")   drawIconMoon(cx, y, 7);
  }
}

void drawLineWithSymbols(const String& line, int startX, int startY, int fontSize) {
  int cx = startX;
  int cy = startY;
  int charW = 6 * fontSize;
  int charH = 8 * fontSize;
  int iconS = charH - 2;
  int i = 0;
  while (i < (int)line.length()) {
    if (line[i] == '<' && i + 1 < (int)line.length()) {
      if (line[i+1] == '3') {
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
    tft.setCursor(cx, cy);
    tft.print(line[i]);
    cx += charW;
    i++;
  }
}

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
  if (!displayAvailable) return;
  tft.fillScreen(COL_BG);
  drawNoteBorder(border);
  tft.setTextColor(userFaceColor);
  // Scale font up: size 1 on OLED → size 2 on TFT for readability
  const int safeFontSize = fontSize < 1 ? 2 : (fontSize > 4 ? 4 : fontSize + 1);
  const bool hasIcons = icons.length() > 0;
  const int topPad = hasIcons ? 30 : 10;
  const int horizontalPadding = (border > 0) ? 16 : 8;
  const int availH = SCREEN_HEIGHT - topPad - 10;
  const int lineHeight = (8 * safeFontSize) + 4;
  const int maxLines = availH / lineHeight;
  const int maxChars = (SCREEN_WIDTH - (horizontalPadding * 2)) / (6 * safeFontSize);
  String lines[maxLines > 20 ? 20 : maxLines];
  int lineCount = 0;
  String remaining = text;
  remaining.trim();

  int usableMaxLines = maxLines > 20 ? 20 : maxLines;
  while (remaining.length() > 0 && lineCount < usableMaxLines) {
    int lineLength = (int)remaining.length() > maxChars ? maxChars : (int)remaining.length();
    if (lineLength < (int)remaining.length()) {
      const int split = remaining.lastIndexOf(' ', lineLength);
      if (split > 0) lineLength = split;
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

  tft.setTextSize(safeFontSize);
  tft.setTextWrap(false);
  const int totalHeight = lineCount * lineHeight;
  int cursorY = topPad + (availH - totalHeight) / 2;
  if (cursorY < topPad) cursorY = topPad;

  for (int i = 0; i < lineCount; i++) {
    int w = lineVisualWidth(lines[i], safeFontSize);
    int startX = (SCREEN_WIDTH - w) / 2;
    drawLineWithSymbols(lines[i], startX, cursorY, safeFontSize);
    cursorY += lineHeight;
  }

  if (hasIcons) drawNoteIcons(icons, 14, 8);
}

void renderBannerFrame() {
  if (!displayAvailable) return;
  tft.setTextSize(4);
  tft.setTextWrap(false);
  if (bannerOffset >= SCREEN_WIDTH) {
    // First frame of this banner — clear whole canvas once
    tft.fillScreen(COL_BG);
  } else {
    // Subsequent frames — only erase the text strip (~40px tall) to avoid full-screen blank flash
    tft.fillRect(0, SCREEN_HEIGHT / 2 - 20, SCREEN_WIDTH, 46, COL_BG);
  }
  tft.setTextColor(COL_PINK);
  tft.setCursor(bannerOffset, SCREEN_HEIGHT / 2 - 16);
  tft.print(currentBanner);
}

void renderImage() {
  if (!displayAvailable) return;
  tft.fillScreen(COL_BG);
  tft.drawBitmap(0, 0, imageBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, userFaceColor);
}

// ─── Face rendering (scaled 2× from 128×64) ───
// OLED face area 128×64 → TFT face area ~240×120 centered at FACE_OFFSET_Y.
// Scale factor S=1.875 for coordinates.

static inline float ease(int phase, int max) {
  float t = (float)phase / (float)max;
  return t < 0.5f ? 2.0f * t : 2.0f * (1.0f - t);
}

void drawEye(int cx, int cy, int w, int h, int r, int pupilDx, int pupilDy) {
  int clampedH = h < 5 ? 5 : h;
  tft.fillRoundRect(cx - w / 2, cy - clampedH / 2, w, clampedH, r, userEyeColor);
  if (clampedH >= 18) {
    tft.fillCircle(cx + pupilDx, cy + pupilDy, 9, COL_BG);
  } else if (clampedH >= 10) {
    tft.fillCircle(cx + pupilDx, cy + pupilDy, 5, COL_BG);
  }
}

void drawBlinkEye(int cx, int cy, int w, int h, int r) {
  int clampedH = h < 5 ? 5 : h;
  tft.fillRoundRect(cx - w / 2, cy - clampedH / 2, w, clampedH, r, userEyeColor);
}

void drawHappyArc(int cx, int cy, int w) {
  for (int t = 0; t < 8; t++) {
    int hw = w / 2;
    tft.drawLine(cx - hw, cy + 8 + t, cx, cy - 8 + t, userFaceColor);
    tft.drawLine(cx, cy - 8 + t, cx + hw, cy + 8 + t, userFaceColor);
  }
}

void drawSadArc(int cx, int cy, int w) {
  for (int t = 0; t < 5; t++) {
    int hw = w / 2;
    tft.drawLine(cx - hw, cy - 5 + t, cx, cy + 9 + t, userFaceColor);
    tft.drawLine(cx, cy + 9 + t, cx + hw, cy - 5 + t, userFaceColor);
  }
}

void drawSmile(int cx, int cy, int w) {
  for (int t = 0; t < 5; t++) {
    int hw = w / 2;
    tft.drawLine(cx - hw, cy + t, cx - hw / 3, cy + 12 + t, userFaceColor);
    tft.drawLine(cx - hw / 3, cy + 12 + t, cx + hw / 3, cy + 12 + t, userFaceColor);
    tft.drawLine(cx + hw / 3, cy + 12 + t, cx + hw, cy + t, userFaceColor);
  }
}

void drawOvalMouth(int cx, int cy, int rw, int rh) {
  for (int t = 0; t < 3; t++) {
    tft.drawRoundRect(cx - rw, cy - rh + t, rw * 2, rh * 2, rh, userFaceColor);
  }
}

void drawKissLips(int cx, int cy) {
  tft.fillCircle(cx, cy, 10, userFaceColor);
  tft.fillCircle(cx, cy, 5, COL_BG);
}

void drawBigHeart(int cx, int cy, int s) {
  tft.fillCircle(cx - s, cy - s / 3, s, COL_ROSE);
  tft.fillCircle(cx + s, cy - s / 3, s, COL_ROSE);
  tft.fillTriangle(cx - s * 2, cy - s / 3 + 1, cx + s * 2, cy - s / 3 + 1, cx, cy + s * 2, COL_ROSE);
}

void drawTear(int cx, int cy, int s) {
  tft.fillCircle(cx, cy + s, s, userFaceColor);
  tft.fillTriangle(cx - s, cy + s, cx + s, cy + s, cx, cy, userFaceColor);
}

void drawBrow(int x1, int y1, int x2, int y2) {
  for (int t = 0; t < 5; t++) {
    tft.drawLine(x1, y1 + t, x2, y2 + t, userFaceColor);
  }
}

void drawCompanionAccessories(int leftX, int rightX, int eyeY, int mouthY) {
  const int faceCenterX = (leftX + rightX) / 2;
  const int hairSize = clampAppearancePercent(companionHairSize);
  const int mustacheSize = clampAppearancePercent(companionMustacheSize);
  const int hairWidth = scaleByPercent(hairSize, companionHairWidth);
  const int hairHeight = scaleByPercent(hairSize, companionHairHeight);
  const int hairThickness = clampAppearancePercent(companionHairThickness);
  const int hairStroke = 2 + scaleByPercent(3, hairThickness);
  const int hairCenterX = faceCenterX + clampAppearanceOffset(companionHairOffsetX) * 2;
  const int hairCenterY = eyeY + clampAppearanceOffset(companionHairOffsetY) * 2;
  const int mustacheWidth = scaleByPercent(mustacheSize, companionMustacheWidth);
  const int mustacheHeight = scaleByPercent(mustacheSize, companionMustacheHeight);
  const int mustacheThickness = clampAppearancePercent(companionMustacheThickness);
  const int mustacheStroke = 2 + scaleByPercent(3, mustacheThickness);
  const int mustacheCenterX = faceCenterX + clampAppearanceOffset(companionMustacheOffsetX) * 2;
  const int mustacheCenterY = mouthY + clampAppearanceOffset(companionMustacheOffsetY) * 2;

  // Ears (scaled 2x)
  if (companionEars == "cat") {
    tft.drawTriangle(leftX - 30, eyeY - 34, leftX - 14, eyeY - 56, leftX + 4, eyeY - 34, userFaceColor);
    tft.drawTriangle(rightX - 4, eyeY - 34, rightX + 14, eyeY - 56, rightX + 30, eyeY - 34, userFaceColor);
  } else if (companionEars == "bear") {
    tft.drawCircle(leftX - 22, eyeY - 38, 11, userFaceColor);
    tft.drawCircle(rightX + 22, eyeY - 38, 11, userFaceColor);
  } else if (companionEars == "bunny") {
    tft.drawRoundRect(leftX - 28, eyeY - 64, 14, 30, 7, userFaceColor);
    tft.drawRoundRect(rightX + 14, eyeY - 64, 14, 30, 7, userFaceColor);
  }

  // Hair (scaled 2x)
  if (companionHair == "tuft") {
    const int lift = scaleByPercent(22, hairHeight);
    const int spread = scaleByPercent(9, hairWidth);
    for (int stroke = 0; stroke < hairStroke; stroke++) {
      tft.drawLine(hairCenterX - spread, hairCenterY - 22 + stroke, hairCenterX, hairCenterY - 22 - lift + stroke, userFaceColor);
      tft.drawLine(hairCenterX, hairCenterY - 22 - lift + stroke, hairCenterX + spread, hairCenterY - 22 + stroke, userFaceColor);
      tft.drawLine(hairCenterX, hairCenterY - 22 - lift + stroke, hairCenterX + 2, hairCenterY - 14 + stroke, userFaceColor);
    }
  } else if (companionHair == "bangs") {
    const int topY = hairCenterY - 22 - scaleByPercent(5, hairHeight);
    const int leftEdge = hairCenterX - (scaleByPercent(68, hairWidth) / 2);
    const int rightEdge = hairCenterX + (scaleByPercent(68, hairWidth) / 2);
    for (int stroke = 0; stroke < hairStroke; stroke++) {
      tft.drawLine(leftEdge, topY + stroke, rightEdge, topY + stroke, userFaceColor);
    }
    for (int x = leftX - 26; x <= rightX + 26; x += 14) {
      const int shiftedX = hairCenterX + (x - faceCenterX);
      for (int stroke = 0; stroke < hairStroke; stroke++) {
        tft.drawLine(shiftedX, topY + 2 + stroke, shiftedX + scaleByPercent(5, hairWidth), hairCenterY - 12 + stroke, userFaceColor);
      }
    }
  } else if (companionHair == "spiky") {
    for (int x = leftX - 26; x <= rightX + 26; x += 18) {
      const int shiftedX = hairCenterX + (x - faceCenterX);
      const int peak = hairCenterY - 18 - scaleByPercent(16, hairHeight);
      for (int stroke = 0; stroke < hairStroke; stroke++) {
        tft.drawLine(shiftedX, hairCenterY - 18 + stroke, shiftedX + scaleByPercent(7, hairWidth), peak + stroke, userFaceColor);
        tft.drawLine(shiftedX + scaleByPercent(7, hairWidth), peak + stroke, shiftedX + scaleByPercent(14, hairWidth), hairCenterY - 18 + stroke, userFaceColor);
      }
    }
  } else if (companionHair == "swoop") {
    const int topY = hairCenterY - 22 - scaleByPercent(11, hairHeight);
    for (int stroke = 0; stroke < hairStroke; stroke++) {
      tft.drawLine(hairCenterX - scaleByPercent(34, hairWidth), hairCenterY - 16 + stroke, hairCenterX + scaleByPercent(22, hairWidth), topY + stroke, userFaceColor);
      tft.drawLine(hairCenterX + scaleByPercent(22, hairWidth), topY + stroke, hairCenterX + scaleByPercent(52, hairWidth), hairCenterY - 9 + stroke, userFaceColor);
      tft.drawLine(hairCenterX - scaleByPercent(18, hairWidth), hairCenterY - 18 + stroke, hairCenterX + scaleByPercent(7, hairWidth), topY + 3 + stroke, userFaceColor);
    }
  } else if (companionHair == "bob") {
    const int topY = hairCenterY - 20 - scaleByPercent(7, hairHeight);
    const int width = scaleByPercent((rightX - leftX) + 68, hairWidth);
    tft.drawRoundRect(hairCenterX - width / 2, topY, width, 18 + scaleByPercent(7, hairHeight), 9, userFaceColor);
    for (int stroke = 0; stroke < hairStroke; stroke++) {
      tft.drawLine(hairCenterX - width / 2, hairCenterY - 2 + stroke, hairCenterX - width / 2 + scaleByPercent(11, hairWidth), hairCenterY + 12 + stroke, userFaceColor);
      tft.drawLine(hairCenterX + width / 2, hairCenterY - 2 + stroke, hairCenterX + width / 2 - scaleByPercent(11, hairWidth), hairCenterY + 12 + stroke, userFaceColor);
    }
  } else if (companionHair == "messy") {
    for (int x = leftX - 18; x <= rightX + 22; x += 16) {
      const int shiftedX = hairCenterX + (x - faceCenterX);
      const int peak = hairCenterY - 14 - scaleByPercent(13, hairHeight) + ((x / 16) % 2 == 0 ? 0 : 5);
      for (int stroke = 0; stroke < hairStroke; stroke++) {
        tft.drawLine(shiftedX, hairCenterY - 14 + stroke, shiftedX + scaleByPercent(5, hairWidth), peak + stroke, userFaceColor);
        tft.drawLine(shiftedX + scaleByPercent(5, hairWidth), peak + stroke, shiftedX + scaleByPercent(11, hairWidth), hairCenterY - 16 + stroke, userFaceColor);
      }
    }
  }

  // Headwear (scaled 2x)
  if (companionHeadwear == "bow") {
    tft.drawTriangle(faceCenterX - 7, eyeY - 44, faceCenterX - 30, eyeY - 34, faceCenterX - 14, eyeY - 22, userFaceColor);
    tft.drawTriangle(faceCenterX + 7, eyeY - 44, faceCenterX + 30, eyeY - 34, faceCenterX + 14, eyeY - 22, userFaceColor);
    tft.drawCircle(faceCenterX, eyeY - 34, 4, userFaceColor);
  } else if (companionHeadwear == "beanie") {
    tft.drawRoundRect(faceCenterX - 44, eyeY - 52, 88, 22, 9, userFaceColor);
    tft.drawLine(faceCenterX - 38, eyeY - 28, faceCenterX + 38, eyeY - 28, userFaceColor);
    tft.drawCircle(faceCenterX, eyeY - 56, 5, userFaceColor);
  } else if (companionHeadwear == "crown") {
    tft.drawLine(faceCenterX - 38, eyeY - 34, faceCenterX + 38, eyeY - 34, userFaceColor);
    tft.drawLine(faceCenterX - 38, eyeY - 34, faceCenterX - 22, eyeY - 56, userFaceColor);
    tft.drawLine(faceCenterX - 22, eyeY - 56, faceCenterX - 4, eyeY - 34, userFaceColor);
    tft.drawLine(faceCenterX - 4, eyeY - 34, faceCenterX + 11, eyeY - 60, userFaceColor);
    tft.drawLine(faceCenterX + 11, eyeY - 60, faceCenterX + 26, eyeY - 34, userFaceColor);
    tft.drawLine(faceCenterX + 26, eyeY - 34, faceCenterX + 38, eyeY - 52, userFaceColor);
  }

  // Glasses (scaled 2x)
  if (companionGlasses == "round") {
    tft.drawCircle(leftX, eyeY, 22, userFaceColor);
    tft.drawCircle(rightX, eyeY, 22, userFaceColor);
    tft.drawLine(leftX + 22, eyeY, rightX - 22, eyeY, userFaceColor);
  } else if (companionGlasses == "square") {
    tft.drawRoundRect(leftX - 26, eyeY - 20, 52, 40, 7, userFaceColor);
    tft.drawRoundRect(rightX - 26, eyeY - 20, 52, 40, 7, userFaceColor);
    tft.drawLine(leftX + 26, eyeY, rightX - 26, eyeY, userFaceColor);
  } else if (companionGlasses == "visor") {
    tft.drawRoundRect(leftX - 34, eyeY - 22, (rightX - leftX) + 68, 38, 11, userFaceColor);
  }

  // Mustache (scaled 2x)
  if (companionMustache == "classic") {
    const int wing = scaleByPercent(22, mustacheWidth);
    const int inner = scaleByPercent(7, mustacheWidth);
    const int rise = scaleByPercent(7, mustacheHeight);
    for (int stroke = 0; stroke < mustacheStroke; stroke++) {
      tft.drawLine(mustacheCenterX - wing, mustacheCenterY - rise + stroke, mustacheCenterX - inner, mustacheCenterY - 2 + stroke, userFaceColor);
      tft.drawLine(mustacheCenterX - wing, mustacheCenterY - rise + 2 + stroke, mustacheCenterX - inner, mustacheCenterY + 2 + stroke, userFaceColor);
      tft.drawLine(mustacheCenterX + inner, mustacheCenterY - 2 + stroke, mustacheCenterX + wing, mustacheCenterY - rise + stroke, userFaceColor);
      tft.drawLine(mustacheCenterX + inner, mustacheCenterY + 2 + stroke, mustacheCenterX + wing, mustacheCenterY - rise + 2 + stroke, userFaceColor);
    }
  } else if (companionMustache == "curled") {
    const int wing = scaleByPercent(22, mustacheWidth);
    const int rise = scaleByPercent(4, mustacheHeight);
    for (int stroke = 0; stroke < mustacheStroke; stroke++) {
      tft.drawLine(mustacheCenterX - wing, mustacheCenterY - rise + stroke, mustacheCenterX - 4, mustacheCenterY - 2 + stroke, userFaceColor);
      tft.drawLine(mustacheCenterX + 4, mustacheCenterY - 2 + stroke, mustacheCenterX + wing, mustacheCenterY - rise + stroke, userFaceColor);
    }
    tft.drawCircle(mustacheCenterX - wing - 4, mustacheCenterY - rise - 2, 4, userFaceColor);
    tft.drawCircle(mustacheCenterX + wing + 4, mustacheCenterY - rise - 2, 4, userFaceColor);
  } else if (companionMustache == "handlebar") {
    const int wing = scaleByPercent(26, mustacheWidth);
    const int curl = scaleByPercent(9, mustacheHeight);
    for (int stroke = 0; stroke < mustacheStroke; stroke++) {
      tft.drawLine(mustacheCenterX - wing, mustacheCenterY - 2 + stroke, mustacheCenterX - 4, mustacheCenterY + stroke, userFaceColor);
      tft.drawLine(mustacheCenterX + 4, mustacheCenterY + stroke, mustacheCenterX + wing, mustacheCenterY - 2 + stroke, userFaceColor);
      tft.drawLine(mustacheCenterX - wing, mustacheCenterY - 2 + stroke, mustacheCenterX - wing - scaleByPercent(7, mustacheWidth), mustacheCenterY - curl + stroke, userFaceColor);
      tft.drawLine(mustacheCenterX + wing, mustacheCenterY - 2 + stroke, mustacheCenterX + wing + scaleByPercent(7, mustacheWidth), mustacheCenterY - curl + stroke, userFaceColor);
    }
  } else if (companionMustache == "walrus") {
    const int width = scaleByPercent(26, mustacheWidth);
    const int height = scaleByPercent(7, mustacheHeight);
    tft.fillRoundRect(mustacheCenterX - width, mustacheCenterY - 11, width * 2, height + 4, 5, userFaceColor);
    tft.fillRect(mustacheCenterX - scaleByPercent(4, mustacheThickness), mustacheCenterY - 5, scaleByPercent(7, mustacheThickness), height + 7, userFaceColor);
  } else if (companionMustache == "pencil") {
    const int width = scaleByPercent(24, mustacheWidth);
    for (int stroke = 0; stroke < mustacheStroke; stroke++) {
      tft.drawLine(mustacheCenterX - width, mustacheCenterY - 4 + stroke, mustacheCenterX + width, mustacheCenterY - 4 + stroke, userFaceColor);
      tft.drawLine(mustacheCenterX - width + scaleByPercent(4, mustacheThickness), mustacheCenterY - 2 + stroke, mustacheCenterX + width - scaleByPercent(4, mustacheThickness), mustacheCenterY - 2 + stroke, userFaceColor);
    }
  } else if (companionMustache == "imperial") {
    const int wing = scaleByPercent(22, mustacheWidth);
    const int rise = scaleByPercent(16, mustacheHeight);
    for (int stroke = 0; stroke < mustacheStroke; stroke++) {
      tft.drawLine(mustacheCenterX - wing, mustacheCenterY - 4 + stroke, mustacheCenterX - 2, mustacheCenterY - 2 + stroke, userFaceColor);
      tft.drawLine(mustacheCenterX + 2, mustacheCenterY - 2 + stroke, mustacheCenterX + wing, mustacheCenterY - 4 + stroke, userFaceColor);
      tft.drawLine(mustacheCenterX - wing, mustacheCenterY - 4 + stroke, mustacheCenterX - wing - scaleByPercent(4, mustacheWidth), mustacheCenterY - rise + stroke, userFaceColor);
      tft.drawLine(mustacheCenterX + wing, mustacheCenterY - 4 + stroke, mustacheCenterX + wing + scaleByPercent(4, mustacheWidth), mustacheCenterY - rise + stroke, userFaceColor);
    }
  }

  // Piercings (scaled 2x)
  if (companionPiercing == "brow") {
    tft.drawLine(rightX + 11, eyeY - 26, rightX + 26, eyeY - 22, userFaceColor);
    tft.drawPixel(rightX + 14, eyeY - 24, userFaceColor);
  } else if (companionPiercing == "nose") {
    tft.drawCircle(faceCenterX + 7, mouthY - 18, 4, userFaceColor);
  } else if (companionPiercing == "lip") {
    tft.drawCircle(faceCenterX + 14, mouthY + 4, 4, userFaceColor);
  }
}

void drawZzz(int x, int y, int phase) {
  int p = phase % 64;
  tft.setTextSize(2);
  tft.setTextColor(userAccentColor);
  int drift = p / 4;
  if (p >= 8)  { tft.setCursor(x,      y + 22 - drift * 2); tft.print('z'); }
  if (p >= 24) { tft.setCursor(x + 12, y + 10 - drift);     tft.print('z'); }
  if (p >= 40) { tft.setCursor(x + 26, y - drift / 2);      tft.print('Z'); }
}

// ─── Expression renderer (scaled 2× from mini, face centered at FACE_OFFSET_Y) ───

void renderExpressionFrame() {
  if (!displayAvailable) return;
  // Clear face/content area only — preserves clock strip (top 40px) and status bar
  tft.fillRect(0, FACE_OFFSET_Y, SCREEN_WIDTH, SCREEN_HEIGHT - 18 - FACE_OFFSET_Y, COL_BG);

  // Scaled face coordinates: OLED LX=36→68, RX=92→172, EY=24→45+offset, MY=52→98+offset
  const int LX = 68;
  const int RX = 172;
  const int eyeYShift = clampAppearanceOffset(companionEyeOffsetY) * 2;
  const int mouthYShift = clampAppearanceOffset(companionMouthOffsetY) * 2;
  const int EY = FACE_OFFSET_Y + 45 + eyeYShift;
  const int MY = FACE_OFFSET_Y + 100 + mouthYShift;
  const int EW = 52;
  const int EH = 40;
  const int ER = 13;

  const int ph = expressionPhase % 64;
  const float t = (float)ph / 63.0f;

  if (currentExpression == "heart") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int s = 14 + (int)(10.0f * wave);
    drawBigHeart(SCREEN_WIDTH / 2, FACE_OFFSET_Y + 80, s);
  } else if (currentExpression == "love") {
    drawEye(LX, EY, EW, EH, ER, 0, 0);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    // Blush cheeks
    tft.fillCircle(LX + 22, EY + 18, 7, COL_PINK);
    tft.fillCircle(RX - 22, EY + 18, 7, COL_PINK);
    drawBigHeart(LX, EY, 7);
    drawBigHeart(RX, EY, 7);
    drawSmile(SCREEN_WIDTH / 2, MY - 4, 52);
    float r1 = fmod(t * 2.0f, 1.0f);
    float r2 = fmod(t * 2.0f + 0.5f, 1.0f);
    if (r1 < 0.8f) {
      int y1 = MY - 8 - (int)(r1 * 75.0f);
      drawBigHeart(30 + (int)(r1 * 18.0f), y1 + eyeYShift, 5);
    }
    if (r2 < 0.8f) {
      int y2 = MY - 8 - (int)(r2 * 68.0f);
      drawBigHeart(210 - (int)(r2 * 18.0f), y2 + eyeYShift, 4);
    }
  } else if (currentExpression == "surprised") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int eyeH = EH + (int)(wave * 14.0f);
    int browLift = (int)(wave * 7.0f);
    int mouthR = 9 + (int)(wave * 5.0f);
    drawEye(LX, EY, EW + 7, eyeH, ER + 3, 0, 0);
    drawEye(RX, EY, EW + 7, eyeH, ER + 3, 0, 0);
    drawBrow(LX - 26, EY - 32 - browLift, LX + 26, EY - 32 - browLift);
    drawBrow(RX - 26, EY - 32 - browLift, RX + 26, EY - 32 - browLift);
    drawOvalMouth(SCREEN_WIDTH / 2, MY, mouthR, mouthR - 1);
  } else if (currentExpression == "angry") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int lidH = 7 + (int)(wave * 5.0f);
    int twitch = (int)(wave * 4.0f);
    int mouthShift = (int)(sin(t * 3.14159f * 6.0f) * 4.0f);
    drawEye(LX, EY + 4, EW, EH - 4, ER, 0, 4);
    drawEye(RX, EY + 4, EW, EH - 4, ER, 0, 4);
    tft.fillRect(LX - EW / 2, EY + 4 - (EH - 4) / 2, EW, lidH, COL_BG);
    tft.fillRect(RX - EW / 2, EY + 4 - (EH - 4) / 2, EW, lidH, COL_BG);
    drawBrow(LX - 26, EY - 30, LX + 14, EY - 11 - twitch);
    drawBrow(RX + 26, EY - 30, RX - 14, EY - 11 - twitch);
    for (int line = 0; line < 5; line++) {
      tft.drawLine(SCREEN_WIDTH / 2 - 18, MY + line + mouthShift, SCREEN_WIDTH / 2 + 18, MY + line + mouthShift, userFaceColor);
    }
  } else if (currentExpression == "sad") {
    drawEye(LX, EY + 6, EW, EH - 7, ER, 0, 7);
    drawEye(RX, EY + 6, EW, EH - 7, ER, 0, 7);
    drawBrow(LX - 26, EY - 11, LX + 18, EY - 30);
    drawBrow(RX + 26, EY - 11, RX - 18, EY - 30);
    drawSadArc(SCREEN_WIDTH / 2, MY - 4, 30);
    if (ph >= 12) {
      float tearT = (float)(ph - 12) / 19.0f;
      int tearY = EY + 26 + (int)(tearT * 44.0f);
      int tearS = tearT < 0.5f ? 5 : 4;
      drawTear(LX + EW / 2 + 4, tearY, tearS);
    }
    if (ph >= 20) {
      float tearT = (float)(ph - 20) / 11.0f;
      int tearY = EY + 26 + (int)(tearT * 40.0f);
      drawTear(RX + EW / 2 + 4, tearY, 4);
    }
  } else if (currentExpression == "sleepy") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int eyeH = 9 + (int)((1.0f - wave) * 9.0f);
    drawEye(LX, EY, EW, eyeH, ER, 0, 0);
    drawEye(RX, EY, EW, eyeH, ER, 0, 0);
    for (int line = 0; line < 4; line++) {
      tft.drawLine(SCREEN_WIDTH / 2 - 14, MY + line, SCREEN_WIDTH / 2 + 14, MY + line, userFaceColor);
    }
    drawZzz(186, 22 + eyeYShift, expressionPhase);
  } else if (currentExpression == "thinking") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int px = 4 + (int)(wave * 9.0f);
    int py = -4 + (int)(wave * 5.0f);
    int bubble = 4 + (int)(wave * 4.0f);
    drawEye(LX, EY, EW - 11, 22, ER, px, py);
    drawEye(RX, EY, EW, EH, ER, px, py);
    tft.fillCircle(204, FACE_OFFSET_Y + 72 + eyeYShift, bubble, userAccentColor);
    tft.fillCircle(216, FACE_OFFSET_Y + 52 + eyeYShift, bubble + 2, userAccentColor);
    tft.fillCircle(224, FACE_OFFSET_Y + 30 + eyeYShift, bubble + 4, userAccentColor);
    for (int line = 0; line < 4; line++) {
      tft.drawLine(SCREEN_WIDTH / 2 - 14, MY + line, SCREEN_WIDTH / 2 + 11, MY - 4 + line, userFaceColor);
    }
  } else if (currentExpression == "happy") {
    int eyeH = EH;
    int py = (int)(sin(t * 3.14159f * 2.0f) * 4.0f);
    if (ph >= 24 && ph <= 27) {
      float blinkT = (float)(ph - 24) / 3.0f;
      if (ph <= 25) {
        eyeH = EH - (int)(blinkT * (EH - 5));
      } else {
        eyeH = 5 + (int)(blinkT * (EH - 5));
      }
    }
    drawEye(LX, EY, EW, eyeH, ER, 0, py);
    drawEye(RX, EY, EW, eyeH, ER, 0, py);
    drawSmile(SCREEN_WIDTH / 2, MY - 5, 44);
  } else if (currentExpression == "smile") {
    drawHappyArc(LX, EY, EW);
    drawHappyArc(RX, EY, EW);
    drawSmile(SCREEN_WIDTH / 2, MY - 4, 44);
  } else if (currentExpression == "confused") {
    float wave = sin(t * 3.14159f * 2.0f);
    int browTwitch = (int)(wave * 5.0f);
    drawEye(LX, EY - 4, EW, EH + 4, ER, -4, 0);
    drawEye(RX, EY + 6, EW - 11, EH - 11, ER - 4, 4, 0);
    drawBrow(LX - 26, EY - 28 - browTwitch, LX + 18, EY - 20);
    drawBrow(RX - 14, EY - 16, RX + 26, EY - 24 + browTwitch);
    for (int line = 0; line < 5; line++) {
      tft.drawLine(SCREEN_WIDTH / 2 - 22, MY + 7 + line, SCREEN_WIDTH / 2 + 22, MY - 4 + line, userFaceColor);
    }
  } else if (currentExpression == "look_around") {
    float sx = sin(t * 3.14159f * 2.0f);
    float sy = cos(t * 3.14159f * 4.0f) * 0.4f;
    int px = (int)(sx * 9.0f);
    int py = (int)(sy * 5.0f);
    drawEye(LX, EY, EW, EH, ER, px, py);
    drawEye(RX, EY, EW, EH, ER, px, py);
    for (int line = 0; line < 4; line++) {
      tft.drawLine(SCREEN_WIDTH / 2 - 14, MY + line, SCREEN_WIDTH / 2 + 14, MY + line, userFaceColor);
    }
  } else if (currentExpression == "kiss") {
    float r1 = fmod(t * 1.5f, 1.0f);
    float r2 = fmod(t * 1.5f + 0.45f, 1.0f);
    drawBlinkEye(LX, EY, EW, 7, ER);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    // Blush cheeks
    tft.fillCircle(LX + 20, EY + 14, 6, COL_PINK);
    tft.fillCircle(RX - 20, EY + 14, 6, COL_PINK);
    drawKissLips(SCREEN_WIDTH / 2, MY);
    if (r1 < 0.85f) {
      int hx = SCREEN_WIDTH / 2 - 18 + (int)(sin(r1 * 3.14159f) * 14.0f);
      int hy = MY - 18 - (int)(r1 * 70.0f);
      drawBigHeart(hx, hy, 5);
    }
    if (r2 < 0.85f) {
      int hx = SCREEN_WIDTH / 2 + 22 - (int)(sin(r2 * 3.14159f) * 11.0f);
      int hy = MY - 14 - (int)(r2 * 62.0f);
      drawBigHeart(hx, hy, 4);
    }
  } else if (currentExpression == "wink") {
    for (int line = 0; line < 5; line++) {
      tft.drawLine(LX - EW / 2, EY + line, LX + EW / 2, EY + line, userEyeColor);
    }
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    drawSmile(SCREEN_WIDTH / 2, MY - 4, 38);
  } else if (currentExpression == "laugh") {
    float wave = sin(t * 3.14159f * 4.0f) * 0.5f + 0.5f;
    int shakeX = (int)(wave * 5.0f) - 2;
    int mouthW = 38 + (int)(wave * 7.0f);
    drawHappyArc(LX + shakeX, EY, EW);
    drawHappyArc(RX + shakeX, EY, EW);
    tft.fillRoundRect(SCREEN_WIDTH / 2 - mouthW / 2, MY - 9, mouthW, 22, 7, userFaceColor);
    tft.fillRoundRect(SCREEN_WIDTH / 2 - mouthW / 2 + 4, MY - 5, mouthW - 8, 14, 5, COL_BG);
  } else if (currentExpression == "star_eyes") {
    float twinkle = sin(t * 3.14159f * 4.0f) * 0.5f + 0.5f;
    int starR = 9 + (int)(twinkle * 4.0f);
    drawEye(LX, EY, EW, EH, ER, 0, 0);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    drawIconStar(LX, EY, starR);
    drawIconStar(RX, EY, starR);
    drawSmile(SCREEN_WIDTH / 2, MY - 4, 44);
  } else if (currentExpression == "excited") {
    float bounce = sin(t * 3.14159f * 4.0f) * 0.5f + 0.5f;
    int eyeShift = (int)(bounce * 7.0f);
    drawEye(LX, EY - eyeShift, EW + 7, EH + 7, ER, 0, 0);
    drawEye(RX, EY - eyeShift, EW + 7, EH + 7, ER, 0, 0);
    drawBrow(LX - 26, EY - 38 - eyeShift, LX + 26, EY - 38 - eyeShift);
    drawBrow(RX - 26, EY - 38 - eyeShift, RX + 26, EY - 38 - eyeShift);
    drawSmile(SCREEN_WIDTH / 2, MY - 4 - (int)(bounce * 4.0f), 52);
  } else if (currentExpression == "tongue") {
    for (int line = 0; line < 5; line++) {
      tft.drawLine(LX - EW / 2, EY + line, LX + EW / 2, EY + line, userEyeColor);
    }
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    for (int line = 0; line < 4; line++) {
      tft.drawLine(SCREEN_WIDTH / 2 - 18, MY - 4 + line, SCREEN_WIDTH / 2, MY + 4 + line, userFaceColor);
      tft.drawLine(SCREEN_WIDTH / 2, MY + 4 + line, SCREEN_WIDTH / 2 + 18, MY - 4 + line, userFaceColor);
    }
    float wobble = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int tongueH = 14 + (int)(wobble * 5.0f);
    tft.fillRoundRect(SCREEN_WIDTH / 2 - 11, MY + 5, 22, tongueH, 7, userFaceColor);
    tft.fillCircle(SCREEN_WIDTH / 2, MY + 5 + tongueH - 5, 4, COL_BG);
  } else {
    float drift = sin(t * 3.14159f * 2.0f);
    int px = (int)(drift * 4.0f);
    int py = (int)(cos(t * 3.14159f * 3.0f) * 2.0f);
    int eyeH = EH;
    if (ph >= 28 && ph <= 30) {
      float blinkT = ph == 29 ? 1.0f : 0.5f;
      eyeH = EH - (int)(blinkT * (EH - 5));
    }
    drawEye(LX, EY, EW, eyeH, ER, px, py);
    drawEye(RX, EY, EW, eyeH, ER, px, py);
    for (int line = 0; line < 4; line++) {
      tft.drawLine(SCREEN_WIDTH / 2 - 13, MY + line, SCREEN_WIDTH / 2 + 13, MY + line, userFaceColor);
    }
  }

  drawCompanionAccessories(LX, RX, EY, MY);
}

void renderIdle() {
  if (!displayAvailable) return;

  // ── Clock/header strip (rows 0..FACE_OFFSET_Y-1) ─────────────────────────
  // Only clear and redraw this strip when the minute changes, so the clock
  // never flashes black on every 500 ms tick.
  static int  s_idleLastMin  = -1;
  static unsigned long s_idleLastCallMs = 0;
  const  unsigned long nowMs = millis();
  // If we haven't called renderIdle recently we're re-entering from another
  // mode; the clock area may have been painted over — force a refresh.
  if (nowMs - s_idleLastCallMs > 1500UL) s_idleLastMin = -1;
  s_idleLastCallMs = nowMs;
  {
    struct tm timeinfo;
    bool timeOk = getLocalTime(&timeinfo, 10) && timeinfo.tm_year > 100;
    int  curMin = timeOk ? (int)timeinfo.tm_min : -1;
    if (curMin != s_idleLastMin) {
      s_idleLastMin = curMin;
      tft.fillRect(0, 0, SCREEN_WIDTH, FACE_OFFSET_Y, COL_BG);
      if (timeOk) {
        char tBuf[8], dBuf[14];
        strftime(tBuf, sizeof(tBuf), "%H:%M", &timeinfo);
        strftime(dBuf, sizeof(dBuf), "%a %b %d", &timeinfo);
        tft.setTextSize(2);
        tft.setTextColor(userAccentColor);
        tft.setCursor((SCREEN_WIDTH - (int)strlen(tBuf) * 12) / 2, 4);
        tft.print(tBuf);
        tft.setTextSize(1);
        tft.setTextColor(userFaceColor);
        tft.setCursor((SCREEN_WIDTH - (int)strlen(dBuf) * 6) / 2, 24);
        tft.print(dBuf);
      }
      drawWeatherBadge(280, 15);
    }
  }

  // ── Face box: only clear the interior each tick (border stays on screen) ──
  tft.fillRect(1, FACE_OFFSET_Y + 1, SCREEN_WIDTH - 2, 178, COL_BG);
  tft.drawRoundRect(0, FACE_OFFSET_Y, SCREEN_WIDTH, 180, 28, userFaceColor);

  const int leftX = 70;
  const int rightX = 170;
  const int eyeY = FACE_OFFSET_Y + 60 + clampAppearanceOffset(companionEyeOffsetY) * 2;
  const int mouthY = FACE_OFFSET_Y + 110 + clampAppearanceOffset(companionMouthOffsetY) * 2;
  const int pupilOffsets[4] = {-4, 0, 4, 0};
  const int pupilDx = pupilOffsets[idleOrbit % 4];

  if (activePetMode == "off") {
    drawEye(leftX, eyeY, 42, 26, 9, 0, 0);
    drawEye(rightX, eyeY, 42, 26, 9, 0, 0);
    tft.drawLine(SCREEN_WIDTH / 2 - 13, mouthY, SCREEN_WIDTH / 2 + 13, mouthY, userFaceColor);
  } else if (activePetMode == "nap" || petPersonality == "sleepy") {
    drawBlinkEye(leftX, eyeY, 44, 7, 7);
    drawBlinkEye(rightX, eyeY, 44, 7, 7);
    tft.drawLine(SCREEN_WIDTH / 2 - 14, mouthY, SCREEN_WIDTH / 2 + 14, mouthY, userFaceColor);
    tft.setTextSize(2);
    tft.setTextColor(userAccentColor);
    tft.setCursor(190, FACE_OFFSET_Y + 44);
    tft.print("z");
    tft.setCursor(204, FACE_OFFSET_Y + 34);
    tft.print("z");
  } else if (activePetMode == "cuddle" || petPersonality == "cuddly") {
    drawEye(leftX, eyeY, 44, 30, 9, pupilDx / 2, 0);
    drawEye(rightX, eyeY, 44, 30, 9, pupilDx / 2, 0);
    drawSmile(SCREEN_WIDTH / 2, mouthY - 2, 44);
    drawIconHeart(120, FACE_OFFSET_Y + 145, 5);
  } else if (activePetMode == "play" || petPersonality == "playful") {
    drawEye(leftX, eyeY, 48, 34, 9, pupilDx * 2, 0);
    drawEye(rightX, eyeY, 48, 34, 9, pupilDx * 2, 0);
    drawSmile(SCREEN_WIDTH / 2, mouthY - 4, 48);
    tft.fillCircle(26 + (idleOrbit % 4) * 4, FACE_OFFSET_Y + 130 - (idleOrbit % 2) * 4, 4, userAccentColor);
  } else if (activePetMode == "party") {
    drawEye(leftX, eyeY, 48, 34, 9, pupilDx * 2, 0);
    drawEye(rightX, eyeY, 48, 34, 9, -pupilDx * 2, 0);
    drawSmile(SCREEN_WIDTH / 2, mouthY - 4, 52);
    drawIconStar(30, FACE_OFFSET_Y + 26 + (idleOrbit % 2) * 4, 5);
    drawIconStar(210, FACE_OFFSET_Y + 30 + ((idleOrbit + 1) % 2) * 4, 5);
  } else {
    drawEye(leftX, eyeY, 44, 30, 9, pupilDx * 2, 2);
    drawEye(rightX, eyeY, 44, 30, 9, -pupilDx, -2);
    drawOvalMouth(SCREEN_WIDTH / 2, mouthY, 9, 7);
  }

  drawCompanionAccessories(leftX, rightX, eyeY, mouthY);

  // Status bar at bottom of screen (size-1 font fits within the 18px gap)
  tft.fillRect(0, SCREEN_HEIGHT - 18, SCREEN_WIDTH, 18, COL_BG);
  drawStatusBar();
}

void renderCurrentMode() {
  if (!displayAvailable) return;
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
    case MODE_SCENE:
      renderSceneFrame();
      break;
    case MODE_FIREWORKS:
      renderFireworksFrame();
      break;
    case MODE_HEART_RAIN:
      renderHeartRainFrame();
      break;
    case MODE_SNOWFALL:
      renderSnowfallFrame();
      break;
    case MODE_STARFIELD:
      renderStarfieldFrame();
      break;
    case MODE_COUNTDOWN:
      renderCountdownFrame();
      break;
    case MODE_WEATHER:
      renderWeatherFrame();
      break;
    case MODE_SLEEP:
      renderSleepFrame();
      break;
    case MODE_IDLE:
    default:
      renderIdle();
      break;
  }
}

// ─── Stick figure scene helpers ───

void drawStickFigure(int cx, int cy, int sc,
                     float armLA, float armRA,
                     float legLA, float legRA,
                     uint16_t color) {
  // Head
  tft.drawCircle(cx, cy - sc * 3, sc, color);
  // Body (neck → hips)
  tft.drawLine(cx, cy - sc * 2, cx, cy + sc * 2, color);
  // Left arm (armLA = angle below left-horizontal; 0=straight out, PI/4=45° down)
  int sY = cy - sc;
  tft.drawLine(cx, sY,
               cx - (int)(sc * 3.f * cosf(armLA)),
               sY + (int)(sc * 3.f * sinf(armLA)), color);
  // Right arm (armRA = angle below right-horizontal)
  tft.drawLine(cx, sY,
               cx + (int)(sc * 3.f * cosf(armRA)),
               sY + (int)(sc * 3.f * sinf(armRA)), color);
  // Left leg (legLA = spread angle from straight down)
  int hY = cy + sc * 2;
  tft.drawLine(cx, hY,
               cx - (int)(sc * 3.f * sinf(legLA)),
               hY + (int)(sc * 3.f * cosf(legLA)), color);
  // Right leg (legRA = spread angle from straight down)
  tft.drawLine(cx, hY,
               cx + (int)(sc * 3.f * sinf(legRA)),
               hY + (int)(sc * 3.f * cosf(legRA)), color);
}

void renderSceneFrame() {
  if (!displayAvailable) return;
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 18, COL_BG);

  float t    = (float)expressionPhase / 63.f;
  float bob  = sinf(t * 3.14159f * 2.f) * 2.f;

  if (currentScene == "wave") {
    drawStickFigure(90, 120, 14, 0.3f, 0.3f, 0.3f, 0.3f, userFaceColor);
    float waveA = 0.2f + sinf(t * 3.14159f * 4.f) * 0.5f;
    drawStickFigure(220, 120 + (int)bob, 14, 0.3f, -waveA, 0.3f, 0.3f, userBodyColor);
    int hx = 160, hy = 70 - (int)(t * 18.f);
    tft.fillCircle(hx - 4, hy - 2, 4, COL_ROSE);
    tft.fillCircle(hx + 4, hy - 2, 4, COL_ROSE);
    tft.fillTriangle(hx - 8, hy, hx + 8, hy, hx, hy + 8, COL_ROSE);

  } else if (currentScene == "bow") {
    drawStickFigure(130, 120, 14, 0.5f, 0.5f, 0.25f, 0.25f, userFaceColor);
    float bowY = sinf(t * 3.14159f) * 12.f;
    drawStickFigure(205, 130 - (int)bowY, 14,
                   0.25f + bowY * 0.04f, 0.25f + bowY * 0.04f,
                   0.25f, 0.25f, userBodyColor);

  } else if (currentScene == "hug") {
    float hugP = 0.2f + sinf(t * 3.14159f * 2.f) * 0.15f;
    drawStickFigure(140, 120, 14, 0.3f, -hugP, 0.25f, 0.25f, userFaceColor);
    drawStickFigure(185, 120, 14,  hugP, 0.3f, 0.25f, 0.25f, userBodyColor);

  } else if (currentScene == "holdHands") {
    float handBob = sinf(t * 3.14159f * 2.f) * 3.f;
    drawStickFigure(100, 125 + (int)handBob, 14, 0.4f, 0.0f, 0.3f, 0.3f, userFaceColor);
    drawStickFigure(220, 125 + (int)handBob, 14, 0.0f, 0.4f, 0.3f, 0.3f, userBodyColor);
    tft.fillCircle(160, 110, 5, COL_GOLD);

  } else if (currentScene == "kiss") {
    float lean = sinf(t * 3.14159f) * 8.f;
    drawStickFigure(130 + (int)lean, 120, 14, 0.5f, -0.2f, 0.25f, 0.25f, userFaceColor);
    drawStickFigure(190 - (int)lean, 120, 14,  0.2f,  0.5f, 0.25f, 0.25f, userBodyColor);
    if (lean > 5.f) {
      for (int i = 0; i < 3; i++) {
        int hx2 = 160 + (i - 1) * 14;
        int hy2 = 76 - (int)(t * 15.f);
        tft.fillCircle(hx2 - 3, hy2,   3, COL_ROSE);
        tft.fillCircle(hx2 + 3, hy2,   3, COL_ROSE);
        tft.fillTriangle(hx2 - 6, hy2 + 1, hx2 + 6, hy2 + 1, hx2, hy2 + 7, COL_ROSE);
      }
    }
  } else { // shyLeanIn (default)
    float lean2 = sinf(t * 3.14159f * 1.5f) * 5.f;
    drawStickFigure(105, 125 - (int)lean2, 14, 0.6f, 0.1f, 0.3f, 0.3f, userFaceColor);
    drawStickFigure(215, 125,              14, 0.3f, 0.7f, 0.3f, 0.3f, userBodyColor);
    tft.fillCircle(160, 95, 4, COL_PINK);
    tft.fillCircle(155, 85, 3, COL_PINK);
    tft.fillCircle(165, 80, 2, COL_PINK);
  }
}

void setScene(const String& name) {
  currentScene         = name;
  tft.fillScreen(COL_BG);
  currentMode          = MODE_SCENE;
  expressionPhase      = 0;
  lastExpressionTickMs = 0;
}

// ─── Particle mode helpers ───

static const uint16_t BURST_COLORS[] = {
  COL_ROSE, COL_GOLD, COL_SKYBLUE, COL_MINT, COL_PINK, COL_LAVENDER, COL_PEACH, COL_ACCENT
};

void initFireworks() {
  int cx = 40 + (int)random(241);
  int cy = 20 + (int)random(181);
  uint16_t col = BURST_COLORS[(uint8_t)random(8)];
  gPtclCount = 16;
  for (uint8_t i = 0; i < 16; i++) {
    float a = i * 3.14159f * 2.f / 16.f;
    int spd = 3 + (int)random(4);
    gPtcl[i].x     = (int16_t)cx;
    gPtcl[i].y     = (int16_t)cy;
    gPtcl[i].vx    = (int8_t)((float)spd * cosf(a));
    gPtcl[i].vy    = (int8_t)((float)spd * sinf(a));
    gPtcl[i].life  = 30;
    gPtcl[i].color = col;
  }
}

void renderFireworksFrame() {
  if (!displayAvailable) return;
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 18, COL_BG);
  bool anyAlive = false;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    if (gPtcl[i].life == 0) continue;
    anyAlive = true;
    gPtcl[i].x   += gPtcl[i].vx;
    gPtcl[i].y   += gPtcl[i].vy;
    gPtcl[i].vy   = (int8_t)(gPtcl[i].vy + 1);  // gravity
    gPtcl[i].life--;
    int16_t nx = gPtcl[i].x, ny = gPtcl[i].y;
    if (nx >= 0 && nx < SCREEN_WIDTH && ny >= 0 && ny < SCREEN_HEIGHT - 18) {
      int r = (gPtcl[i].life > 20) ? 3 : (gPtcl[i].life > 10) ? 2 : 1;
      tft.fillCircle(nx, ny, r, gPtcl[i].color);
    }
  }
  if (!anyAlive) initFireworks();
}

void initHeartRain() {
  gPtclCount = 18;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    gPtcl[i].y     = (int16_t)random(SCREEN_HEIGHT - 18);
    gPtcl[i].vx    = 0;
    gPtcl[i].vy    = (int8_t)(1 + random(3));
    gPtcl[i].life  = (uint8_t)(1 + random(3));  // heart size
    gPtcl[i].color = (i % 2 == 0) ? COL_ROSE : COL_PINK;
  }
}

void renderHeartRainFrame() {
  if (!displayAvailable) return;
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 18, COL_BG);
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].y += gPtcl[i].vy;
    if (gPtcl[i].y > SCREEN_HEIGHT - 18) {
      gPtcl[i].y = -10;
      gPtcl[i].x = (int16_t)random(SCREEN_WIDTH);
    }
    int16_t x = gPtcl[i].x, y = gPtcl[i].y;
    int s = gPtcl[i].life;
    uint16_t c = gPtcl[i].color;
    tft.fillCircle(x - s, y - s / 2, s, c);
    tft.fillCircle(x + s, y - s / 2, s, c);
    tft.fillTriangle(x - s * 2, y - s / 2 + 1, x + s * 2, y - s / 2 + 1, x, y + s * 2, c);
  }
}

void initSnowfall() {
  gPtclCount = 28;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    gPtcl[i].y     = (int16_t)random(SCREEN_HEIGHT - 18);
    gPtcl[i].vx    = (int8_t)(random(3) - 1);
    gPtcl[i].vy    = (int8_t)(1 + random(2));
    gPtcl[i].life  = (uint8_t)(1 + random(3));  // snowflake radius
    gPtcl[i].color = userFaceColor;
  }
}

void renderSnowfallFrame() {
  if (!displayAvailable) return;
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 18, COL_BG);
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x += gPtcl[i].vx;
    gPtcl[i].y += gPtcl[i].vy;
    if (gPtcl[i].y >= SCREEN_HEIGHT - 18) { gPtcl[i].y = 0; gPtcl[i].x = (int16_t)random(SCREEN_WIDTH); }
    if (gPtcl[i].x < 0)             gPtcl[i].x = SCREEN_WIDTH - 1;
    if (gPtcl[i].x >= SCREEN_WIDTH) gPtcl[i].x = 0;
    tft.fillCircle(gPtcl[i].x, gPtcl[i].y, gPtcl[i].life, gPtcl[i].color);
  }
}

void initStarfield() {
  gPtclCount = 36;
  const int CX = SCREEN_WIDTH / 2, CY = (SCREEN_HEIGHT - 18) / 2;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x     = (int16_t)(CX + random(41) - 20);
    gPtcl[i].y     = (int16_t)(CY + random(41) - 20);
    gPtcl[i].vx    = 0;
    gPtcl[i].vy    = 0;
    gPtcl[i].life  = (uint8_t)(4 + random(60));  // Z depth: large=far, small=near
    gPtcl[i].color = userFaceColor;
  }
}

void renderStarfieldFrame() {
  if (!displayAvailable) return;
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 18, COL_BG);
  const int CX = SCREEN_WIDTH / 2, CY = (SCREEN_HEIGHT - 18) / 2;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    float z = gPtcl[i].life / 64.f + 0.01f;
    int sx = CX + (int)((gPtcl[i].x - CX) / z);
    int sy = CY + (int)((gPtcl[i].y - CY) / z);
    gPtcl[i].life--;
    if (gPtcl[i].life == 0 || sx < 0 || sx >= SCREEN_WIDTH || sy < 0 || sy >= SCREEN_HEIGHT - 18) {
      gPtcl[i].x    = (int16_t)(CX + random(41) - 20);
      gPtcl[i].y    = (int16_t)(CY + random(41) - 20);
      gPtcl[i].life = (uint8_t)(50 + random(14));
    } else {
      int r = (gPtcl[i].life < 20) ? 2 : 1;
      uint16_t c = (gPtcl[i].life < 20) ? userAccentColor : userFaceColor;
      tft.fillCircle(sx, sy, r, c);
    }
  }
}

void setParticleMode(const String& name) {
  lastParticleTickMs = 0;
  expressionPhase    = 0;
  tft.fillScreen(COL_BG);
  if      (name == "fireworks")  { currentMode = MODE_FIREWORKS;  initFireworks();  }
  else if (name == "heart_rain") { currentMode = MODE_HEART_RAIN; initHeartRain(); }
  else if (name == "snowfall")   { currentMode = MODE_SNOWFALL;   initSnowfall();  }
  else                           { currentMode = MODE_STARFIELD;  initStarfield(); }
}

// ─── Countdown mode ───

void renderCountdownFrame() {
  if (!displayAvailable) return;
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 18, COL_BG);
  long elapsed   = (long)((millis() - countdownStartMs) / 1000UL);
  long remaining = countdownSeconds - elapsed;
  if (remaining < 0) remaining = 0;
  long h = remaining / 3600;
  long m = (remaining % 3600) / 60;
  long s = remaining % 60;
  char buf[12];
  if (h > 0) snprintf(buf, sizeof(buf), "%02ld:%02ld:%02ld", h, m, s);
  else       snprintf(buf, sizeof(buf), "%02ld:%02ld", m, s);
  // Label
  tft.setTextSize(1);
  tft.setTextColor(userFaceColor);
  tft.setCursor((SCREEN_WIDTH - 9 * 6) / 2, 55);
  tft.print("COUNTDOWN");
  // Large digits
  tft.setTextSize(4);
  tft.setTextColor(COL_GOLD);
  tft.setCursor((SCREEN_WIDTH - (int)strlen(buf) * 24) / 2, 75);
  tft.print(buf);
  // Progress bar
  int barTotal = SCREEN_WIDTH - 40;
  int safeSecs = (countdownSeconds > 0) ? (int)countdownSeconds : 1;
  int barFill  = (int)((long)barTotal * remaining / safeSecs);
  tft.fillRect(20, 148, barTotal, 8, COL_BG);
  tft.fillRect(20, 148, barFill,  8, COL_ACCENT);
  tft.drawRect(20, 148, barTotal, 8, userFaceColor);
  // At zero: trigger fireworks celebration
  if (remaining == 0 && !countdownExpired) {
    countdownExpired = true;
    setParticleMode("fireworks");
    startTransientExpression("love", 4000, "Time's up!");
  }
}

void setCountdown(long seconds) {
  countdownSeconds   = seconds;
  countdownStartMs   = millis();
  countdownExpired   = false;
  currentMode        = MODE_COUNTDOWN;
  lastParticleTickMs = 0;
}

// ─── NTP clock ───

void syncNtp() {
  configTime(ntpUtcOffsetSeconds, 0, "pool.ntp.org", "time.google.com");
}

// ─── Weather widget (OpenMeteo) ───

static int weatherCodeCategory(int code) {
  if (code == 0)  return 0; // clear sky
  if (code <= 3)  return 1; // partly / overcast
  if (code <= 48) return 1; // fog  → show as cloud
  if (code <= 67) return 2; // drizzle / rain
  if (code <= 77) return 3; // snow
  if (code <= 82) return 2; // rain showers
  if (code <= 86) return 3; // snow showers
  return 4;                 // thunderstorm
}

static uint16_t weatherCategoryColor(int cat) {
  switch (cat) {
    case 0: return COL_GOLD;
    case 1: return userFaceColor;
    case 2: return COL_SKYBLUE;
    case 3: return userFaceColor;
    case 4: return COL_LAVENDER;
    default: return userFaceColor;
  }
}

void drawWeatherIcon(int cx, int cy, int r, int cat) {
  if (cat == 0) {  // Sun
    tft.fillCircle(cx, cy, r, COL_GOLD);
    for (int i = 0; i < 8; i++) {
      float a = i * 3.14159f / 4.f;
      tft.drawLine(cx + (int)((r+2)*cosf(a)), cy + (int)((r+2)*sinf(a)),
                   cx + (int)((r+r/2)*cosf(a)), cy + (int)((r+r/2)*sinf(a)), COL_GOLD);
    }
  } else if (cat == 1) {  // Cloud
    tft.fillCircle(cx - r/3, cy, r*2/3, userFaceColor);
    tft.fillCircle(cx + r/3, cy, r*2/3, userFaceColor);
    tft.fillCircle(cx, cy - r/3, r/2, userFaceColor);
    tft.fillRect(cx - r*2/3, cy, r*4/3, r/2+1, userFaceColor);
  } else if (cat == 2) {  // Rain
    tft.fillCircle(cx - r/4, cy - r/4, r*2/3, COL_SKYBLUE);
    tft.fillCircle(cx + r/4, cy - r/4, r*2/3, COL_SKYBLUE);
    tft.fillRect(cx - r*2/3, cy - r/4, r*4/3, r/3+1, COL_SKYBLUE);
    for (int d = 0; d < 3; d++)
      tft.drawLine(cx - r/3 + d*r/3, cy + r/4, cx - r/3 + d*r/3 - 2, cy + r, COL_SKYBLUE);
  } else if (cat == 3) {  // Snow
    tft.fillCircle(cx - r/4, cy - r/4, r*2/3, userFaceColor);
    tft.fillCircle(cx + r/4, cy - r/4, r*2/3, userFaceColor);
    tft.fillRect(cx - r*2/3, cy - r/4, r*4/3, r/3+1, userFaceColor);
    for (int d = 0; d < 3; d++)
      tft.fillCircle(cx - r/3 + d*r/3, cy + r/2, 2, userFaceColor);
  } else {  // Storm
    tft.fillCircle(cx - r/4, cy - r/4, r*2/3, COL_LAVENDER);
    tft.fillCircle(cx + r/4, cy - r/4, r*2/3, COL_LAVENDER);
    tft.fillRect(cx - r*2/3, cy - r/4, r*4/3, r/3+1, COL_LAVENDER);
    tft.fillTriangle(cx, cy + r/4, cx - r/4, cy + 3*r/4, cx + r/8, cy + r/2, COL_GOLD);
    tft.fillTriangle(cx + r/8, cy + r/2, cx - r/8, cy + 3*r/4, cx + r/4, cy + 3*r/4, COL_GOLD);
  }
}

void drawWeatherBadge(int x, int y) {
  if (weatherCode < 0) return;
  drawWeatherIcon(x, y, 8, weatherCodeCategory(weatherCode));
}

void renderWeatherFrame() {
  if (!displayAvailable) return;
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 18, COL_BG);
  if (weatherCode < 0) {
    tft.setTextSize(2);
    tft.setTextColor(userFaceColor);
    tft.setCursor(40, 95);
    tft.print("No weather data");
    tft.setTextSize(1);
    tft.setTextColor(COL_ACCENT);
    tft.setCursor(55, 122);
    tft.print("Send set_location first");
    return;
  }
  int cat = weatherCodeCategory(weatherCode);
  drawWeatherIcon(SCREEN_WIDTH / 2, 68, 28, cat);
  char tempBuf[12];
  int tWhole = weatherTempTenths / 10;
  int tFrac  = abs(weatherTempTenths) % 10;
  snprintf(tempBuf, sizeof(tempBuf), "%d.%d C", tWhole, tFrac);
  tft.setTextSize(3);
  tft.setTextColor(weatherCategoryColor(cat));
  tft.setCursor((SCREEN_WIDTH - (int)strlen(tempBuf) * 18) / 2, 115);
  tft.print(tempBuf);
  const char* labels[] = { "Clear", "Cloudy", "Rain", "Snow", "Storm" };
  tft.setTextSize(2);
  tft.setTextColor(userFaceColor);
  tft.setCursor((SCREEN_WIDTH - (int)strlen(labels[cat]) * 12) / 2, 158);
  tft.print(labels[cat]);
}

void fetchWeather() {
  if (weatherLat == 0.f && weatherLon == 0.f) return;
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClient wcl;
  if (!wcl.connect("api.open-meteo.com", 80)) return;
  String path = "/v1/forecast?latitude=";
  path += String(weatherLat, 4);
  path += "&longitude=";
  path += String(weatherLon, 4);
  path += "&current=temperature_2m,weathercode";
  wcl.print(String("GET ") + path + " HTTP/1.0\r\nHost: api.open-meteo.com\r\nConnection: close\r\n\r\n");
  unsigned long t0 = millis();
  bool inBody = false;
  char hdr[4] = {0,0,0,0};
  String body;
  body.reserve(400);
  while (millis() - t0 < 8000) {
    while (wcl.available()) {
      char c = wcl.read();
      if (!inBody) {
        hdr[0]=hdr[1]; hdr[1]=hdr[2]; hdr[2]=hdr[3]; hdr[3]=c;
        if (hdr[0]=='\r' && hdr[1]=='\n' && hdr[2]=='\r' && hdr[3]=='\n') inBody = true;
      } else if (body.length() < 480) {
        body += c;
      }
    }
    if (!wcl.connected() && !wcl.available()) break;
    delay(5);
  }
  wcl.stop();
  if (body.length() > 10) {
    float tempF = extractJsonFloatField(body, "temperature_2m", -999.f);
    if (tempF > -998.f) {
      weatherTempTenths = (int)(tempF * 10.f + (tempF >= 0.f ? 0.5f : -0.5f));
      weatherCode = extractJsonIntField(body, "weathercode", -1);
    }
  }
  lastWeatherFetchMs = millis();
}

// ─── Daily greetings + spontaneous reactions ───

void drawStatusBar() {
  if (!displayAvailable) return;
  bool wifiOk  = (WiFi.status() == WL_CONNECTED);
  bool relayOk = (lastRelaySuccessMs > 0 && millis() - lastRelaySuccessMs < 120000UL);
  tft.setTextSize(1);
  // WiFi dot
  tft.fillCircle(SCREEN_WIDTH - 28, STATUS_BAR_Y + 3, 5,
                 wifiOk ? ST77XX_GREEN : COL_ROSE);
  // Relay dot
  tft.fillCircle(SCREEN_WIDTH - 18, STATUS_BAR_Y + 3, 5,
                 relayOk ? ST77XX_GREEN : (!relayUrl.isEmpty() ? COL_ROSE : COL_FG));
}

void checkTimeGreetings() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastTimeCheckMs < 60000UL) return;
  lastTimeCheckMs = millis();
  struct tm ti;
  if (!getLocalTime(&ti, 10)) return;
  const int hour = ti.tm_hour, yday = ti.tm_yday;
  // Morning greeting (6–9 am)
  if (hour >= 6 && hour < 10 && lastGreetingDay != yday) {
    if (currentMode == MODE_IDLE && !transientActive) {
      lastGreetingDay = yday;
      bondLevel    = clampLevel(bondLevel + 5);
      boredomLevel = clampLevel(boredomLevel - 10);
      persistPetState();
      startTransientExpression("love", 4500, "Good morning!");
    }
  }
  // Evening goodnight (21–23)
  if (hour >= 21 && lastEveningDay != yday) {
    if (currentMode == MODE_IDLE && !transientActive) {
      lastEveningDay = yday;
      setSleepMode();
    }
  }
  // Miss you: idle >25 min with no interaction
  if (currentMode == MODE_IDLE && !transientActive &&
      lastIdleInteractionMs > 0 &&
      millis() - lastIdleInteractionMs > 25UL * 60UL * 1000UL) {
    lastIdleInteractionMs = millis();
    startTransientExpression("sad", 2200, "Miss you!");
  }
}

// ─── Emoji-reactive note messages (Phase 10) ───

void detectEmojiReaction(const String& text) {
  // Scan the note text for known emoji-like tokens and react
  struct { const char* token; const char* expr; unsigned long ms; } rules[] = {
    { "<3",  "love",     3000 },
    { ":)",  "happy",    2200 },
    { ":D",  "laugh",    2200 },
    { ":(",  "sad",      2200 },
    { ":*",  "kiss",     2200 },
    { ";)",  "wink",     2200 },
    { ":o",  "surprised",2200 },
    { ":O",  "surprised",2200 },
    { "xD",  "laugh",    2200 },
    { "<*>", "star_eyes",2500 },
    { "zzz", "sleepy",   3000 },
    { "ZZZ", "sleepy",   3000 },
    { "!",   "excited",  1800 },
  };
  for (size_t i = 0; i < sizeof(rules)/sizeof(rules[0]); i++) {
    if (text.indexOf(rules[i].token) >= 0) {
      lastNoteEmoji = rules[i].expr;
      emojiReactionEndsMs = millis() + rules[i].ms;
      startTransientExpression(rules[i].expr, rules[i].ms, "Reacting!");
      return;
    }
  }
}

// ─── Sleep / goodnight ambient scene (Phase 11) ───

void renderSleepFrame() {
  if (!displayAvailable) return;
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 18, COL_BG);
  const float t = (float)(expressionPhase % 64) / 63.f;
  // Gentle pulsing moon
  const int MCX = 125, MCY = 90;
  float pulse = 0.8f + sinf(t * 3.14159f * 2.f) * 0.2f;
  int mr = (int)(32.f * pulse);
  tft.fillCircle(MCX, MCY, mr, COL_GOLD);
  tft.fillCircle(MCX + mr / 3, MCY - mr / 4, mr - 4, COL_BG);  // crescent shadow
  // Stars
  static const uint16_t STAR_X[] = { 18, 52, 230, 290, 16, 270, 195, 60 };
  static const uint8_t  STAR_Y[] = { 14, 36,  18,  42, 62,  72,  50, 72 };
  for (int i = 0; i < 8; i++) {
    float phase = t * 3.14159f * 2.f + i * 0.78f;
    uint16_t c = (sinf(phase) > 0.f) ? COL_GOLD : userAccentColor;
    tft.fillCircle(STAR_X[i], STAR_Y[i], 2, c);
  }
  // Zzz floating up
  int zDrift = (int)(t * 40.f);
  tft.setTextSize(2);
  tft.setTextColor(COL_LAVENDER);
  tft.setCursor(190, 120 - zDrift);
  tft.print("z");
  tft.setTextSize(3);
  tft.setCursor(208, 95 - zDrift);
  tft.print("Z");
  // Calm message
  tft.setTextSize(1);
  tft.setTextColor(userFaceColor);
  tft.setCursor((SCREEN_WIDTH - 10*6) / 2, 162);
  tft.print("Sweet dreams");
  drawStatusBar();
}

void setSleepMode() {
  tft.fillScreen(COL_BG);
  currentMode     = MODE_SLEEP;
  expressionPhase = 0;
  lastExpressionTickMs = 0;
  statusText = "Good night";
  renderCurrentMode();
  publishStatus();
}

// ─── Flower drawing helpers (scaled 2×) ───

void drawFlowerRose(int cx, int cy, int scale, int phase) {
  float spiralT = (float)phase / 63.f;
  float spiralOff = spiralT * 0.4f;
  int outerR   = 15 * scale / 8;
  int outerDist = 30 * scale / 8;
  int midR     = 11 * scale / 8;
  int midDist  = 17 * scale / 8;
  int centerR  = 7  * scale / 8;
  if (outerR    < 3) outerR    = 3;
  if (outerDist < 7) outerDist = 7;
  if (midR      < 3) midR      = 3;
  if (midDist   < 5) midDist   = 5;
  if (centerR   < 2) centerR   = 2;

  for (int i = 0; i < 5; i++) {
    float a = i * 3.14159f * 2.f / 5.f + spiralOff;
    int px = cx + (int)(outerDist * cosf(a));
    int py = cy + (int)(outerDist * sinf(a));
    tft.fillCircle(px, py, outerR, COL_ROSE);
  }
  for (int i = 0; i < 5; i++) {
    float a = i * 3.14159f * 2.f / 5.f + spiralOff + 0.314f;
    int px = cx + (int)(midDist * cosf(a));
    int py = cy + (int)(midDist * sinf(a));
    tft.fillCircle(px, py, midR, COL_PINK);
  }
  tft.fillCircle(cx, cy, centerR + 2, COL_GOLD);
  tft.fillCircle(cx, cy, centerR - 1, COL_BG);
}

void drawFlowerSunflower(int cx, int cy, int scale, int phase) {
  float t = (float)phase / 63.f;
  int petalLen = 34 * scale / 8;
  int petalW   = 7  * scale / 8;
  int centerR  = 22 * scale / 8;
  if (petalLen < 9)  petalLen = 9;
  if (petalW   < 2)  petalW   = 2;
  if (centerR  < 5)  centerR  = 5;

  for (int i = 0; i < 16; i++) {
    float a = i * 3.14159f * 2.f / 16.f + t * 0.2f;
    int tip_x = cx + (int)((centerR + petalLen) * cosf(a));
    int tip_y = cy + (int)((centerR + petalLen) * sinf(a));
    int base_x = cx + (int)(centerR * cosf(a));
    int base_y = cy + (int)(centerR * sinf(a));
    for (int w = -petalW/2; w <= petalW/2; w++) {
      float perp_a = a + 3.14159f / 2.f;
      int dx = (int)(w * cosf(perp_a));
      int dy = (int)(w * sinf(perp_a));
      tft.drawLine(base_x + dx, base_y + dy, tip_x + dx, tip_y + dy, COL_GOLD);
    }
  }
  tft.fillCircle(cx, cy, centerR, userFaceColor);
  int numSeeds = 21 + (int)(t * 11.f);
  for (int i = 0; i < numSeeds; i++) {
    float angle = i * 2.399f + t * 0.5f;
    float r     = (centerR - 4) * sqrtf((float)i / 34.f);
    int sx = cx + (int)(r * cosf(angle));
    int sy = cy + (int)(r * sinf(angle));
    if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT) {
      tft.fillCircle(sx, sy, 2, COL_BG);
    }
  }
}

void drawFlowerKingProtea(int cx, int cy, int scale, int phase) {
  float t = (float)phase / 63.f;
  float growT = t < 0.1f ? t * 10.f : 1.0f;
  int bractLen = (int)(38 * scale / 8 * growT);
  int bractW   = 5  * scale / 8;
  int centerR  = 26 * scale / 8;
  if (bractLen < 5)  bractLen = 5;
  if (bractW   < 2)  bractW   = 2;
  if (centerR  < 7)  centerR  = 7;

  for (int i = 0; i < 20; i++) {
    float a = i * 3.14159f * 2.f / 20.f + t * 0.1f;
    int base_x = cx + (int)(centerR * cosf(a));
    int base_y = cy + (int)(centerR * sinf(a));
    int tip_x  = cx + (int)((centerR + bractLen) * cosf(a));
    int tip_y  = cy + (int)((centerR + bractLen) * sinf(a));
    float side_a = a + 3.14159f / 2.f;
    int sx = (int)(bractW * cosf(side_a));
    int sy = (int)(bractW * sinf(side_a));
    tft.fillTriangle(base_x + sx, base_y + sy, base_x - sx, base_y - sy, tip_x, tip_y, COL_PEACH);
  }
  tft.fillCircle(cx, cy, centerR, userFaceColor);
  tft.fillCircle(cx, cy, centerR - 5, COL_BG);
  tft.fillCircle(cx, cy, centerR - 11, userFaceColor);
  tft.fillCircle(cx, cy, 4, COL_BG);
}

void renderFlowerFrame() {
  if (!displayAvailable) return;
  tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 18, COL_BG);
  int cx = 120, cy = 100;
  // Stem
  tft.drawLine(cx,     cy + 42, cx,     SCREEN_HEIGHT - 10, userFaceColor);
  tft.drawLine(cx + 1, cy + 42, cx + 1, SCREEN_HEIGHT - 10, userFaceColor);
  tft.drawLine(cx + 2, cy + 42, cx + 2, SCREEN_HEIGHT - 10, userFaceColor);
  // Leaf
  for (int i = 0; i < 14; i++) {
    tft.drawLine(cx + 2, cy + 80 + i, cx + 2 + 14 - i, cy + 80 + i, userFaceColor);
  }

  if (currentFlower == "sunflower") {
    drawFlowerSunflower(cx, cy, 8, expressionPhase);
  } else if (currentFlower == "king_protea") {
    drawFlowerKingProtea(cx, cy, 8, expressionPhase);
  } else {
    drawFlowerRose(cx, cy, 8, expressionPhase);
  }
}

void drawNoteWithFlowerAccent(const String& text, int fontSize, int border, const String& icons, const String& flowerType) {
  if (!displayAvailable) return;
  tft.fillScreen(COL_BG);

  const int flowerCX = 60;
  const int flowerCY = 100;
  const int flowerScale = 6;
  if (flowerType == "sunflower") {
    drawFlowerSunflower(flowerCX, flowerCY, flowerScale, expressionPhase);
  } else if (flowerType == "king_protea") {
    drawFlowerKingProtea(flowerCX, flowerCY, flowerScale, expressionPhase);
  } else {
    drawFlowerRose(flowerCX, flowerCY, flowerScale, expressionPhase);
  }

  // Vertical divider
  tft.drawLine(125, 6, 125, SCREEN_HEIGHT - 6, userFaceColor);

  // Text on the right side
  const int safeFontSize = 2;
  const int textLeft = 132;
  const int textAreaW = SCREEN_WIDTH - textLeft - 6;
  const int maxChars = textAreaW / (6 * safeFontSize);
  const int lineHeight = (8 * safeFontSize) + 4;
  const int maxLines = (SCREEN_HEIGHT - 16) / lineHeight;

  String lines[maxLines > 20 ? 20 : maxLines];
  int lineCount = 0;
  String remaining = text;
  remaining.trim();
  int usableMaxLines = maxLines > 20 ? 20 : maxLines;
  while (remaining.length() > 0 && lineCount < usableMaxLines) {
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

  tft.setTextSize(safeFontSize);
  tft.setTextColor(userFaceColor);
  tft.setTextWrap(false);
  int totalH = lineCount * lineHeight;
  int startY = 8 + (SCREEN_HEIGHT - 16 - totalH) / 2;
  if (startY < 8) startY = 8;
  for (int i = 0; i < lineCount; i++) {
    tft.setCursor(textLeft, startY + i * lineHeight);
    tft.print(lines[i]);
  }
}

// ─── Preferences and state (identical to mini) ───

void tryStoredPrefs() {
  preferences.begin("desk-cfg", true);
  currentSsid = preferences.getString("ssid", "");
  storedWifiPass = preferences.getString("pass", "");
  relayUrl = preferences.getString("relay_url", "");
  deviceToken = preferences.getString("device_token", "");
  petPersonality = normalizePetPersonality(preferences.getString("pet_personality", petPersonality));
  activePetMode = normalizePetMode(preferences.getString("pet_mode", activePetMode));
  companionHair = normalizeCompanionHair(preferences.getString("companion_hair", companionHair));
  companionEars = normalizeCompanionEars(preferences.getString("companion_ears", companionEars));
  companionMustache = normalizeCompanionMustache(preferences.getString("companion_mustache", companionMustache));
  companionGlasses = normalizeCompanionGlasses(preferences.getString("companion_glasses", companionGlasses));
  companionHeadwear = normalizeCompanionHeadwear(preferences.getString("companion_headwear", companionHeadwear));
  companionPiercing = normalizeCompanionPiercing(preferences.getString("companion_piercing", companionPiercing));
  companionHairSize = clampAppearancePercent(preferences.getInt("companion_hair_size", companionHairSize));
  companionMustacheSize = clampAppearancePercent(preferences.getInt("companion_mustache_size", companionMustacheSize));
  companionHairWidth = clampAppearancePercent(preferences.getInt("companion_hair_width", companionHairWidth));
  companionHairHeight = clampAppearancePercent(preferences.getInt("companion_hair_height", companionHairHeight));
  companionHairThickness = clampAppearancePercent(preferences.getInt("companion_hair_thickness", companionHairThickness));
  companionHairOffsetX = clampAppearanceOffset(preferences.getInt("companion_hair_offset_x", companionHairOffsetX));
  companionHairOffsetY = clampAppearanceOffset(preferences.getInt("companion_hair_offset_y", companionHairOffsetY));
  companionEyeOffsetY = clampAppearanceOffset(preferences.getInt("companion_eye_offset_y", companionEyeOffsetY));
  companionMouthOffsetY = clampAppearanceOffset(preferences.getInt("companion_mouth_offset_y", companionMouthOffsetY));
  companionMustacheWidth = clampAppearancePercent(preferences.getInt("companion_mustache_width", companionMustacheWidth));
  companionMustacheHeight = clampAppearancePercent(preferences.getInt("companion_mustache_height", companionMustacheHeight));
  companionMustacheThickness = clampAppearancePercent(preferences.getInt("companion_mustache_thickness", companionMustacheThickness));
  companionMustacheOffsetX = clampAppearanceOffset(preferences.getInt("companion_mustache_offset_x", companionMustacheOffsetX));
  companionMustacheOffsetY = clampAppearanceOffset(preferences.getInt("companion_mustache_offset_y", companionMustacheOffsetY));
  bondLevel = clampLevel(preferences.getInt("bond_level", bondLevel));
  energyLevel = clampLevel(preferences.getInt("energy_level", energyLevel));
  boredomLevel = clampLevel(preferences.getInt("boredom_level", boredomLevel));
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
  Serial.printf("[BOOT] Loaded relay url='%s' token='%s' ssid='%s'\n",
                relayUrl.c_str(), deviceToken.c_str(), currentSsid.c_str());
}

// ─── Command handler (identical to mini) ───

void handleCommandJson(const String& body) {
  const String type = extractJsonStringField(body, "type");
  if (type.isEmpty()) {
    statusText = "Bad command JSON";
    publishStatus();
    return;
  }
  lastIdleInteractionMs = millis(); // track last command for spontaneous greetings

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
    setBanner(extractJsonStringField(body, "text"), extractJsonIntField(body, "speed", 35));
    return;
  }

  if (type == "set_expression") {
    setExpression(extractJsonStringField(body, "expression", "happy"));
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
    companionHair = normalizeCompanionHair(extractJsonStringField(body, "hair", companionHair));
    companionEars = normalizeCompanionEars(extractJsonStringField(body, "ears", companionEars));
    companionMustache = normalizeCompanionMustache(extractJsonStringField(body, "mustache", companionMustache));
    companionGlasses = normalizeCompanionGlasses(extractJsonStringField(body, "glasses", companionGlasses));
    companionHeadwear = normalizeCompanionHeadwear(extractJsonStringField(body, "headwear", companionHeadwear));
    companionPiercing = normalizeCompanionPiercing(extractJsonStringField(body, "piercing", companionPiercing));
    companionHairSize = clampAppearancePercent(extractJsonIntField(body, "hairSize", companionHairSize));
    companionMustacheSize = clampAppearancePercent(extractJsonIntField(body, "mustacheSize", companionMustacheSize));
    companionHairWidth = clampAppearancePercent(extractJsonIntField(body, "hairWidth", companionHairWidth));
    companionHairHeight = clampAppearancePercent(extractJsonIntField(body, "hairHeight", companionHairHeight));
    companionHairThickness = clampAppearancePercent(extractJsonIntField(body, "hairThickness", companionHairThickness));
    companionHairOffsetX = clampAppearanceOffset(extractJsonIntField(body, "hairOffsetX", companionHairOffsetX));
    companionHairOffsetY = clampAppearanceOffset(extractJsonIntField(body, "hairOffsetY", companionHairOffsetY));
    companionEyeOffsetY = clampAppearanceOffset(extractJsonIntField(body, "eyeOffsetY", companionEyeOffsetY));
    companionMouthOffsetY = clampAppearanceOffset(extractJsonIntField(body, "mouthOffsetY", companionMouthOffsetY));
    companionMustacheWidth = clampAppearancePercent(extractJsonIntField(body, "mustacheWidth", companionMustacheWidth));
    companionMustacheHeight = clampAppearancePercent(extractJsonIntField(body, "mustacheHeight", companionMustacheHeight));
    companionMustacheThickness = clampAppearancePercent(extractJsonIntField(body, "mustacheThickness", companionMustacheThickness));
    companionMustacheOffsetX = clampAppearanceOffset(extractJsonIntField(body, "mustacheOffsetX", companionMustacheOffsetX));
    companionMustacheOffsetY = clampAppearanceOffset(extractJsonIntField(body, "mustacheOffsetY", companionMustacheOffsetY));
    persistPetState();
    if (currentMode == MODE_IDLE) renderCurrentMode();
    publishStatus();
    return;
  }

  if (type == "care_action") {
    sendCareAction(extractJsonStringField(body, "action", "pet"));
    return;
  }

  if (type == "set_relay") {
    saveRelaySettings(extractJsonStringField(body, "relayUrl"), extractJsonStringField(body, "deviceToken"));
    return;
  }

  if (type == "set_scene") {
    setScene(extractJsonStringField(body, "scene", "wave"));
    return;
  }

  if (type == "set_particle") {
    setParticleMode(extractJsonStringField(body, "particle", "fireworks"));
    return;
  }

  if (type == "set_countdown") {
    setCountdown((long)extractJsonIntField(body, "seconds", 60));
    return;
  }

  if (type == "set_timezone") {
    ntpUtcOffsetSeconds = extractJsonIntField(body, "offsetSeconds", 0);
    if (WiFi.status() == WL_CONNECTED) syncNtp();
    return;
  }

  if (type == "set_location") {
    weatherLat = extractJsonFloatField(body, "lat", weatherLat);
    weatherLon = extractJsonFloatField(body, "lon", weatherLon);
    lastWeatherFetchMs = 0;  // force immediate refetch
    if (WiFi.status() == WL_CONNECTED) fetchWeather();
    statusText = "Location saved";
    publishStatus();
    return;
  }

  if (type == "show_weather") {
    currentMode = MODE_WEATHER;
    renderCurrentMode();
    publishStatus();
    return;
  }

  if (type == "goodnight") {
    setSleepMode();
    return;
  }

  if (type == "set_rotation") {
    static const uint8_t LANDSCAPE_MADCTL[] = { 0x28, 0x68, 0xA8, 0xE8 };
    int rot = extractJsonIntField(body, "rotation", 0) & 3;
    tft.setRotation(1);  // reset geometry to 320×240
    { uint8_t m = LANDSCAPE_MADCTL[rot]; tft.sendCommand(0x36, &m, 1); }
    preferences.begin("desk-cfg", false);
    preferences.putInt("display_rot", rot);
    preferences.end();
    tft.fillScreen(COL_BG);
    renderCurrentMode();
    statusText = String("MADCTL slot ") + String(rot);
    publishStatus();
    return;
  }

  if (type == "set_colors") {
    int eye    = extractJsonIntField(body, "eyeColor",    -1);
    int face   = extractJsonIntField(body, "faceColor",   -1);
    int accent = extractJsonIntField(body, "accentColor", -1);
    int bodycol= extractJsonIntField(body, "bodyColor",   -1);
    if (eye    >= 0) userEyeColor    = (uint16_t)eye;
    if (face   >= 0) userFaceColor   = (uint16_t)face;
    if (accent >= 0) userAccentColor = (uint16_t)accent;
    if (bodycol>= 0) userBodyColor   = (uint16_t)bodycol;
    preferences.begin("desk-cfg", false);
    preferences.putUShort("col_eye",    userEyeColor);
    preferences.putUShort("col_face",   userFaceColor);
    preferences.putUShort("col_accent", userAccentColor);
    preferences.putUShort("col_body",   userBodyColor);
    preferences.end();
    tft.fillScreen(COL_BG);
    renderCurrentMode();
    statusText = "Colors updated";
    publishStatus();
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

// ─── State setters (identical to mini) ───

void setIdleStatus(const String& value) {
  statusText = value;
  tft.fillScreen(COL_BG);
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
  noteQueueIndex = noteQueueCount - 1;
  currentNote = noteQueue[noteQueueIndex];
  currentNoteFontSize = noteFontSizeQueue[noteQueueIndex];
  currentNoteFlowerAccent = flowerAccent;
  currentMode = MODE_NOTE;
  statusText = "Showing note";
  renderCurrentMode();
  publishStatus();
  // Emoji-reactive: also scan new note text for emoji tokens
  startTransientExpression(pickReactionExpression("note"), 2200, "Loved your note");
  detectEmojiReaction(boundedText);
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
  tft.fillScreen(COL_BG);
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
  tft.fillScreen(COL_BG);
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

// ─── WiFi (identical to mini) ───

bool connectToWifi(const String& ssid, const String& password) {
  WiFi.disconnect(true, false);
  delay(500);
  storedWifiPass = password;
  preferences.begin("desk-cfg", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  if (!relayUrl.isEmpty()) preferences.putString("relay_url", relayUrl);
  if (!deviceToken.isEmpty()) preferences.putString("device_token", deviceToken);
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
  const bool shouldRestoreWifi = !currentSsid.isEmpty() && storedWifiPass.length() > 0;
  if (wifiJoinInProgress()) markWifiJoinFinished();
  if (bootWifiRestorePending) bootWifiRestorePending = false;
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
    if (ssid.isEmpty()) continue;
    bool duplicate = false;
    for (int existing = 0; existing < availableWifiNetworkCount; existing++) {
      if (availableWifiNetworks[existing] == ssid) { duplicate = true; break; }
    }
    if (!duplicate) availableWifiNetworks[availableWifiNetworkCount++] = ssid;
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
    statusText = availableWifiNetworkCount > 0 ? "Wi-Fi list updated" : "No Wi-Fi found";
  }
  publishStatusWithNetworks();
}

// ─── BLE callbacks and setup (identical to mini) ───

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
    if (!body.isEmpty()) handleCommandJson(body);
  }
};

class ImageCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    if (!imageTransferActive) return;
    const auto value = characteristic->getValue();
    const size_t remaining = sizeof(imageBuffer) - receivedImageBytes;
    const size_t chunkSize = bleValueLength(value) < remaining ? bleValueLength(value) : remaining;
    memcpy(imageBuffer + receivedImageBytes, bleValueData(value), chunkSize);
    receivedImageBytes += chunkSize;
  }
};

void pushRelayStatus() {
  if (WiFi.status() != WL_CONNECTED || relayUrl.isEmpty() || deviceToken.isEmpty()) {
    Serial.printf("[RELAY] Push skipped: wifi=%d url=%s tok=%s\n",
                  WiFi.status(), relayUrl.isEmpty() ? "empty" : "ok",
                  deviceToken.isEmpty() ? "empty" : "ok");
    return;
  }
  const String url = relayUrl + "/v1/device/" + deviceToken + "/status";
  const String body = buildStatusJson();
  String response;
  const int code = relayRequest("POST", url, body, response);
  if (code >= 200 && code < 300) {
    lastRelaySuccessMs = millis();
    statusText = "Relay push OK";
    publishStatus();
  } else {
    statusText = (code < 0) ? response : (String("Relay HTTP ") + String(code));
    publishStatus();
  }
  relayStatusDirty = false;
  lastRelayStatusPushMs = millis();
}

void pollRelay() {
  if (WiFi.status() != WL_CONNECTED || relayUrl.isEmpty() || deviceToken.isEmpty()) return;
  const unsigned long pollInterval = currentMode == MODE_BANNER ? 8000UL : 4000UL;
  if (millis() - lastRelayPollMs < pollInterval) return;
  lastRelayPollMs = millis();
  const String url = relayUrl + "/v1/device/" + deviceToken + "/pull";
  String response;
  const int code = relayRequest("GET", url, "", response);
  if (code == 200) {
    lastRelaySuccessMs = millis();
    handleCommandJson(response);
  } else if (code > 0) {
    lastRelaySuccessMs = millis();
  } else {
    statusText = (code < 0) ? response : (String("Relay poll ") + String(code));
    publishStatus();
  }
}

void setupBle() {
  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(517);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());
  BLEService* service = bleServer->createService(SERVICE_UUID);
  commandCharacteristic = service->createCharacteristic(COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
  commandCharacteristic->setCallbacks(new CommandCallbacks());
  statusCharacteristic = service->createCharacteristic(STATUS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  statusCharacteristic->addDescriptor(new BLE2902());
  imageCharacteristic = service->createCharacteristic(IMAGE_UUID, BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_WRITE);
  imageCharacteristic->setCallbacks(new ImageCallbacks());
  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

// ─── Display setup (ST7789 via SPI + FT6336U cap touch via I2C) ───

void setupDisplay() {
  // Enable backlight
  Serial.println("[TFT] Enabling backlight...");
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Start hardware SPI with the board's wired TFT pins before creating display
  Serial.printf("[TFT] SPI.begin(SCK=%d MISO=%d MOSI=%d CS=%d)\n", TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  SPI.setFrequency(40000000);  // 40 MHz — safe for ST7789V

  // Use 3-pin hardware SPI constructor (CS, DC, RST) — SPI bus already configured above
  Serial.println("[TFT] Creating ST7789 display object (hardware SPI)...");
  pTft = new Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
  Serial.println("[TFT] Calling tft.init(240, 320)...");
  tft.init(240, 320);
  tft.setSPISpeed(40000000);  // lock in 40 MHz after init
  Serial.println("[TFT] tft.init() done.");
  // Adafruit's setRotation() sends wrong MADCTL values for this Freenove panel.
  // Use setRotation(1) only to set the internal 320×240 geometry, then override
  // MADCTL via raw SPI.  Four landscape variants the user can cycle through:
  //   0 → 0x28 (MV|RGB)            1 → 0x68 (MX|MV|RGB)
  //   2 → 0xA8 (MY|MV|RGB)         3 → 0xE8 (MX|MY|MV|RGB)
  static const uint8_t LANDSCAPE_MADCTL[] = { 0x28, 0x68, 0xA8, 0xE8 };
  tft.setRotation(1);  // sets internal W=320 H=240
  preferences.begin("desk-cfg", true);
  int displayRot = preferences.getInt("display_rot", 0);
  userEyeColor    = preferences.getUShort("col_eye",    COL_FG);
  userFaceColor   = preferences.getUShort("col_face",   COL_FG);
  userAccentColor = preferences.getUShort("col_accent", COL_ACCENT);
  userBodyColor   = preferences.getUShort("col_body",   COL_ROSE);
  preferences.end();
  displayRot = displayRot & 3;
  { uint8_t m = LANDSCAPE_MADCTL[displayRot]; tft.sendCommand(0x36, &m, 1); }
  Serial.printf("[TFT] MADCTL override: slot %d → 0x%02X\n", displayRot, LANDSCAPE_MADCTL[displayRot]);
  tft.fillScreen(COL_BG);
  displayAvailable = true;
  Serial.println("[TFT] Screen cleared.");

  // Initialize FT6336U capacitive touch via I2C (bare register access, no library)
  Serial.printf("[TFT] Starting I2C for touch (SDA=%d SCL=%d)...\n", TOUCH_SDA, TOUCH_SCL);
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  if (ft6336u_init()) {
    touchAvailable = true;
    Serial.println("[TFT] FT6336U found at 0x38. Touch ready.");
  } else {
    Serial.println("[TFT] WARNING: FT6336U not found at 0x38! Touch disabled.");
  }

  Serial.printf("[TFT] ST7789 240x320 initialized. Free heap: %u\n", ESP.getFreeHeap());
}

// ─── Touch handling (FT6336U capacitive, replaces physical buttons) ───

void handleTouch() {
  if (!touchAvailable) return;
  uint8_t touchCount = ft6336u_touched();
  bool isTouched = (touchCount > 0);
  unsigned long now = millis();

  if (isTouched && !touchActive) {
    touchActive  = true;
    touchStartMs = now;
    TouchPoint p = ft6336u_getPoint();
    // Map raw portrait touch coords → landscape screen coords for setRotation(1):
    // portrait-Y → screen-X, (240 - portrait-X) → screen-Y
    touchStartX  = (int)p.y;
    touchStartY  = 240 - (int)p.x;
  }

  if (!isTouched && touchActive) {
    touchActive = false;
    unsigned long holdDuration = now - touchStartMs;
    lastIdleInteractionMs = now;

    if (holdDuration >= BTN_HOLD_MS) {
      // ─ Long press: clear display
      noteQueueCount = 0; noteQueueIndex = 0;
      currentMode = MODE_IDLE; currentNote = "";
      preferences.begin("desk-cfg", false);
      preferences.remove("note_text");
      preferences.end();
      setIdleStatus("Ready");
      renderCurrentMode(); publishStatus();
      energyLevel   = clampLevel(energyLevel + 4);
      boredomLevel  = clampLevel(boredomLevel + 6);
      persistPetState();
      startTransientExpression(pickReactionExpression("button_clear"), 1600, "Miss my notes");

    } else if (holdDuration >= 500) {
      // ─ Medium hold: comfort reaction
      bondLevel    = clampLevel(bondLevel + 3);
      boredomLevel = clampLevel(boredomLevel - 8);
      persistPetState();
      startTransientExpression(pickReactionExpression("comfort"), 2000, "Thanks for staying");

    } else {
      // ─ Short tap
      if (currentMode == MODE_NOTE && noteQueueCount > 1) {
        // Cycle notes
        noteQueueIndex      = (noteQueueIndex + 1) % noteQueueCount;
        currentNote         = noteQueue[noteQueueIndex];
        currentNoteFontSize = noteFontSizeQueue[noteQueueIndex];
        currentMode         = MODE_NOTE;
        statusText          = "Showing note";
        renderCurrentMode(); publishStatus();
        bondLevel    = clampLevel(bondLevel + 2);
        boredomLevel = clampLevel(boredomLevel - 4);
        persistPetState();
        startTransientExpression(pickReactionExpression("button_next"), 1200, "Thanks for the tap");

      } else if (currentMode != MODE_IDLE && currentMode != MODE_NOTE) {
        // Tap on non-idle screen: dismiss
        setIdleStatus("Ready");

      } else {
        // Tap in idle/note: double-tap = cheer, single tap = pet
        if (now - lastTapReleaseMs < 400UL && lastTapReleaseMs > 0) {
          // Double tap
          lastTapReleaseMs = 0;
          bondLevel    = clampLevel(bondLevel + 6);
          boredomLevel = clampLevel(boredomLevel - 12);
          persistPetState();
          startTransientExpression(pickReactionExpression("cheer"), 2200, "Yay!");
        } else {
          // Single tap: sparkle at touch point + pet
          lastTapReleaseMs = now;
          if (displayAvailable) {
            tft.fillCircle(touchStartX, touchStartY, 7, COL_GOLD);
            tft.fillCircle(touchStartX, touchStartY, 3, COL_BG);
          }
          delay(100);
          bondLevel    = clampLevel(bondLevel + 2);
          boredomLevel = clampLevel(boredomLevel - 5);
          persistPetState();
          startTransientExpression(pickReactionExpression("pet"), 1800, "That's nice");
        }
      }
    }
  }
}

// ─── Main ───

void setup() {
  Serial.begin(115200);
  delay(2000);  // extra time to open serial monitor
  Serial.println("\n=== Desk Companion TFT 2.8\" boot ===");
  Serial.printf("[BOOT] Reset reason: %d\n", static_cast<int>(esp_reset_reason()));
  Serial.printf("[BOOT] Free heap: %u  PSRAM: %u\n", ESP.getFreeHeap(), ESP.getFreePsram());

  WiFi.persistent(false);

  Serial.println("[BOOT] setupDisplay...");
  setupDisplay();
  Serial.println("[BOOT] clearImageBuffer...");
  clearImageBuffer();

  Serial.println("[BOOT] tryStoredPrefs...");
  tryStoredPrefs();
  Serial.println("[BOOT] setupBle...");
  setupBle();
  Serial.println("[BOOT] BLE ready.");

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
      bannerOffset -= 3;  // 3px per tick for wider screen
      const int textWidth = currentBanner.length() * 24;  // textSize 4 → 24px/char
      if (bannerOffset < -textWidth) {
        bannerOffset = SCREEN_WIDTH;
      }
      renderBannerFrame();
    }
  }

  if (currentMode == MODE_EXPRESSION) {
    const unsigned long now = millis();
    if (now - lastExpressionTickMs >= 30) {
      lastExpressionTickMs = now;
      expressionPhase = (expressionPhase + 1) % 64;
      renderExpressionFrame();
    }
  }

  if (currentMode == MODE_FLOWER) {
    const unsigned long now = millis();
    if (now - lastExpressionTickMs >= 35) {
      lastExpressionTickMs = now;
      expressionPhase = (expressionPhase + 1) % 64;
      renderFlowerFrame();
    }
  }

  if (currentMode == MODE_SCENE) {
    const unsigned long now = millis();
    if (now - lastExpressionTickMs >= 30) {
      lastExpressionTickMs = now;
      expressionPhase = (expressionPhase + 1) % 64;
      renderSceneFrame();
    }
  }

  if (currentMode == MODE_FIREWORKS || currentMode == MODE_HEART_RAIN ||
      currentMode == MODE_SNOWFALL  || currentMode == MODE_STARFIELD) {
    const unsigned long now = millis();
    if (now - lastParticleTickMs >= 30) {
      lastParticleTickMs = now;
      renderCurrentMode();
    }
  }

  if (currentMode == MODE_COUNTDOWN) {
    const unsigned long now = millis();
    if (now - lastParticleTickMs >= 500) {
      lastParticleTickMs = now;
      renderCountdownFrame();
    }
  }

  // Sleep mode tick (same 35ms as flower)
  if (currentMode == MODE_SLEEP) {
    const unsigned long now = millis();
    if (now - lastExpressionTickMs >= 35) {
      lastExpressionTickMs = now;
      expressionPhase = (expressionPhase + 1) % 64;
      renderSleepFrame();
    }
  }

  // Weather auto-fetch every 10 min when WiFi+location set
  if (WiFi.status() == WL_CONNECTED &&
      (weatherLat != 0.f || weatherLon != 0.f) &&
      (lastWeatherFetchMs == 0 || millis() - lastWeatherFetchMs >= 600000UL)) {
    fetchWeather();
  }

  checkTimeGreetings();

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
      syncNtp();
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

  handleTouch();

  delay(1);
}
