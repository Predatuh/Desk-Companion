// Cloudflare Worker — HTTPS proxy for ESP32 → Railway relay server.
// ESP32 connects here (Cloudflare offers RSA ciphers it can handle),
// then this Worker forwards requests to Railway's HTTPS endpoint.

const RELAY_ORIGIN = "https://desk-companion-production.up.railway.app";

export default {
  async fetch(request) {
    const url = new URL(request.url);

    // Forward the request to Railway, preserving method, headers, and body
    const targetUrl = RELAY_ORIGIN + url.pathname + url.search;

    const headers = new Headers(request.headers);
    headers.set("Host", "desk-companion-production.up.railway.app");
    // Remove Cloudflare-specific headers that might confuse the origin
    headers.delete("cf-connecting-ip");
    headers.delete("cf-ray");

    const init = {
      method: request.method,
      headers: headers,
    };

    // Forward body for POST/PUT/PATCH
    if (request.method !== "GET" && request.method !== "HEAD") {
      init.body = request.body;
    }

    try {
      const response = await fetch(targetUrl, init);

      // Return the response from Railway
      return new Response(response.body, {
        status: response.status,
        statusText: response.statusText,
        headers: response.headers,
      });
    } catch (error) {
      return new Response(
        JSON.stringify({ error: "proxy_error", message: error.message }),
        { status: 502, headers: { "Content-Type": "application/json" } }
      );
    }
  },
};
