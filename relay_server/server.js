const http = require('http');

const port = Number(process.env.PORT || 8787);
const devices = new Map();

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

function sendJson(res, statusCode, body) {
  res.writeHead(statusCode, {
    'content-type': 'application/json',
    'cache-control': 'no-store, no-cache, must-revalidate, max-age=0',
    pragma: 'no-cache',
    expires: '0',
  });
  res.end(JSON.stringify(body));
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    let raw = '';
    req.on('data', (chunk) => {
      raw += chunk;
      if (raw.length > 5 * 1024 * 1024) { // 5MB limit
        const err = new Error('payload too large');
        console.error(`Payload size limit exceeded: ${raw.length} bytes`);
        reject(err);
        req.destroy();
      }
    });
    req.on('end', () => resolve(raw));
    req.on('error', (err) => {
        console.error('Request body read error:', err);
        reject(err);
    });
  });
}

function parsePath(url) {
  try {
    const pathname = new URL(url, 'http://localhost').pathname.replace(/^\/+/, '/');
    return pathname.split('/').filter(Boolean);
  } catch (e) {
    console.error(`Invalid URL received: ${url}`);
    return [];
  }
}

async function handleRequest(req, res) {
  const startTime = Date.now();
  res.on('finish', () => {
      console.log(`[${new Date().toISOString()}] ${req.method} ${req.url} - ${res.statusCode} [${Date.now() - startTime}ms]`);
  });

  try {
    const parts = parsePath(req.url || '/');

    if (req.method === 'GET' && parts.length === 1 && parts[0] === 'health') {
      return sendJson(res, 200, { ok: true, devices: devices.size, memory: process.memoryUsage() });
    }

    if (parts.length !== 4 || parts[0] !== 'v1' || parts[1] !== 'device') {
      return sendJson(res, 404, { error: 'not_found', path: req.url });
    }

    const token = decodeURIComponent(parts[2]);
    const action = parts[3];
    const device = getDevice(token);

    if (req.method === 'POST' && action === 'command') {
      const rawBody = await readBody(req);
      if (!rawBody) {
          return sendJson(res, 400, { error: 'empty_body' });
      }
      const body = JSON.parse(rawBody);

      const commands = Array.isArray(body.commands) ? body.commands
          : (body.command && typeof body.command === 'object') ? [body.command]
          : null;
      if (!commands || commands.length === 0) {
        return sendJson(res, 400, { error: 'missing_command' });
      }

      const now = new Date().toISOString();
      const type = commands[0].type || '';

      if (!type.includes('chunk') && !type.includes('commit') && type !== 'status') {
          if(device.queue.length > 0) {
            console.log(`[${token}] Clearing ${device.queue.length} old commands from queue for new command type: ${type}`);
          }
          device.queue = [];
      }

      for (const cmd of commands) {
          device.queue.push(cmd);
      }
      
      // Limit queue size to prevent memory issues
      const MAX_QUEUE_SIZE = 200;
      if (device.queue.length > MAX_QUEUE_SIZE) {
          device.queue = device.queue.slice(device.queue.length - MAX_QUEUE_SIZE);
          console.log(`[${token}] Queue truncated. New size: ${device.queue.length}`);
      }

      device.lastCommandAt = now;
      device.updatedAt = now;
      return sendJson(res, 200, { ok: true, queue_size: device.queue.length });
    }

    if (req.method === 'GET' && action === 'command') {
      const now = new Date().toISOString();
      device.lastPullAt = now;
      device.updatedAt = now;

      const command = device.queue.shift() || null;
      return sendJson(res, 200, { command });
    }

    if (req.method === 'POST' && action === 'status') {
      const now = new Date().toISOString();
      device.lastStatusAt = now;
      device.updatedAt = now;
      const body = JSON.parse((await readBody(req)) || '{}');
      device.lastStatus = body.status || 'unknown';
      return sendJson(res, 200, { ok: true });
    }

    if (req.method === 'GET' && action === 'status') {
      return sendJson(res, 200, {
        lastStatus: device.lastStatus,
        lastCommandAt: device.lastCommandAt,
        lastPullAt: device.lastPullAt,
        lastStatusAt: device.lastStatusAt,
        queueSize: device.queue.length,
      });
    }

    return sendJson(res, 405, { error: 'method_not_allowed' });
  } catch (error) {
    console.error(`[${new Date().toISOString()}] Unhandled error for ${req.method} ${req.url}:`, error);
    if (!res.headersSent) {
        return sendJson(res, 500, { error: 'internal_server_error', message: error.message });
    }
    res.end();
  }
}

const server = http.createServer(handleRequest);

server.listen(port, () => {
  console.log(`Relay server listening on http://localhost:${port}`);
});

process.on('uncaughtException', (err, origin) => {
  console.error(`[${new Date().toISOString()}] Uncaught Exception:`, err);
  console.error(`[${new Date().toISOString()}] Origin:`, origin);
  process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
  console.error(`[${new Date().toISOString()}] Unhandled Rejection at:`, promise);
  console.error(`[${new Date().toISOString()}] Reason:`, reason);
});