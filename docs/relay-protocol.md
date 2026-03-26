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
  "deviceToken": "desk-01"
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
  "ip": "192.168.1.55"
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

## Nearby image transfer over BLE

When BLE is active, large image pushes use chunked transfer:

1. Send `begin_image` with `total` byte count.
2. Write raw bitmap chunks to the image characteristic.
3. Send `commit_image`.

Remote relay image delivery uses `set_image` with a base64 payload instead.