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

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3D

#define OLED_SDA_PIN -1
#define OLED_SCL_PIN -1

#define BTN_NEXT_PIN  4   // short press → next note
#define BTN_CLEAR_PIN 5   // 5-second hold → clear display
#define BTN_HOLD_MS   5000UL
#define NOTE_QUEUE_MAX 5
#define NOTE_TEXT_MAX 80

static const char* DEVICE_NAME = "Desk Companion S3";
static const char* SERVICE_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f001";
static const char* COMMAND_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f002";
static const char* STATUS_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f003";
static const char* IMAGE_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f004";

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Preferences preferences;

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
};

DisplayMode currentMode = MODE_IDLE;
String statusText = "Booting";
String currentNote = "hi honey";
String currentBanner = "hello from your desk buddy";
String currentSsid = "";
String ipAddress = "";
String relayUrl = "";
String deviceToken = "";
String currentExpression = "happy";
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
unsigned long lastWifiCheckMs = 0;
unsigned long lastWifiBeginMs = 0;
bool wifiJoinActive = false;
bool relayStatusDirty = true;
uint8_t idleOrbit = 0;
uint8_t expressionPhase = 0;
String availableWifiNetworks[10];
int availableWifiNetworkCount = 0;

// Pending WiFi connect (non-blocking from BLE callback)
bool wifiConnectPending = false;
String pendingWifiSsid = "";
String pendingWifiPass = "";
bool wifiScanPending = false;
bool wifiScanInProgress = false;
bool wifiWasConnected = false;
String storedWifiPass = "";  // kept for boot WiFi connect in setup()
int wifiReconnectAttempts = 0;
unsigned long lastRelayAttemptMs = 0;

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
void drawWrappedText(const String& text, int fontSize, int border, const String& icons);
void renderBannerFrame();
void renderExpressionFrame();
void renderImage();
void renderIdle();
void renderCurrentMode();
void setIdleStatus(const String& value);
void setExpression(const String& expression);
void setNote(const String& text, int fontSize, int border, const String& icons);
void setBanner(const String& text, int speed);
void setImageReady();
void saveRelaySettings(const String& nextRelayUrl, const String& nextDeviceToken);
bool connectToWifi(const String& ssid, const String& password);
bool wifiJoinInProgress();
void markWifiJoinStarted();
void markWifiJoinFinished();
void scanWifiNetworks();
void tryStoredWifi();
void handleCommandJson(const String& body);
void pushRelayStatus();
void pollRelay();
void setupBle();
void setupDisplay();
void setupButtons();
void handleButtons();

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
  // Status for relay push and HTTP GET (keep compact for TLS memory safety)
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\",";
  json += "\"relayUrl\":\"" + jsonEscape(relayUrl) + "\",";
  json += "\"deviceToken\":\"" + jsonEscape(deviceToken) + "\"";
  json += "}";
  return json;
}

String buildBleStatusJson() {
  // Compact status for BLE notify (fits in MTU)
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\"";
  json += "}";
  return json;
}

