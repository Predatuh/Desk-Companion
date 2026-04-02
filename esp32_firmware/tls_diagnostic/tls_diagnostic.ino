/*
 * TLS Diagnostic — flash this ONCE to diagnose relay connectivity.
 * Reads stored WiFi + relay prefs from NVS, connects WiFi,
 * and tries every possible way to reach the relay server.
 * Watch Serial Monitor at 115200 baud for results.
 */
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>

Preferences prefs;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n========== TLS DIAGNOSTIC ==========\n");

  // 1. Read stored credentials
  prefs.begin("desk-cfg", true);
  String ssid  = prefs.getString("ssid", "");
  String pass  = prefs.getString("pass", "");
  String relay = prefs.getString("relay_url", "");
  String token = prefs.getString("device_token", "");
  prefs.end();

  Serial.printf("SSID:  [%s]\n", ssid.c_str());
  Serial.printf("Pass:  [%d chars]\n", pass.length());
  Serial.printf("Relay: [%s]\n", relay.c_str());
  Serial.printf("Token: [%s]\n", token.c_str());
  Serial.printf("Heap:  %u bytes\n\n", ESP.getFreeHeap());

  if (ssid.isEmpty() || pass.isEmpty()) {
    Serial.println("ERROR: No WiFi credentials stored. Flash main firmware first.");
    return;
  }
  if (relay.isEmpty()) {
    Serial.println("ERROR: No relay URL stored.");
    return;
  }

  // 2. Connect WiFi
  Serial.printf("Connecting to WiFi '%s'...\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("WiFi FAILED. Status=%d\n", WiFi.status());
    return;
  }
  Serial.printf("WiFi OK. IP=%s  DNS=%s\n",
    WiFi.localIP().toString().c_str(),
    WiFi.dnsIP().toString().c_str());
  Serial.printf("Heap after WiFi: %u\n\n", ESP.getFreeHeap());

  // Ensure relay URL is https
  String url = relay;
  if (url.startsWith("http://") && url.indexOf(".railway.app") != -1) {
    url = "https://" + url.substring(7);
  }

  // Parse host from URL
  int protoEnd = url.indexOf("://") + 3;
  int pathStart = url.indexOf('/', protoEnd);
  String host = (pathStart > 0) ? url.substring(protoEnd, pathStart) : url.substring(protoEnd);

  // 3. DNS test
  Serial.printf("--- DNS TEST: %s ---\n", host.c_str());
  IPAddress ip;
  for (int i = 0; i < 3; i++) {
    bool ok = WiFi.hostByName(host.c_str(), ip);
    Serial.printf("  Attempt %d: ok=%d  ip=%s\n", i+1, ok, ip.toString().c_str());
    if (ok && ip != IPAddress(0,0,0,0)) break;
    delay(1000);
  }
  if (ip == IPAddress(0,0,0,0)) {
    Serial.println("DNS FAILED — all attempts returned 0.0.0.0");
    return;
  }
  Serial.println();

  // 4. Plain TCP test (port 443)
  Serial.printf("--- PLAIN TCP TEST: %s:443 ---\n", ip.toString().c_str());
  {
    WiFiClient tcp;
    tcp.setTimeout(15);  // 15 seconds
    unsigned long t0 = millis();
    bool ok = tcp.connect(ip, 443);
    unsigned long elapsed = millis() - t0;
    Serial.printf("  Result: %s  Time: %lums\n", ok ? "OK" : "FAIL", elapsed);
    tcp.stop();
  }
  Serial.printf("  Heap: %u\n\n", ESP.getFreeHeap());

  // 5. Raw WiFiClientSecure test (by hostname)
  Serial.printf("--- TLS TEST (by hostname): %s:443 ---\n", host.c_str());
  {
    WiFiClientSecure sc;
    sc.setInsecure();
    sc.setHandshakeTimeout(30);
    unsigned long t0 = millis();
    int ok = sc.connect(host.c_str(), 443);
    unsigned long elapsed = millis() - t0;
    int err = sc.lastError(nullptr, 0);
    Serial.printf("  Result: %d  Time: %lums  lastError: %d\n", ok, elapsed, err);
    if (ok) {
      // Try a simple HTTP request manually
      sc.printf("GET /health HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", host.c_str());
      unsigned long respStart = millis();
      String response = "";
      while (millis() - respStart < 5000) {
        while (sc.available()) {
          response += (char)sc.read();
        }
        if (!sc.connected() && !sc.available()) break;
        delay(10);
      }
      Serial.printf("  Response length: %d\n", response.length());
      // Print first 500 chars
      if (response.length() > 0) {
        Serial.println("  --- Response (first 500 chars) ---");
        Serial.println(response.substring(0, 500));
        Serial.println("  --- End ---");
      }
    }
    sc.stop();
  }
  Serial.printf("  Heap: %u\n\n", ESP.getFreeHeap());

  // 6. Raw WiFiClientSecure test (by IP)
  Serial.printf("--- TLS TEST (by IP): %s:443 ---\n", ip.toString().c_str());
  {
    WiFiClientSecure sc;
    sc.setInsecure();
    sc.setHandshakeTimeout(30);
    unsigned long t0 = millis();
    int ok = sc.connect(ip, 443);
    unsigned long elapsed = millis() - t0;
    int err = sc.lastError(nullptr, 0);
    Serial.printf("  Result: %d  Time: %lums  lastError: %d\n", ok, elapsed, err);
    sc.stop();
  }
  Serial.printf("  Heap: %u\n\n", ESP.getFreeHeap());

  // 7. HTTPClient test with various connect timeouts
  int timeouts[] = {5000, 10000, 15000, 30000};
  for (int i = 0; i < 4; i++) {
    Serial.printf("--- HTTPClient TEST (connectTimeout=%dms) ---\n", timeouts[i]);
    WiFiClientSecure* sc = new WiFiClientSecure();
    sc->setInsecure();
    sc->setHandshakeTimeout(30);
    HTTPClient http;
    String testUrl = url + "/health";
    bool began = http.begin(*sc, testUrl);
    Serial.printf("  begin(%s) = %d\n", testUrl.c_str(), began);
    if (began) {
      http.setReuse(false);
      http.setConnectTimeout(timeouts[i]);
      http.setTimeout(8000);
      http.addHeader("Connection", "close");
      unsigned long t0 = millis();
      int code = http.GET();
      unsigned long elapsed = millis() - t0;
      Serial.printf("  GET result: %d  Time: %lums\n", code, elapsed);
      if (code > 0) {
        String body = http.getString();
        Serial.printf("  Body: %s\n", body.c_str());
      } else {
        Serial.printf("  Error: %s\n", http.errorToString(code).c_str());
      }
      http.end();
    }
    delete sc;
    Serial.printf("  Heap: %u\n\n", ESP.getFreeHeap());
    delay(1000);
  }

  Serial.println("========== DIAGNOSTIC COMPLETE ==========");
}

void loop() {
  delay(60000);
}
