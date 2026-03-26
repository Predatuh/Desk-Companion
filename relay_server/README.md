# Desk Companion Relay

Tiny relay for off-LAN delivery with:

- persisted queue/status state in `relay_server/data/devices.json`
- a simple built-in web UI served from `/`
- support for both `http://` and `https://` relay base URLs on the client side
- BLE remains the nearby setup and direct-send path; this relay is the remote delivery path

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

The relay listens on port `8787` by default.

## Domain setup

1. Point a subdomain such as `relay.yourdomain.com` to the machine running this server.
2. Reverse-proxy the subdomain to `localhost:8787`, or expose `8787` directly.
3. Open `https://relay.yourdomain.com/health` to confirm the relay is live.
4. Open `https://relay.yourdomain.com/` to use the built-in web sender.

Example nginx config:

```nginx
server {
	listen 80;
	listen 443 ssl;
	server_name relay.yourdomain.com;

	location / {
		proxy_pass http://127.0.0.1:8787;
		proxy_http_version 1.1;
		proxy_set_header Host $host;
		proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
	}
}
```

## Web UI

The root path `/` serves a simple page where you can:

- queue sticky notes
- queue banner messages
- clear the display
- inspect the device status

## Notes

- The ESP32 stores the relay base URL and token, then polls `pull` for queued commands.
- Relay state survives server restarts via `relay_server/data/devices.json`.
- The active app flow does not use direct LAN control; remote commands go through this relay and nearby setup stays on BLE.
- Command and status payloads are documented in `docs/relay-protocol.md`.
