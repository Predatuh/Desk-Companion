# Desk Companion Relay

Tiny in-memory HTTP relay for off-LAN delivery.

## Endpoints

- `GET /health`
- `POST /v1/device/:token/command`
- `GET /v1/device/:token/pull`
- `POST /v1/device/:token/status`
- `GET /v1/device/:token/status`

## Run

```powershell
cd relay_server
node server.js
```

The ESP32 stores the relay base URL and token, then polls `pull` for queued commands.
