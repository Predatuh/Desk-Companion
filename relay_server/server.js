const http = require('http');

const port = Number(process.env.PORT || 8787);
const devices = new Map();

function getDevice(token) {
  if (!devices.has(token)) {
    devices.set(token, {
      queue: [],
      lastStatus: null,
      updatedAt: null,
    });
  }
  return devices.get(token);
}

function sendJson(res, statusCode, body) {
  res.writeHead(statusCode, { 'content-type': 'application/json' });
  res.end(JSON.stringify(body));
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
  const pathname = new URL(url, 'http://localhost').pathname;
  return pathname.split('/').filter(Boolean);
}

const server = http.createServer(async (req, res) => {
  try {
    const parts = parsePath(req.url || '/');

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
      device.queue.push(body.command);
      device.updatedAt = new Date().toISOString();
      return sendJson(res, 202, {
        queued: true,
        pending: device.queue.length,
        token,
      });
    }

    if (req.method === 'GET' && action === 'pull') {
      const nextCommand = device.queue.shift();
      device.updatedAt = new Date().toISOString();
      if (!nextCommand) {
        res.writeHead(204);
        return res.end();
      }
      return sendJson(res, 200, nextCommand);
    }

    if (req.method === 'POST' && action === 'status') {
      const body = JSON.parse((await readBody(req)) || '{}');
      device.lastStatus = body;
      device.updatedAt = new Date().toISOString();
      return sendJson(res, 200, { stored: true, token });
    }

    if (req.method === 'GET' && action === 'status') {
      return sendJson(res, 200, {
        token,
        pending: device.queue.length,
        updatedAt: device.updatedAt,
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

server.listen(port, () => {
  console.log(`Desk Companion relay listening on ${port}`);
});
