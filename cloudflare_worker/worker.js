// Cloudflare Worker — HTTPS proxy for ESP32 → Railway relay server.
// ESP32 connects here (Cloudflare offers RSA ciphers it can handle),
// then this Worker forwards requests to Railway's HTTPS endpoint.

const DEFAULT_RELAY_ORIGIN = "https://desk-companion-production.up.railway.app";

function getRelayOrigin(env) {
  const configuredOrigin = String(env.RELAY_ORIGIN || DEFAULT_RELAY_ORIGIN).trim();
  return configuredOrigin.replace(/\/+$/, "");
}

function buildTargetUrl(requestUrl, relayOrigin) {
  const incomingUrl = new URL(requestUrl);
  return new URL(`${incomingUrl.pathname}${incomingUrl.search}`, `${relayOrigin}/`);
}

function buildProxyHeaders(request) {
  const headers = new Headers(request.headers);

  // Drop hop-by-hop and proxy-specific headers. Forwarding these can cause
  // Cloudflare subrequests to fail before they ever reach Railway.
  const blockedHeaders = [
    "cf-connecting-ip",
    "cf-ipcountry",
    "cf-ray",
    "cf-visitor",
    "connection",
    "content-length",
    "host",
    "x-forwarded-host",
    "x-forwarded-proto",
  ];

  for (const headerName of blockedHeaders) {
    headers.delete(headerName);
  }

  return headers;
}

export default {
  async fetch(request, env) {
    const relayOrigin = getRelayOrigin(env);
    const targetUrl = buildTargetUrl(request.url, relayOrigin);
    const headers = buildProxyHeaders(request);

    try {
      const response = await fetch(targetUrl, {
        method: request.method,
        headers,
        body: request.method === "GET" || request.method === "HEAD"
          ? undefined
          : request.body,
        redirect: "manual",
      });

      return new Response(response.body, {
        status: response.status,
        statusText: response.statusText,
        headers: response.headers,
      });
    } catch (error) {
      return new Response(
        JSON.stringify({
          error: "proxy_error",
          message: error instanceof Error ? error.message : String(error),
          origin: relayOrigin,
          path: targetUrl.pathname,
        }),
        {
          status: 502,
          headers: { "Content-Type": "application/json" },
        }
      );
    }
  },
};
