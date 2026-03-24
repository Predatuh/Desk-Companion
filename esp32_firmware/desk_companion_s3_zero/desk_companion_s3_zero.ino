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
WiFiClientSecure relayHttpsClient;

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
int bannerSpeed = 35;
int bannerOffset = SCREEN_WIDTH;
unsigned long lastBannerTickMs = 0;
unsigned long lastDecorTickMs = 0;
unsigned long lastExpressionTickMs = 0;
unsigned long lastRelayPollMs = 0;
unsigned long lastRelayStatusPushMs = 0;
bool relayStatusDirty = true;
uint8_t idleOrbit = 0;
uint8_t expressionPhase = 0;
String availableWifiNetworks[10];
int availableWifiNetworkCount = 0;

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
bool beginHttpClient(HTTPClient& client, const String& url, uint16_t timeoutMs = 150);
void clearImageBuffer();
bool decodeBase64IntoImage(const String& input);
void publishStatus();
void drawWrappedText(const String& text, int fontSize);
void renderBannerFrame();
void renderExpressionFrame();
void renderImage();
void renderIdle();
void renderCurrentMode();
void setIdleStatus(const String& value);
void setExpression(const String& expression);
void setNote(const String& text, int fontSize);
void setBanner(const String& text, int speed);
void setImageReady();
void saveRelaySettings(const String& nextRelayUrl, const String& nextDeviceToken);
bool connectToWifi(const String& ssid, const String& password);
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
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\",";
  json += "\"relayUrl\":\"" + jsonEscape(relayUrl) + "\",";
  json += "\"deviceToken\":\"" + jsonEscape(deviceToken) + "\",";
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
    case MODE_IDLE:
    default:
      return "idle";
  }
}

