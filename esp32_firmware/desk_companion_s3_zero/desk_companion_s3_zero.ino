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

enum DisplayMode {
  MODE_IDLE,
  MODE_NOTE,
  MODE_BANNER,
  MODE_IMAGE,
};

DisplayMode currentMode = MODE_IDLE;
String statusText = "Booting";
String currentNote = "hi honey";
String currentBanner = "hello from your desk buddy";
String currentSsid = "";
String ipAddress = "";
String relayUrl = "";
String deviceToken = "";
int bannerSpeed = 35;
int bannerOffset = SCREEN_WIDTH;
unsigned long lastBannerTickMs = 0;
unsigned long lastDecorTickMs = 0;
unsigned long lastRelayPollMs = 0;
unsigned long lastRelayStatusPushMs = 0;
bool relayStatusDirty = true;
uint8_t idleOrbit = 0;
String availableWifiNetworks[10];
int availableWifiNetworkCount = 0;

uint8_t imageBuffer[SCREEN_WIDTH * SCREEN_HEIGHT / 8] = {0};
size_t expectedImageBytes = 0;
size_t receivedImageBytes = 0;
bool imageTransferActive = false;

// Note queue
String noteQueue[NOTE_QUEUE_MAX];
int noteQueueCount = 0;
int noteQueueIndex = 0;

// Button state
bool btnNextLast = HIGH;
bool btnClearLast = HIGH;
bool btnClearHoldFired = false;
unsigned long btnNextDownMs = 0;
unsigned long btnClearDownMs = 0;

const char* modeName(DisplayMode mode);
bool beginHttpClient(HTTPClient& client, const String& url);
void clearImageBuffer();
bool decodeBase64IntoImage(const String& input);
void publishStatus();
void drawWrappedText(const String& text);
void renderBannerFrame();
void renderImage();
void renderIdle();
void renderCurrentMode();
void setIdleStatus(const String& value);
void setNote(const String& text);
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
    case MODE_IDLE:
    default:
      return "idle";
  }
}

bool beginHttpClient(HTTPClient& client, const String& url) {
  if (url.startsWith("https://")) {
    return false;
  }
  return client.begin(url);
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

void drawWrappedText(const String& text) {
  display.clearDisplay();
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 8, SH110X_WHITE);
  display.drawCircle(10, 9, 2, SH110X_WHITE);
  display.drawCircle(118, 9, 2, SH110X_WHITE);
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setTextWrap(true);

  const int maxChars = 20;
  int cursorY = 14;
  String remaining = text;
  while (remaining.length() > 0 && cursorY <= 54) {
    int lineLength = remaining.length() > maxChars ? maxChars : remaining.length();
    if (lineLength < remaining.length()) {
      const int split = remaining.lastIndexOf(' ', lineLength);
      if (split > 0) {
        lineLength = split;
      }
    }

    const String line = remaining.substring(0, lineLength);
    int16_t x1;
    int16_t y1;
    uint16_t width;
    uint16_t height;
    display.getTextBounds(line, 0, 0, &x1, &y1, &width, &height);
    display.setCursor((SCREEN_WIDTH - width) / 2, cursorY);
    display.println(line);
    remaining = remaining.substring(lineLength);
    remaining.trim();
    cursorY += 12;
  }

  display.display();
}

void renderBannerFrame() {
  display.clearDisplay();
  display.fillRect(0, 0, SCREEN_WIDTH, 14, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK);
  display.setTextSize(1);
  display.setCursor(6, 3);
  display.print("FOR YOUR DESK");
  display.setTextColor(SH110X_WHITE);
  display.drawLine(0, 18, SCREEN_WIDTH - 1, 18, SH110X_WHITE);
  display.setTextSize(2);
  display.setTextWrap(false);
  display.setCursor(bannerOffset, 32);
  display.print(currentBanner);
  display.display();
}

void renderImage() {
  display.clearDisplay();
  display.drawBitmap(0, 0, imageBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 6, SH110X_WHITE);
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
      drawWrappedText(currentNote);
      break;
    case MODE_BANNER:
      renderBannerFrame();
      break;
    case MODE_IMAGE:
      renderImage();
      break;
    case MODE_IDLE:
    default:
      renderIdle();
      break;
  }
}

void setIdleStatus(const String& value) {
  statusText = value;
  currentMode = MODE_IDLE;
  renderCurrentMode();
  publishStatus();
}

void setNote(const String& text) {
  const String boundedText = text.length() > NOTE_TEXT_MAX ? text.substring(0, NOTE_TEXT_MAX) : text;

  // Push into circular queue, evicting oldest when full
  if (noteQueueCount < NOTE_QUEUE_MAX) {
    noteQueue[noteQueueCount++] = boundedText;
  } else {
    for (int i = 0; i < NOTE_QUEUE_MAX - 1; i++) noteQueue[i] = noteQueue[i + 1];
    noteQueue[NOTE_QUEUE_MAX - 1] = boundedText;
    if (noteQueueIndex > 0) noteQueueIndex--;
  }
  // Display the newest note
  noteQueueIndex = noteQueueCount - 1;
  currentNote = noteQueue[noteQueueIndex];
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
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  statusText = "Joining Wi-Fi";
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
    setNote(extractJsonStringField(body, "text"));
    return;
  }

  if (type == "set_banner") {
    setBanner(
      extractJsonStringField(body, "text"),
      extractJsonIntField(body, "speed", 35)
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
  if (millis() - lastRelayPollMs < 2500) {
    return;
  }

  lastRelayPollMs = millis();

  HTTPClient client;
  if (!beginHttpClient(client, relayUrl + "/v1/device/" + deviceToken + "/pull")) {
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