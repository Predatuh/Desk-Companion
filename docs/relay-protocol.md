# Desk Companion Relay Protocol

This project uses two transport paths:

- BLE for nearby setup and direct sends.
- Hosted relay for remote delivery from anywhere.

There is no direct LAN or local device HTTP control path in the active app flow.

## Transport model

1. Use BLE when the phone is physically near the device.
2. Use BLE to provision Wi-Fi credentials and relay settings onto the ESP32.
3. After the device joins Wi-Fi, it pushes status to the hosted relay and polls the relay for queued commands.
4. When BLE is unavailable, the app sends commands to the relay over HTTPS.

## Relay endpoints

- `GET /health`
- `POST /v1/device/:token/command`
- `GET /v1/device/:token/pull`
- `POST /v1/device/:token/status`
- `GET /v1/device/:token/status`

## App to relay

Remote sends use:

```json
{
  "command": {
    "type": "set_note",
    "text": "good luck today",
    "fontSize": 2,
    "border": 1,
    "icons": "heart,moon",
    "flowerAccent": "rose"
  }
}
```

The relay stores the command in the device queue and returns a `2xx` response when queued successfully.

Large full-color images are relayed as a sequence of smaller queued commands instead of one oversized payload:

```json
{ "type": "begin_color_image", "total": 153600 }
```

```json
{ "type": "color_image_chunk", "data": "<base64 chunk>" }
```

```json
{ "type": "commit_color_image" }
```

This allows the TFT device to keep polling and assemble the full RGB565 frame in PSRAM without requiring BLE.

## Device to relay

### Pull queued command

The ESP32 polls:

- `GET /v1/device/:token/pull`

Responses:

- `200` with one queued command JSON body.
- `204` when there is nothing queued.

### Push device status

The ESP32 posts:

```json
{
  "mode": "note",
  "status": "Showing note",
  "ssid": "Home WiFi",
  "ip": "192.168.1.55",
  "relayUrl": "https://relay.example.com",
  "deviceToken": "desk-01",
  "personality": "curious",
  "petMode": "hangout",
  "hair": "none",
  "ears": "none",
  "mustache": "none",
  "glasses": "none",
  "headwear": "none",
  "piercing": "none",
  "hairSize": 100,
  "mustacheSize": 100,
  "bondLevel": 50,
  "energyLevel": 72,
  "boredomLevel": 28
}
```

The relay stores this under `lastStatus` and updates `updatedAt`.

## BLE status payload

Standard BLE status notifications contain:

```json
{
  "mode": "idle",
  "status": "BLE connected.",
  "ssid": "Home WiFi",
  "ip": "192.168.1.55",
  "personality": "curious",
  "petMode": "hangout",
  "hair": "none",
  "ears": "none",
  "mustache": "none",
  "glasses": "none",
  "headwear": "none",
  "piercing": "none",
  "hairSize": 100,
  "mustacheSize": 100,
  "bondLevel": 50,
  "energyLevel": 72,
  "boredomLevel": 28
}
```

Wi-Fi scan responses extend that payload with:

```json
{
  "wifiNetworks": ["Home WiFi", "Guest", "Phone Hotspot"]
}
```

## Command types

### BLE-only provisioning commands

- `connect_wifi`

```json
{
  "type": "connect_wifi",
  "ssid": "Home WiFi",
  "password": "secret"
}
```

- `scan_wifi`

```json
{
  "type": "scan_wifi"
}
```

- `forget_wifi`

```json
{
  "type": "forget_wifi"
}
```

- `set_relay`

```json
{
  "type": "set_relay",
  "relayUrl": "https://relay.example.com",
  "deviceToken": "desk-01"
}
```

### Display commands

### Pet and personality commands

- `set_personality`

```json
{
  "type": "set_personality",
  "personality": "playful"
}
```

Supported personalities in the first version:

- `playful`
- `cuddly`
- `sleepy`
- `curious`

- `trigger_pet_mode`

```json
{
  "type": "trigger_pet_mode",
  "petMode": "cuddle"
}
```

Supported pet modes in the first version:

- `off`
- `hangout`
- `play`
- `cuddle`
- `nap`
- `party`
- `needy`

These commands set the device's active companion mode. `off` disables companion behavior, `hangout` returns to the default personality-driven idle behavior, and the other modes stay active until changed again.

- `set_companion_style`

```json
{
  "type": "set_companion_style",
  "hair": "spiky",
  "ears": "cat",
  "mustache": "curled",
  "glasses": "round",
  "headwear": "bow",
  "piercing": "brow",
  "hairSize": 110,
  "mustacheSize": 135
}
```

Supported style options:

- `hair`: `none`, `tuft`, `bangs`, `spiky`, `swoop`, `bob`, `messy`
- `ears`: `none`, `cat`, `bear`, `bunny`
- `mustache`: `none`, `classic`, `curled`, `handlebar`, `walrus`, `pencil`, `imperial`
- `glasses`: `none`, `round`, `square`, `visor`
- `headwear`: `none`, `bow`, `beanie`, `crown`
- `piercing`: `none`, `brow`, `nose`, `lip`
- `hairSize`: integer percentage from `70` to `170`
- `mustacheSize`: integer percentage from `70` to `170`

Style changes persist on the device and are applied to the idle face and emotion scenes. The Flutter app now includes a local live preview so users can adjust these values before sending them.

- `care_action`

```json
{
  "type": "care_action",
  "action": "pet"
}
```

Supported care actions:

- `pet`
- `cheer`
- `comfort`
- `dance`
- `surprise`

Care actions temporarily trigger a reaction scene and also adjust the companion's internal relationship and needs stats.

- `set_note`

```json
{
  "type": "set_note",
  "text": "miss you already <3",
  "fontSize": 2,
  "border": 3,
  "icons": "heart,star",
  "flowerAccent": "sunflower"
}
```

- `set_banner`

```json
{
  "type": "set_banner",
  "text": "good luck today",
  "speed": 35
}
```

- `set_expression`

```json
{
  "type": "set_expression",
  "expression": "happy"
}
```

- `set_flower`

```json
{
  "type": "set_flower",
  "flower": "king_protea"
}
```

- `clear`

```json
{
  "type": "clear"
}
```

- `status`

```json
{
  "type": "status"
}
```

- `set_image`

```json
{
  "type": "set_image",
  "data": "<base64 1024-byte monochrome bitmap>"
}
```

- `set_color_image`

```json
{
  "type": "set_color_image",
  "data": "<base64 320x240 RGB565 bitmap>"
}
```

## Nearby image transfer over BLE

When BLE is active, large image pushes use chunked transfer:

1. Send `begin_image` with `total` byte count.
2. Write raw bitmap chunks to the image characteristic.
3. Send `commit_image`.

Remote relay image delivery uses `set_image` for monochrome bitmaps and `set_color_image` for TFT RGB565 frames.

## Pet behavior notes

- The device now keeps a persistent `personality` and `petMode` in preferences.
- The device also keeps persistent appearance settings for `hair`, `ears`, `mustache`, `glasses`, `headwear`, and `piercing`.
- The device also keeps persistent `bondLevel`, `energyLevel`, and `boredomLevel` values in preferences.
- While the display is idle, the firmware can trigger short autonomous pet-like reactions based on the current companion mode.
- Notes, banners, and hardware button interactions can trigger a brief emotional reaction before the device returns to its previous display mode.