bool beginHttpClient(HTTPClient& client, const String& url, uint16_t timeoutMs) {
  bool started = false;
  if (url.startsWith("https://")) {
    relayHttpsClient.setInsecure();
    started = client.begin(relayHttpsClient, url);
  } else {
    started = client.begin(relayHttpClient, url);
  }
  if (started) {
    client.setTimeout(timeoutMs);
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
  const String payload = buildStatusJson();
  if (statusCharacteristic != nullptr) {
    statusCharacteristic->setValue(payload.c_str());
    statusCharacteristic->notify();
  }
  relayStatusDirty = true;
}

void drawWrappedText(const String& text, int fontSize) {
  display.clearDisplay();
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 8, SH110X_WHITE);
  display.setTextColor(SH110X_WHITE);
  const int safeFontSize = fontSize < 1 ? 1 : (fontSize > 2 ? 2 : fontSize);
  const int horizontalPadding = 8;
  const int maxLines = 6;
  const int lineHeight = (8 * safeFontSize) + 2;
  const int maxChars = (SCREEN_WIDTH - (horizontalPadding * 2)) / (6 * safeFontSize);
  String lines[maxLines];
  int lineCount = 0;
  String remaining = text;
  remaining.trim();

  while (remaining.length() > 0 && lineCount < maxLines) {
    int lineLength = remaining.length() > maxChars ? maxChars : remaining.length();
    if (lineLength < remaining.length()) {
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
  int cursorY = (SCREEN_HEIGHT - totalHeight) / 2;
  if (cursorY < 8) {
    cursorY = 8;
  }

  for (int i = 0; i < lineCount; i++) {
    int16_t x1;
    int16_t y1;
    uint16_t width;
    uint16_t height;
    display.getTextBounds(lines[i], 0, 0, &x1, &y1, &width, &height);
    display.setCursor((SCREEN_WIDTH - width) / 2, cursorY);
    display.print(lines[i]);
    cursorY += lineHeight;
  }

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
      drawWrappedText(currentNote, currentNoteFontSize);
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

void drawEyeRect(int cx, int cy, int w, int h, int r) {
  display.fillRoundRect(cx - w / 2, cy - h / 2, w, h, r, SH110X_WHITE);
}

void drawHappyArc(int cx, int cy, int w) {
  for (int t = 0; t < 4; t++) {
    int hw = w / 2;
    display.drawLine(cx - hw, cy + 3 + t, cx, cy - 3 + t, SH110X_WHITE);
    display.drawLine(cx, cy - 3 + t, cx + hw, cy + 3 + t, SH110X_WHITE);
  }
}

void drawThickSmile(int cx, int cy, int w) {
  for (int t = 0; t < 3; t++) {
    int hw = w / 2;
    display.drawLine(cx - hw, cy + t, cx - hw / 3, cy + 5 + t, SH110X_WHITE);
    display.drawLine(cx - hw / 3, cy + 5 + t, cx + hw / 3, cy + 5 + t, SH110X_WHITE);
    display.drawLine(cx + hw / 3, cy + 5 + t, cx + hw, cy + t, SH110X_WHITE);
  }
}

void drawKissLips(int cx, int cy) {
  display.fillCircle(cx, cy, 3, SH110X_WHITE);
  display.fillCircle(cx, cy, 1, SH110X_BLACK);
}

void drawBigHeart(int cx, int cy, int s) {
  display.fillCircle(cx - s, cy - s / 3, s, SH110X_WHITE);
  display.fillCircle(cx + s, cy - s / 3, s, SH110X_WHITE);
  display.fillTriangle(
    cx - s * 2, cy - s / 3 + 1,
    cx + s * 2, cy - s / 3 + 1,
    cx, cy + s * 2,
    SH110X_WHITE
  );
}

void renderExpressionFrame() {
  display.clearDisplay();

  const int LX = 38;
  const int RX = 90;
  const int EY = 24;
  const int MY = 48;

  if (currentExpression == "heart") {
    static const int sizes[] = {7, 8, 9, 10, 11, 12, 12, 11, 10, 9, 8, 7};
    int s = sizes[expressionPhase % 12];
    drawBigHeart(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 4, s);
    display.display();
    return;
  }

  if (currentExpression == "happy") {
    bool blink = (expressionPhase % 12) >= 10;
    if (blink) {
      drawEyeRect(LX, EY, 22, 4, 2);
      drawEyeRect(RX, EY, 22, 4, 2);
    } else {
      drawEyeRect(LX, EY, 22, 16, 5);
      drawEyeRect(RX, EY, 22, 16, 5);
    }
    drawThickSmile(SCREEN_WIDTH / 2, MY, 20);
  } else if (currentExpression == "smile") {
    drawHappyArc(LX, EY, 22);
    drawHappyArc(RX, EY, 22);
    drawThickSmile(SCREEN_WIDTH / 2, MY, 16);
  } else if (currentExpression == "confused") {
    drawEyeRect(LX, EY - 2, 22, 18, 5);
    drawEyeRect(RX, EY + 2, 18, 12, 4);
    for (int t = 0; t < 3; t++) {
      display.drawLine(LX - 12, EY - 16 + t, LX + 8, EY - 12 + t, SH110X_WHITE);
      display.drawLine(RX - 6, EY - 10 + t, RX + 12, EY - 14 + t, SH110X_WHITE);
    }
    for (int t = 0; t < 2; t++) {
      display.drawLine(SCREEN_WIDTH / 2 - 10, MY + 2 + t, SCREEN_WIDTH / 2 + 10, MY - 2 + t, SH110X_WHITE);
    }
  } else if (currentExpression == "look_around") {
    static const int xShifts[] = {0, 2, 4, 6, 6, 4, 2, 0, -2, -4, -6, -4};
    int dx = xShifts[expressionPhase % 12];
    drawEyeRect(LX + dx, EY, 22, 16, 5);
    drawEyeRect(RX + dx, EY, 22, 16, 5);
    for (int t = 0; t < 2; t++) {
      display.drawLine(SCREEN_WIDTH / 2 - 6, MY + t, SCREEN_WIDTH / 2 + 6, MY + t, SH110X_WHITE);
    }
  } else if (currentExpression == "kiss") {
    drawEyeRect(LX, EY, 22, 4, 2);
    drawEyeRect(RX, EY, 22, 16, 5);
    drawKissLips(SCREEN_WIDTH / 2, MY);
    static const int heartY[] = {0, -2, -4, -6, -8, -10, -12, -14, -12, -8, -4, -1};
    drawBigHeart(100, 24 + heartY[expressionPhase % 12], 3);
    if (expressionPhase % 12 > 3) {
      drawBigHeart(112, 30 + heartY[(expressionPhase + 6) % 12], 2);
    }
  } else {
    drawEyeRect(LX, EY, 22, 16, 5);
    drawEyeRect(RX, EY, 22, 16, 5);
  }

  display.display();
}

void setIdleStatus(const String& value) {
  statusText = value;
  currentMode = MODE_IDLE;
  renderCurrentMode();
  publishStatus();
}

void setNote(const String& text, int fontSize) {
  const String boundedText = text.length() > NOTE_TEXT_MAX ? text.substring(0, NOTE_TEXT_MAX) : text;
  const int boundedFontSize = fontSize < 1 ? 1 : (fontSize > 2 ? 2 : fontSize);

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
  publishStatus();
}

bool connectToWifi(const String& ssid, const String& password) {
  const bool switchingNetworks = currentSsid != ssid;

  if (WiFi.status() == WL_CONNECTED || switchingNetworks) {
    WiFi.disconnect(false, false);
    delay(250);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.begin(ssid.c_str(), password.c_str());

  statusText = "Joining Wi-Fi";
  currentSsid = ssid;
  ipAddress = "";
  publishStatus();

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(200);
  }

  if (WiFi.status() != WL_CONNECTED) {
    statusText = "Wi-Fi failed";
    ipAddress = "";
    publishStatus();
    return false;
  }

  currentSsid = ssid;
  ipAddress = WiFi.localIP().toString();
  WiFi.setAutoReconnect(true);
  preferences.begin("desk-cfg", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  preferences.putString("relay_url", relayUrl);
  preferences.putString("device_token", deviceToken);
  preferences.end();

  statusText = "Wi-Fi connected";
  publishStatus();
  return true;
}

void scanWifiNetworks() {
  WiFi.mode(WIFI_STA);
  statusText = "Scanning Wi-Fi";
  publishStatus();

  WiFi.scanDelete();
  availableWifiNetworkCount = 0;

  const int foundNetworks = WiFi.scanNetworks();
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
    if (duplicate) {
      continue;
    }

    availableWifiNetworks[availableWifiNetworkCount++] = ssid;
  }

  WiFi.scanDelete();
  statusText = availableWifiNetworkCount > 0 ? "Wi-Fi list updated" : "No Wi-Fi found";
  publishStatus();
}

void tryStoredWifi() {
  preferences.begin("desk-cfg", true);
  currentSsid = preferences.getString("ssid", "");
  const String password = preferences.getString("pass", "");
  relayUrl = preferences.getString("relay_url", "");
  deviceToken = preferences.getString("device_token", "");
  preferences.end();

  if (!currentSsid.isEmpty()) {
    connectToWifi(currentSsid, password);
  }
}

void handleCommandJson(const String& body) {
  const String type = extractJsonStringField(body, "type");
  if (type.isEmpty()) {
    statusText = "Bad command JSON";
    publishStatus();
    return;
  }

  if (type == "connect_wifi") {
    connectToWifi(
      extractJsonStringField(body, "ssid"),
      extractJsonStringField(body, "password")
    );
    return;
  }

  if (type == "scan_wifi") {
    scanWifiNetworks();
    return;
  }

  if (type == "set_note") {
    setNote(
      extractJsonStringField(body, "text"),
      extractJsonIntField(body, "fontSize", 1)
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
    return;
  }

  HTTPClient client;
  if (!beginHttpClient(client, relayUrl + "/v1/device/" + deviceToken + "/status")) {
    return;
  }
  client.addHeader("Content-Type", "application/json");
  client.POST(buildStatusJson());
  client.end();

  relayStatusDirty = false;
  lastRelayStatusPushMs = millis();
}

void pollRelay() {
  if (WiFi.status() != WL_CONNECTED || relayUrl.isEmpty() || deviceToken.isEmpty()) {
    return;
  }
  const unsigned long pollInterval = currentMode == MODE_BANNER ? 8000UL : 2500UL;
  if (millis() - lastRelayPollMs < pollInterval) {
    return;
  }

  lastRelayPollMs = millis();

  HTTPClient client;
  if (!beginHttpClient(client, relayUrl + "/v1/device/" + deviceToken + "/pull", 1200)) {
    return;
  }

  const int code = client.GET();
  if (code == 200) {
    handleCommandJson(client.getString());
  }
  client.end();
}

void setupBle() {
  BLEDevice::init(DEVICE_NAME);
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
  setupDisplay();
  setupButtons();
  clearImageBuffer();
  setupBle();
  tryStoredWifi();
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
    if (now - lastExpressionTickMs >= 120) {
      lastExpressionTickMs = now;
      expressionPhase = (expressionPhase + 1) % 12;
      renderExpressionFrame();
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    ipAddress = WiFi.localIP().toString();
  } else {
    ipAddress = "";
  }

  if (relayStatusDirty || (millis() - lastRelayStatusPushMs >= 30000)) {
    pushRelayStatus();
  }
  pollRelay();
  handleButtons();

  delay(1);
}