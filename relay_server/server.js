const fs = require('fs/promises');
const http = require('http');
const path = require('path');

const port = Number(process.env.PORT || 8787);
const dataDir = path.join(__dirname, 'data');
const dataFile = path.join(dataDir, 'devices.json');
const publicDir = path.join(__dirname, 'public');
const devices = new Map();
let persistTimer = null;

function getDevice(token) {
  if (!devices.has(token)) {
    devices.set(token, {
      queue: [],
      lastStatus: null,
      lastCommandAt: null,
      lastPullAt: null,
      lastStatusAt: null,
      updatedAt: null,
    });
  }
  return devices.get(token);
}

function serializeDevices() {
  return JSON.stringify(
    Object.fromEntries(devices.entries()),
    null,
    2,
  );
}

async function loadDevices() {
  try {
    const raw = await fs.readFile(dataFile, 'utf8');
    const parsed = JSON.parse(raw);
    for (const [token, value] of Object.entries(parsed)) {
      devices.set(token, {
        queue: Array.isArray(value.queue) ? value.queue : [],
        lastStatus: value.lastStatus ?? null,
        lastCommandAt: value.lastCommandAt ?? null,
        lastPullAt: value.lastPullAt ?? null,
        lastStatusAt: value.lastStatusAt ?? null,
        updatedAt: value.updatedAt ?? null,
      });
    }
  } catch (error) {
    if (error && error.code !== 'ENOENT') {
      console.error('Failed to load relay data:', error);
    }
  }
}

function queuePersist() {
  if (persistTimer) {
    clearTimeout(persistTimer);
  }
  persistTimer = setTimeout(async () => {
    persistTimer = null;
    try {
      await fs.mkdir(dataDir, { recursive: true });
      await fs.writeFile(dataFile, serializeDevices(), 'utf8');
    } catch (error) {
      console.error('Failed to persist relay data:', error);
    }
  }, 150);
}

function sendJson(res, statusCode, body) {
  res.writeHead(statusCode, { 'content-type': 'application/json' });
  res.end(JSON.stringify(body));
}

function sendFile(res, filePath, contentType) {
  fs.readFile(filePath)
    .then((body) => {
      res.writeHead(200, { 'content-type': contentType });
      res.end(body);
    })
    .catch(() => sendJson(res, 404, { error: 'not_found' }));
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    let raw = '';
    req.on('data', (chunk) => {
      raw += chunk;
      if (raw.length > 1024 * 1024) {
        reject(new Error('payload too large'));
        req.destroy();
      }
    });
    req.on('end', () => resolve(raw));
    req.on('error', reject);
  });
}

function parsePath(url) {
  const pathname = new URL(url, 'http://localhost').pathname.replace(/^\/+/, '/');
  return pathname.split('/').filter(Boolean);
}

const server = http.createServer(async (req, res) => {
  try {
    const parts = parsePath(req.url || '/');

    if (req.method === 'GET' && (parts.length === 0 || (parts.length === 1 && parts[0] === 'index.html'))) {
      return sendFile(res, path.join(publicDir, 'index.html'), 'text/html; charset=utf-8');
    }

    if (req.method === 'GET' && parts.length === 1 && parts[0] === 'health') {
      return sendJson(res, 200, { ok: true, devices: devices.size });
    }

    if (parts.length !== 4 || parts[0] !== 'v1' || parts[1] !== 'device') {
      return sendJson(res, 404, { error: 'not_found' });
    }

    const token = decodeURIComponent(parts[2]);
    const action = parts[3];
    const device = getDevice(token);

    if (req.method === 'POST' && action === 'command') {
      const body = JSON.parse((await readBody(req)) || '{}');
      if (!body.command || typeof body.command !== 'object') {
        return sendJson(res, 400, { error: 'missing_command' });
      }
      const now = new Date().toISOString();
      device.queue.push(body.command);
      device.lastCommandAt = now;
      device.updatedAt = now;
      queuePersist();
      return sendJson(res, 202, {
        queued: true,
        pending: device.queue.length,
        token,
      });
    }

    if (req.method === 'GET' && action === 'pull') {
      const now = new Date().toISOString();
      const nextCommand = device.queue.shift();
      device.lastPullAt = now;
      device.updatedAt = now;
      queuePersist();
      if (!nextCommand) {
        res.writeHead(204);
        return res.end();
      }
      return sendJson(res, 200, nextCommand);
    }

    if (req.method === 'POST' && action === 'status') {
      const body = JSON.parse((await readBody(req)) || '{}');
      const now = new Date().toISOString();
      device.lastStatus = body;
      device.lastStatusAt = now;
      device.updatedAt = now;
      queuePersist();
      return sendJson(res, 200, { stored: true, token });
    }

    if (req.method === 'GET' && action === 'status') {
      return sendJson(res, 200, {
        token,
        pending: device.queue.length,
        updatedAt: device.updatedAt,
        lastCommandAt: device.lastCommandAt,
        lastPullAt: device.lastPullAt,
        lastStatusAt: device.lastStatusAt,
        lastStatus: device.lastStatus,
      });
    }

    return sendJson(res, 405, { error: 'method_not_allowed' });
  } catch (error) {
    return sendJson(res, 500, {
      error: 'server_error',
      message: error instanceof Error ? error.message : String(error),
    });
  }
});

loadDevices().finally(() => {
  server.listen(port, () => {
    console.log(`Desk Companion relay listening on ${port}`);
  });
});