String buildBleStatusWithNetworksJson() {
  // BLE status with wifi networks included (after scan)
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\",";
  json += "\"wifiNetworks\":[";
  for (int i = 0; i < availableWifiNetworkCount; i++) {
    if (i > 0) json += ",";
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
  
  String finalUrl = url;
  if (finalUrl.startsWith("http://") && finalUrl.indexOf(".railway.app") != -1) {
    finalUrl = "https://" + finalUrl.substring(7);
  }

  Serial.println(String("[http] begin url=") + finalUrl + " heap=" + String(ESP.getFreeHeap()));

  if (finalUrl.startsWith("https://")) {
    started = client.begin(relaySecureClient, finalUrl);
  } else {
    started = client.begin(relayHttpClient, finalUrl);
  }
  
  if (started) {
    client.setReuse(true); // MUST be true for Keep-Alive
    client.setTimeout(timeoutMs);
  } else {
    Serial.println("[http] client.begin returned false!");
  }
  return started;
}

void clearImageBuffer() {
  memset(imageBuffer, 0, sizeof(imageBuffer));
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
  const String payload = buildBleStatusJson();
  if (statusCharacteristic != nullptr) {
    statusCharacteristic->setValue(payload.c_str());
    statusCharacteristic->notify();
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
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setTextWrap(false);
  display.setCursor(bannerOffset, 25);
  display.print(currentBanner);
  display.display();
}

void renderImage() {
  display.clearDisplay();
  display.drawBitmap(0, 0, imageBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
  display.display();
}

void renderIdle() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 10, SH110X_WHITE);
  display.setCursor(12, 7);
  display.println("Desk Companion");
  display.drawLine(10, 17, SCREEN_WIDTH - 11, 17, SH110X_WHITE);
  display.setCursor(12, 24);
  display.println(ipAddress.isEmpty() ? "Waiting for Wi-Fi" : ipAddress);
  display.setCursor(12, 38);
  display.println("Use app or website");
  display.setCursor(12, 50);
  display.println("to interact");

  const int orbitXs[4] = {100, 108, 116, 108};
  const int orbitYs[4] = {22, 14, 22, 30};
  display.fillCircle(orbitXs[idleOrbit % 4], orbitYs[idleOrbit % 4], 2, SH110X_WHITE);
  display.display();
}

void renderCurrentMode() {
  switch (currentMode) {
    case MODE_NOTE:
      drawWrappedText(currentNote, currentNoteFontSize, currentNoteBorder, currentNoteIcons);
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

// Interpolate between two ints
static inline int lerp(int a, int b, float t) {
  return a + (int)((b - a) * t);
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
  display.clearDisplay();

  // EMO geometry — bigger eyes, wider spacing
  const int LX = 36;   // left eye center X
  const int RX = 92;   // right eye center X
  const int EY = 24;   // eye center Y
  const int MY = 52;   // mouth center Y
  const int EW = 28;   // eye width  (EMO = wide)
  const int EH = 22;   // eye height (EMO = tall)
  const int ER = 7;    // eye corner radius (rounder = more EMO)

  int ph = expressionPhase % 32;
  float t = (float)ph / 31.0f; // 0..1 progress through the cycle

  // ── Heart: big pulsing heart, smooth sine size ──
  if (currentExpression == "heart") {
    // Smooth 32-phase pulse: 8→14→8
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int s = 8 + (int)(6.0f * wave);
    drawBigHeart(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 2, s);
    display.display();
    return;
  }

  // ── Love: heart-pupil eyes, huge grin, floating hearts ──
  if (currentExpression == "love") {
    drawEye(LX, EY, EW, EH, ER, 0, 0);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    // Draw hearts OVER the pupils
    drawBigHeart(LX, EY, 4);
    drawBigHeart(RX, EY, 4);
    drawSmile(SCREEN_WIDTH / 2, MY - 2, 28);
    // Two floating hearts, staggered
    float r1 = fmod(t * 2.0f, 1.0f);
    float r2 = fmod(t * 2.0f + 0.5f, 1.0f);
    int y1 = MY - 4 - (int)(r1 * 40.0f);
    int y2 = MY - 4 - (int)(r2 * 36.0f);
    if (r1 < 0.8f) drawBigHeart(16 + (int)(r1 * 10.0f), y1, 3);
    if (r2 < 0.8f) drawBigHeart(112 - (int)(r2 * 10.0f), y2, 2);
    display.display();
    return;
  }

  // ── Surprised: eyes widen dramatically, brows raised, O-mouth ──
  if (currentExpression == "surprised") {
    // Eyes grow and shrink smoothly
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int eh = EH + (int)(wave * 8.0f);
    drawEye(LX, EY, EW + 4, eh, ER + 2, 0, 0);
    drawEye(RX, EY, EW + 4, eh, ER + 2, 0, 0);
    int browLift = (int)(wave * 4.0f);
    drawBrow(LX - 14, EY - 17 - browLift, LX + 14, EY - 17 - browLift);
    drawBrow(RX - 14, EY - 17 - browLift, RX + 14, EY - 17 - browLift);
    // Mouth O size pulses
    int mr = 5 + (int)(wave * 3.0f);
    drawOvalMouth(SCREEN_WIDTH / 2, MY, mr, mr - 1);
    display.display();
    return;
  }

  // ── Angry: V-brows, flat squinting eyes, rigid trembling mouth ──
  if (currentExpression == "angry") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    // Eyes stay wide but FLAT — top half clipped by thick V-brows
    drawEye(LX, EY + 2, EW, EH - 2, ER, 0, 2);
    drawEye(RX, EY + 2, EW, EH - 2, ER, 0, 2);
    // Black-fill the top portion of each eye to simulate angry lids
    int lidH = 4 + (int)(wave * 3.0f);
    display.fillRect(LX - EW/2, EY + 2 - (EH-2)/2, EW, lidH, SH110X_BLACK);
    display.fillRect(RX - EW/2, EY + 2 - (EH-2)/2, EW, lidH, SH110X_BLACK);
    // V-shaped brows: inner ends LOW (near nose), outer ends HIGH
    int twitch = (int)(wave * 2.0f);
    drawBrow(LX - 14, EY - 16, LX + 8, EY - 6 - twitch);   // left: outer-high, inner-low
    drawBrow(RX + 14, EY - 16, RX - 8, EY - 6 - twitch);   // right: outer-high, inner-low
    // Small tight straight mouth with tremble
    int mShift = (int)(sin(t * 3.14159f * 6.0f) * 2.0f);
    for (int tt = 0; tt < 3; tt++) {
      display.drawLine(SCREEN_WIDTH/2 - 10, MY + tt + mShift, SCREEN_WIDTH/2 + 10, MY + tt + mShift, SH110X_WHITE);
    }
    display.display();
    return;
  }

  // ── Sad: droopy eyes, sad brows, frown, tears drip ──
  if (currentExpression == "sad") {
    drawEye(LX, EY + 3, EW, EH - 4, ER, 0, 4);
    drawEye(RX, EY + 3, EW, EH - 4, ER, 0, 4);
    // Sad brows — outer raised
    drawBrow(LX - 14, EY - 6, LX + 10, EY - 16);
    drawBrow(RX + 14, EY - 6, RX - 10, EY - 16);
    drawSadArc(SCREEN_WIDTH / 2, MY - 2, 16);
    // Smooth tear drip: tear falls in second half of cycle
    if (ph >= 12) {
      float tearT = (float)(ph - 12) / 19.0f;
      int tearY = EY + 14 + (int)(tearT * 24.0f);
      int tearS = (tearT < 0.5f) ? 3 : 2;
      drawTear(LX + EW/2 + 2, tearY, tearS);
    }
    if (ph >= 20) {
      float tearT = (float)(ph - 20) / 11.0f;
      int tearY = EY + 14 + (int)(tearT * 22.0f);
      drawTear(RX + EW/2 + 2, tearY, 2);
    }
    display.display();
    return;
  }

  // ── Sleepy: eyes slowly droop, ZZZ rises, occasional full close ──
  if (currentExpression == "sleepy") {
    // Smooth sine droop: eye height oscillates EH→6→EH
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int eh = 6 + (int)((1.0f - wave) * (EH - 6));
    drawEye(LX, EY, EW, eh, ER, 0, 0);
    drawEye(RX, EY, EW, eh, ER, 0, 0);
    // Relaxed flat mouth
    for (int tt = 0; tt < 2; tt++) {
      display.drawLine(SCREEN_WIDTH/2 - 8, MY + tt, SCREEN_WIDTH/2 + 8, MY + tt, SH110X_WHITE);
    }
    drawZzz(98, 12, expressionPhase);
    display.display();
    return;
  }

  // ── Thinking: one squinted eye, gaze drifts up-right, bubble pulses ──
  if (currentExpression == "thinking") {
    float wave = sin(t * 3.14159f * 2.0f);
    int px = 2 + (int)(wave * 5.0f);
    int py = -2 + (int)(wave * 3.0f);
    drawEye(LX, EY, EW - 6, 12, ER, px, py); // squinted left
    drawEye(RX, EY, EW, EH, ER, px, py);      // normal right
    // Thought bubbles — 3 dots rising, size pulsing
    float bp = ease(ph, 32);
    int bs = 1 + (int)(bp * 2.0f);
    display.fillCircle(108, 38, bs,     SH110X_WHITE);
    display.fillCircle(114, 28, bs + 1, SH110X_WHITE);
    display.fillCircle(118, 16, bs + 2, SH110X_WHITE);
    // Neutral tilt mouth
    for (int tt = 0; tt < 2; tt++) {
      display.drawLine(SCREEN_WIDTH/2 - 8, MY + 2 + tt, SCREEN_WIDTH/2 + 8, MY + tt, SH110X_WHITE);
    }
    display.display();
    return;
  }

  // ── Happy: smooth gradual blink, pupil drift, big grin ──
  if (currentExpression == "happy") {
    // Smooth blink at phases 24-31: close 24-27, open 28-31
    int eh = EH;
    int py = (int)(sin(t * 3.14159f * 2.0f) * 2.0f); // gentle pupil bounce
    if (ph >= 24 && ph <= 27) {
      // Closing: EH → 3
      float blinkT = (float)(ph - 24) / 3.0f;
      eh = EH - (int)(blinkT * (EH - 3));
    } else if (ph >= 28 && ph <= 31) {
      // Opening: 3 → EH
      float blinkT = (float)(ph - 28) / 3.0f;
      eh = 3 + (int)(blinkT * (EH - 3));
    }
    drawEye(LX, EY, EW, eh, ER, 0, py);
    drawEye(RX, EY, EW, eh, ER, 0, py);
    drawSmile(SCREEN_WIDTH / 2, MY - 2, 24);
  }
  // ── Smile: happy arcs + warm smile ──
  else if (currentExpression == "smile") {
    drawHappyArc(LX, EY, EW);
    drawHappyArc(RX, EY, EW);
    drawSmile(SCREEN_WIDTH / 2, MY - 2, 20);
  }
  // ── Confused: asymmetric eyes, brow twitch, slanted mouth ──
  else if (currentExpression == "confused") {
    float wave = sin(t * 3.14159f * 4.0f) * 0.5f + 0.5f; // double speed
    int browTwitch = (int)(wave * 3.0f);
    drawEye(LX, EY - 2, EW, EH + 4, ER, -2, 0);
    drawEye(RX, EY + 3, EW - 6, EH - 6, ER - 2, 2, 0);
    drawBrow(LX - 14, EY - 15 - browTwitch, LX + 10, EY - 11);
    drawBrow(RX - 8, EY - 9, RX + 14, EY - 13 + browTwitch);
    for (int tt = 0; tt < 3; tt++) {
      display.drawLine(SCREEN_WIDTH/2 - 12, MY + 4 + tt, SCREEN_WIDTH/2 + 12, MY - 2 + tt, SH110X_WHITE);
    }
  }
  // ── Look around: smooth sinusoidal pupil sweep ──
  else if (currentExpression == "look_around") {
    // True sine sweep — much smoother than lookup table
    float sx = sin(t * 3.14159f * 2.0f);
    float sy = cos(t * 3.14159f * 4.0f) * 0.4f;
    int px = (int)(sx * 8.0f);
    int py = (int)(sy * 3.0f);
    drawEye(LX, EY, EW, EH, ER, px, py);
    drawEye(RX, EY, EW, EH, ER, px, py);
    for (int tt = 0; tt < 2; tt++) {
      display.drawLine(SCREEN_WIDTH/2 - 8, MY + tt, SCREEN_WIDTH/2 + 8, MY + tt, SH110X_WHITE);
    }
  }
  // ── Kiss: wink, open eye, lips, hearts rise from mouth ──
  else if (currentExpression == "kiss") {
    // Left eye: smooth wink (stays closed)
    drawBlinkEye(LX, EY, EW, 4, ER);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    drawKissLips(SCREEN_WIDTH / 2, MY);
    // Hearts rise smoothly from lips upward
    float r1 = fmod(t * 1.5f, 1.0f);
    float r2 = fmod(t * 1.5f + 0.4f, 1.0f);
    if (r1 < 0.85f) {
      int hy = MY - 10 - (int)(r1 * 38.0f);
      int hx = SCREEN_WIDTH / 2 + (int)(sin(r1 * 3.14159f) * 8.0f);
      drawBigHeart(hx, hy, 3);
    }
    if (r2 < 0.85f) {
      int hy = MY - 8 - (int)(r2 * 34.0f);
      int hx = SCREEN_WIDTH / 2 + 12 - (int)(sin(r2 * 3.14159f) * 6.0f);
      drawBigHeart(hx, hy, 2);
    }
  }
  // ── Default / Idle personality: neutral face with micro-movements ──
  else {
    // Subtle autonomous micro-movements — the bot feels alive
    float drift = sin(t * 3.14159f * 2.0f);
    int px = (int)(drift * 2.0f); // tiny pupil drift
    int py = (int)(cos(t * 3.14159f * 3.0f) * 1.0f);
    // Occasional blink in idle
    int eh = EH;
    if (ph >= 28 && ph <= 30) {
      float blinkT = (ph == 29) ? 1.0f : 0.5f;
      eh = EH - (int)(blinkT * (EH - 3));
    }
    drawEye(LX, EY, EW, eh, ER, px, py);
    drawEye(RX, EY, EW, eh, ER, px, py);
    // Relaxed flat mouth
    for (int tt = 0; tt < 2; tt++) {
      display.drawLine(SCREEN_WIDTH / 2 - 7, MY + tt, SCREEN_WIDTH / 2 + 7, MY + tt, SH110X_WHITE);
    }
  }

  display.display();
}

void setIdleStatus(const String& value) {
  statusText = value;
  currentMode = MODE_IDLE;
  renderCurrentMode();
  publishStatus();
}

void setNote(const String& text, int fontSize, int border, const String& icons) {
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
  preferences.end();

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
  currentMode = MODE_NOTE;
  statusText = "Showing note";
  renderCurrentMode();
  publishStatus();
}

void setBanner(const String& text, int speed) {
  currentBanner = text;
  bannerSpeed = speed;
  bannerOffset = SCREEN_WIDTH;
  currentMode = MODE_BANNER;
  statusText = "Banner running";
  renderCurrentMode();
  publishStatus();
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

void saveRelaySettings(const String& nextRelayUrl, const String& nextDeviceToken) {
  relayUrl = nextRelayUrl;
  deviceToken = nextDeviceToken;
  preferences.begin("desk-cfg", false);
  preferences.putString("relay_url", relayUrl);
  preferences.putString("device_token", deviceToken);
  preferences.end();
  statusText = relayUrl.isEmpty() ? "Relay cleared" : "Relay configured";
  Serial.println(String("[relay-cfg] url=[") + relayUrl + "] token=[" + deviceToken + "]");
  publishStatus();
  // Push immediately so the relay knows we're online
  relayStatusDirty = true;
}

bool connectToWifi(const String& ssid, const String& password) {
  Serial.println(String("[wifi-join] ssid=[") + ssid + "] pass_len=" + String(password.length()));

  // Soft disconnect — do NOT pass true (that kills the radio on shared BLE/WiFi)
  WiFi.disconnect(false, false);
  delay(200);

  // Save credentials BEFORE attempting connect so they survive reboot.
  // Only save relay settings if they're non-empty to avoid wiping
  // previously configured values when WiFi is set first.
  preferences.begin("desk-cfg", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  if (!relayUrl.isEmpty()) preferences.putString("relay_url", relayUrl);
  if (!deviceToken.isEmpty()) preferences.putString("device_token", deviceToken);
  preferences.end();

  WiFi.mode(WIFI_STA);
  delay(100);  // let mode switch settle
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), password.c_str());
  markWifiJoinStarted();

  statusText = "Joining Wi-Fi";
  currentSsid = ssid;
  ipAddress = "";
  publishStatus();

  Serial.print("[wifi-join] waiting: ");
  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
    Serial.print(String(WiFi.status()) + " ");
    delay(500);
  }
  Serial.println();

  const int finalStatus = WiFi.status();
  Serial.println(String("[wifi-join] final status=") + String(finalStatus));

  markWifiJoinFinished();
  if (finalStatus == WL_CONNECTED) {
    ipAddress = WiFi.localIP().toString();
    wifiWasConnected = true;
    statusText = "Wi-Fi connected";
    Serial.println(String("[wifi-join] OK ip=") + ipAddress);
    publishStatus();
    pushRelayStatus();
    return true;
  }

  wifiWasConnected = false;
  // Show the numeric status so the user can report it
  // 1=NO_SSID_AVAIL  4=CONNECT_FAILED  6=DISCONNECTED
  statusText = String("Wi-Fi failed (") + String(finalStatus) + ")";
  Serial.println(String("[wifi-join] FAILED status=") + String(finalStatus));
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

  // Must disconnect to scan reliably on ESP32
  const bool wasConnected = WiFi.status() == WL_CONNECTED;
  if (wasConnected) {
    WiFi.disconnect(false, false);
    delay(300);
  }

  WiFi.mode(WIFI_STA);
  WiFi.scanDelete();
  availableWifiNetworkCount = 0;

  const int foundNetworks = WiFi.scanNetworks(false, false, false, 300);
  for (int i = 0; i < foundNetworks && availableWifiNetworkCount < 10; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    bool dup = false;
    for (int e = 0; e < availableWifiNetworkCount; e++) {
      if (availableWifiNetworks[e] == ssid) { dup = true; break; }
    }
    if (!dup) availableWifiNetworks[availableWifiNetworkCount++] = ssid;
  }
  WiFi.scanDelete();

  // Reconnect if we were connected before
  if (wasConnected && !currentSsid.isEmpty()) {
    preferences.begin("desk-cfg", true);
    const String storedPass = preferences.getString("pass", "");
    preferences.end();
    WiFi.begin(currentSsid.c_str(), storedPass.c_str());
    WiFi.setAutoReconnect(true);
  }

  statusText = availableWifiNetworkCount > 0 ? "Wi-Fi list updated" : "No Wi-Fi found";
  publishStatusWithNetworks();
}

void tryStoredWifi() {
  preferences.begin("desk-cfg", true);
  currentSsid = preferences.getString("ssid", "");
  storedWifiPass = preferences.getString("pass", "");
  relayUrl = preferences.getString("relay_url", "");
  deviceToken = preferences.getString("device_token", "");
  // Restore last note
  const String savedNote = preferences.getString("note_text", "");
  if (!savedNote.isEmpty()) {
    currentNote = savedNote;
    currentNoteFontSize = preferences.getInt("note_fs", 1);
    currentNoteBorder = preferences.getInt("note_border", 0);
    currentNoteIcons = preferences.getString("note_icons", "");
    noteQueue[0] = currentNote;
    noteFontSizeQueue[0] = currentNoteFontSize;
    noteQueueCount = 1;
    noteQueueIndex = 0;
    currentMode = MODE_NOTE;
  }
  preferences.end();
  // WiFi.begin() is called by setup() after this returns
}

void handleCommandJson(const String& body) {
  const String type = extractJsonStringField(body, "type");
  if (type.isEmpty()) {
    statusText = "Bad command JSON";
    publishStatus();
    return;
  }

  if (type == "connect_wifi") {
    // Don't block BLE callback — defer to loop()
    pendingWifiSsid = extractJsonStringField(body, "ssid");
    pendingWifiPass = extractJsonStringField(body, "password");
    wifiConnectPending = true;
    statusText = "Wi-Fi queued";
    publishStatus();
    return;
  }

  if (type == "scan_wifi") {
    // Defer scan to loop() so BLE callback returns immediately
    wifiScanPending = true;
    statusText = "Scan queued";
    publishStatus();
    return;
  }

  if (type == "forget_wifi") {
    WiFi.disconnect(true, true);  // disconnect + erase SDK credentials
    WiFi.setAutoReconnect(false);
    currentSsid = "";
    ipAddress = "";
    wifiWasConnected = false;
    availableWifiNetworkCount = 0;
    // Clear stored credentials from NVS
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
      extractJsonStringField(body, "icons", "")
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

  if (type == "set_relay") {
    saveRelaySettings(
      extractJsonStringField(body, "relayUrl"),
      extractJsonStringField(body, "deviceToken")
    );
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
    Serial.println(String("[relay-push] SKIP: wifi=") + String(WiFi.status()) + " url=[" + relayUrl + "] token=[" + deviceToken + "]");
    return;
  }

  lastRelayAttemptMs = millis();

  const String url = relayUrl + "/v1/device/" + deviceToken + "/status";
  Serial.println(String("[relay-push] POST ") + url);
  HTTPClient client;
  if (!beginHttpClient(client, url, 5000)) {
    Serial.println("[relay-push] beginHttpClient FAILED");
    return;
  }
  client.addHeader("Content-Type", "application/json");
  client.addHeader("Connection", "keep-alive");
  
  const String body = buildStatusJson();
  Serial.println(String("[relay-push] body=") + body.substring(0, min((int)body.length(), 200)));
  int code = client.POST(body);
  Serial.println(String("[relay-push] response=") + String(code));
  if (code < 0) {
    Serial.println(String("[relay-push] HTTPClient error: ") + client.errorToString(code));
  }
  
  // MUST read the response to clear the socket buffer for the next request
  if (code > 0) {
    client.getString();
  } else {
    // If connection dropped, flush TLS state to reconnect clean next time
    relaySecureClient.stop();
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
  Serial.println(String("[relay-poll] GET ") + url);
  HTTPClient client;
  if (!beginHttpClient(client, url, 5000)) {
    Serial.println("[relay-poll] beginHttpClient FAILED");
    return;
  }
  client.addHeader("Connection", "keep-alive");

  const int code = client.GET();
  Serial.println(String("[relay-poll] response=") + String(code));
  if (code < 0) {
    Serial.println(String("[relay-poll] HTTPClient error: ") + client.errorToString(code));
  }
  if (code == 200) {
    const String cmd = client.getString();
    Serial.println(String("[relay-poll] command=") + cmd);
    handleCommandJson(cmd);
  } else if (code > 0) {
    // MUST read response to keep the socket alive!
    client.getString();
  } else {
    // If connection dropped, flush TLS state to reconnect clean next time
    relaySecureClient.stop();
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

void setupDisplay() {
#if OLED_SDA_PIN >= 0 && OLED_SCL_PIN >= 0
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
#else
  Wire.begin();
#endif

  delay(100); // allow display to settle after power-on

  // Probe bus to find the actual I2C address
  uint8_t foundAddr = 0;
  for (uint8_t addr : {0x3C, 0x3D}) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      foundAddr = addr;
      Serial.print("[OLED] Found I2C device at 0x");
      Serial.println(addr, HEX);
      break;
    }
  }
  if (foundAddr == 0) {
    Serial.println("[OLED] ERROR: No I2C device found! Check SDA/SCL wiring.");
    foundAddr = OLED_ADDRESS;
  }

  if (!display.begin(foundAddr, true)) {
    Serial.print("[OLED] display.begin() FAILED at 0x");
    Serial.println(foundAddr, HEX);
    pinMode(LED_BUILTIN, OUTPUT);
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH); delay(200);
      digitalWrite(LED_BUILTIN, LOW);  delay(200);
    }
  }

  Serial.print("[OLED] display.begin() OK at 0x");
  Serial.println(foundAddr, HEX);

  display.clearDisplay();
  display.display();
}

void setupButtons() {
  pinMode(BTN_NEXT_PIN,  INPUT_PULLUP);
  pinMode(BTN_CLEAR_PIN, INPUT_PULLUP);
}

void handleButtons() {
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

  WiFi.persistent(false);  // never let SDK cache credentials to flash

  setupDisplay();
  setupButtons();
  clearImageBuffer();

  // Load stored credentials from NVS
  tryStoredWifi();

  // BLE first — initialises the shared radio properly
  setupBle();

  // Defer boot WiFi to loop() so it uses the exact same connectToWifi()
  // code path that works when triggered via BLE.  This also gives the
  // BLE stack time to fully settle before we touch the shared radio.
  if (!currentSsid.isEmpty() && storedWifiPass.length() > 0) {
    Serial.println(String("[boot] deferring wifi join for ssid=[") + currentSsid + "]");
    pendingWifiSsid = currentSsid;
    pendingWifiPass = storedWifiPass;
    wifiConnectPending = true;
  }

  // Prevent loop() reconnect tracker from firing before the deferred join
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

  // --- WiFi status tracking ---
  // WiFi.setAutoReconnect(true) handles reconnection at the driver level.
  // We only track state changes here and do a last-resort reconnect after 60s.
  const bool wifiNow = WiFi.status() == WL_CONNECTED;
  if (wifiNow) {
    ipAddress = WiFi.localIP().toString();
    markWifiJoinFinished();
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      wifiReconnectAttempts = 0;
      statusText = "Wi-Fi connected";
      Serial.println(String("[wifi] CONNECTED ip=") + ipAddress);
      publishStatus();
    }
    lastWifiCheckMs = millis();  // reset timer while connected
  } else {
    ipAddress = "";
    if (wifiJoinActive && millis() - lastWifiBeginMs >= 30000) {
      markWifiJoinFinished();
      statusText = "Wi-Fi connect failed";
      Serial.println(String("[wifi] JOIN TIMEOUT status=") + String(WiFi.status()));
      publishStatus();
    }
    if (wifiWasConnected) {
      wifiWasConnected = false;
      statusText = "Wi-Fi reconnecting";
      Serial.println("[wifi] DISCONNECTED — auto-reconnect active");
      lastWifiCheckMs = millis();  // start 60s countdown
      publishStatus();
    }
  }

  // Handle deferred WiFi scan (from BLE callback)
  if (wifiScanPending) {
    wifiScanPending = false;
    scanWifiNetworks();
  }

  // Handle deferred WiFi connect (from BLE callback)
  if (wifiConnectPending) {
    wifiConnectPending = false;
    connectToWifi(pendingWifiSsid, pendingWifiPass);
    pendingWifiSsid = "";
    pendingWifiPass = "";
  }

  if (relayStatusDirty || (millis() - lastRelayStatusPushMs >= 30000)) {
    pushRelayStatus();
  }
  pollRelay();
  handleButtons();

  delay(1);
}
