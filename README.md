# Desk Companion

Separate Flutter app, ESP32-S3 firmware, and optional relay service for a tiny desk display.

## What it does

- Connects to the ESP32-S3 Zero over BLE for first-time setup.
- Sends Wi-Fi credentials to the device over BLE.
- Saves Wi-Fi credentials on the device so it can reconnect after reboot or temporary Wi-Fi loss.
- Sends sticky notes, scrolling banner text, and 128x64 monochrome images.
- Includes an exact 128x64 drawing pad so anything you sketch on the phone maps pixel-for-pixel to the OLED.
- Includes a live draw mode that streams the current bitmap while you sketch.
- Supports a small relay server for off-home-network delivery.
- Uses BLE for setup and fallback, plus a hosted relay for remote delivery.

## Flutter app

Project path:

- `desk_companion/`

Main UI flow:

- Find the device over BLE.
- Send Wi-Fi credentials.
- Optionally save relay URL plus device token to the ESP32.
- Send note, banner, image, or drawing payloads.
- Turn on live draw if you want the OLED to update as your finger moves.

Packages used:

- `flutter_blue_plus`
- `provider`
- `http`
- `file_picker`
- `image`
- `google_fonts`

Run it from this folder:

```powershell
cd desk_companion
flutter pub get
flutter run
```

## ESP32-S3 firmware

Sketch path:

- `desk_companion/esp32_firmware/desk_companion_s3_zero/desk_companion_s3_zero.ino`

Arduino libraries required:

- `ESP32 BLE Arduino`
- `Adafruit GFX Library`
- `Adafruit SSD1306`

Display assumptions:

- 128x64 SSD1306 OLED
- I2C address `0x3C`
- default ESP32-S3 I2C pins unless overridden in the sketch

## BLE protocol

Service UUID:

- `63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f001`

Characteristics:

- command write: `63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f002`
- status read/notify: `63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f003`
- image write: `63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f004`

Command payloads are JSON strings. Images are transferred as raw 1024-byte monochrome bitmap chunks after a `begin_image` command.

## Relay server

Project path:

- `desk_companion/relay_server/`

Run it with:

```powershell
cd desk_companion/relay_server
node server.js
```

The ESP32 stores the relay base URL and token, then polls the relay for queued commands.

Firmware note:

- The ESP32 firmware in this repo does not run a local HTTP server.
- Device setup happens over BLE.
- Remote delivery happens through the relay server.
- Wi-Fi credentials are stored in device preferences so the ESP32 can reconnect automatically.
- For the ESP32-S3 Zero build in this repo, use an `http://` relay URL instead of `https://` to keep the sketch within the default app size limit.