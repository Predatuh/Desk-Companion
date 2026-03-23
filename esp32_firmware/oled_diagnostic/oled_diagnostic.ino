#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Wire.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

struct PinPair {
  int sda;
  int scl;
};

const PinPair kPinPairs[] = {
  {8, 9},
  {9, 8},
  {5, 6},
  {6, 5},
  {1, 2},
  {2, 1},
  {3, 4},
  {4, 3},
};

const uint8_t kAddresses[] = {0x3C, 0x3D};

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool probeAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void showSuccess(int sda, int scl, uint8_t address) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("OLED OK");
  display.println();
  display.print("SDA: ");
  display.println(sda);
  display.print("SCL: ");
  display.println(scl);
  display.print("ADDR: 0x");
  display.println(address, HEX);
  display.println();
  display.println("If you can read this,");
  display.println("the panel is fine.");
  display.display();
}

void blinkForever() {
  pinMode(LED_BUILTIN, OUTPUT);
  while (true) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(150);
    digitalWrite(LED_BUILTIN, LOW);
    delay(150);
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("[OLED-DIAG] Starting");

  for (const PinPair& pair : kPinPairs) {
    Wire.end();
    Wire.begin(pair.sda, pair.scl);
    delay(80);

    Serial.print("[OLED-DIAG] Testing pins SDA=");
    Serial.print(pair.sda);
    Serial.print(" SCL=");
    Serial.println(pair.scl);

    for (uint8_t address : kAddresses) {
      if (!probeAddress(address)) {
        Serial.print("[OLED-DIAG] No response at 0x");
        Serial.println(address, HEX);
        continue;
      }

      Serial.print("[OLED-DIAG] Found response at 0x");
      Serial.println(address, HEX);

      if (display.begin(address, true)) {
        Serial.println("[OLED-DIAG] display.begin() OK");
        showSuccess(pair.sda, pair.scl, address);
        return;
      }

      Serial.println("[OLED-DIAG] display.begin() FAILED");
      delay(200);
    }
  }

  Serial.println("[OLED-DIAG] No working OLED combination found");
  blinkForever();
}

void loop() {
  delay(1000);
}