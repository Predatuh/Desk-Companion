// ─── Desk Companion TFT 2.8" (ST7789 240×320, Freenove FNK0104) ───
// Adapted from mini.ino for the ST7789 240×320 IPS TFT with FT6336U capacitive touch.
// Target board: Freenove ESP32-S3 Display (FNK0104) — all-in-one CYD.
// All logic (BLE, Wi-Fi, relay, pet, commands) is identical to the Mini OLED.
// Display: Adafruit_ST7789 + GFXcanvas16 PSRAM framebuffer for flicker-free rendering.

// Firework size params: {particleCount, speedMin, speedMax, heartScale, lifeBase}
struct FwSizeParams { uint8_t count; uint8_t spdMin; uint8_t spdMax; float heartScale; uint8_t lifeBase; };
static const FwSizeParams FW_SIZES[] = {
  {10, 1, 3, 0.25f, 18},  // 0 = small
  {22, 2, 5, 0.4f,  25},  // 1 = medium (original)
  {30, 3, 6, 0.55f, 30},  // 2 = large
  {38, 4, 8, 0.7f,  35},  // 3 = xl
  {46, 5, 10, 0.85f, 40}, // 4 = xxl
};

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_wifi.h>
#include <BLE2902.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <esp_system.h>
#include <ctype.h>
#include <mbedtls/base64.h>
#include <string>
#include <time.h>

// ─── TFT SPI pin configuration (Freenove FNK0104 Touch variant) ───
#define TFT_CS    10
#define TFT_DC    46
#define TFT_MOSI  11
#define TFT_SCK   12
#define TFT_MISO  13
#define TFT_RST   -1   // no dedicated reset
#define TFT_BL    45   // backlight, active HIGH

// ─── FT6336U capacitive touch I2C pins ───
#define TOUCH_SDA  16
#define TOUCH_SCL  15
#define TOUCH_RST  18
#define TOUCH_INT  17

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// Color palette (Adafruit_GFX ST77XX constants)
#define COL_BG       ST77XX_BLACK
#define COL_FG       ST77XX_WHITE
#define COL_ACCENT   ST77XX_CYAN
// Extended RGB565 color palette
#define COL_ROSE     0xFB56  // Hot pink / rose  (255, 106, 176)
#define COL_GOLD     0xFFE0  // Yellow-gold      (255, 224,   0)
#define COL_PINK     0xFDB8  // Soft light pink  (255, 182, 193)
#define COL_MINT     0x07E0  // Fresh mint green (  0, 255,   0)
#define COL_LAVENDER 0x92BD  // Soft purple      (148,  87, 235)
#define COL_PEACH    0xFED6  // Warm peach       (255, 218, 181)
#define COL_SKYBLUE  0x867D  // Sky blue         (135, 206, 235)

// Emoji glyph ids — declared up here (before the first function) so the
// Arduino auto-generated prototype for emojiClassify() can see the type.
enum EmojiId {
  EMO_NONE = 0, EMO_IGNORE, EMO_SMILE, EMO_GRIN, EMO_LAUGH, EMO_HEARTEYES, EMO_KISS,
  EMO_WINK, EMO_COOL, EMO_CRY, EMO_SOB, EMO_ANGRY, EMO_SLEEPY, EMO_THINK, EMO_SURPRISE,
  EMO_NEUTRAL, EMO_HEART, EMO_STAR, EMO_SPARKLE, EMO_FIRE, EMO_PARTY, EMO_THUMBSUP,
  EMO_FLOWER, EMO_SUN, EMO_MOON, EMO_CLOUD, EMO_SNOW, EMO_RAINBOW, EMO_CAKE, EMO_GIFT
};

#ifndef BTN_NEXT_PIN
#define BTN_NEXT_PIN -1  // touch replaces physical buttons
#endif

#ifndef BTN_CLEAR_PIN
#define BTN_CLEAR_PIN -1
#endif

#ifndef BTN_HOLD_MS
#define BTN_HOLD_MS 3000UL
#endif

#ifndef NOTE_QUEUE_MAX
#define NOTE_QUEUE_MAX 5
#endif

#ifndef NOTE_TEXT_MAX
#define NOTE_TEXT_MAX 80
#endif

#ifndef BOOT_WIFI_DELAY_MS
#define BOOT_WIFI_DELAY_MS 4000UL
#endif

#ifndef DESK_COMPANION_DEVICE_NAME
#define DESK_COMPANION_DEVICE_NAME "Desk Companion TFT"
#endif

static const char* DEVICE_NAME = DESK_COMPANION_DEVICE_NAME;
static const char* SERVICE_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f001";
static const char* COMMAND_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f002";
static const char* STATUS_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f003";
static const char* IMAGE_UUID = "63f10c20-d7c4-4bc9-a0e0-5c3b3ad0f004";

Adafruit_ST7789* pTft = nullptr;
// GFXcanvas16 framebuffer — all drawing goes here, then pushed to display in one SPI burst.
GFXcanvas16* canvas = nullptr;
// gfx points to canvas when available, else to tft directly.
Adafruit_GFX* gfx = nullptr;
bool spriteReady = false;
Preferences preferences;
bool displayAvailable = false;
bool touchAvailable = false;

// ─── Inline FT6336U I2C touch driver (no external library needed) ───
#define FT6336U_ADDR 0x38
struct TouchPoint { uint16_t x; uint16_t y; };

bool ft6336u_init() {
  Wire.beginTransmission(FT6336U_ADDR);
  return (Wire.endTransmission() == 0);
}

uint8_t ft6336u_read_reg(uint8_t reg) {
  Wire.beginTransmission(FT6336U_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)FT6336U_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

uint8_t ft6336u_touched() {
  return ft6336u_read_reg(0x02) & 0x0F;  // TD_STATUS register, lower nibble = touch count
}

TouchPoint ft6336u_getPoint() {
  TouchPoint pt;
  Wire.beginTransmission(FT6336U_ADDR);
  Wire.write(0x03);  // start of first touch data
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)FT6336U_ADDR, (uint8_t)4);
  uint8_t xH = Wire.read();
  uint8_t xL = Wire.read();
  uint8_t yH = Wire.read();
  uint8_t yL = Wire.read();
  pt.x = ((xH & 0x0F) << 8) | xL;
  pt.y = ((yH & 0x0F) << 8) | yL;
  return pt;
}

// Convenience: pTft is the hardware display, canvas is the offscreen buffer.
// gfx-> draws to canvas. pushCanvas() blits canvas to display in one SPI burst.
#define tft (*pTft)

inline void pushCanvas() {
  if (!(spriteReady && canvas && pTft)) return;
  // Push the PSRAM framebuffer in blocking horizontal bands. A single full-frame
  // DMA from PSRAM can return before the transfer finishes, so the next frame's
  // fill overwrites the buffer mid-transfer → drifting vertical bars. Banding with
  // block=true guarantees each strip completes before we touch the buffer again.
  uint16_t* buf = canvas->getBuffer();
  const int BAND = 48;  // rows per strip
  pTft->startWrite();
  for (int y = 0; y < SCREEN_HEIGHT; y += BAND) {
    int h = (y + BAND <= SCREEN_HEIGHT) ? BAND : (SCREEN_HEIGHT - y);
    pTft->setAddrWindow(0, y, SCREEN_WIDTH, h);
    pTft->writePixels(buf + (uint32_t)y * SCREEN_WIDTH, (uint32_t)SCREEN_WIDTH * h, true, false);
  }
  pTft->endWrite();
}

// Sub-pixel float offset added by the AA primitives below — drives smooth,
// fractional "breathing/float" of the face. Set during face rendering, reset to 0
// afterwards so backgrounds/particles/flowers are unaffected.
float gFxOffX = 0.0f, gFxOffY = 0.0f;

// ═════════════════════════════════════════════════════════════════════════════
//  FX TOOLKIT — soft, realistic RGB565 rendering on the GFXcanvas16 buffer.
//  These give the companion gradients, anti-aliased edges, glows and alpha
//  blending that plain Adafruit_GFX primitives can't. Everything reads/writes
//  the canvas framebuffer directly (native RGB565) for speed, and degrades to
//  plain pixels when the canvas isn't available (direct-to-TFT fallback).
//  Tuned for the single target board: Freenove ESP32-S3 320×240 ST7789.
// ═════════════════════════════════════════════════════════════════════════════

// Pack 8-bit RGB into RGB565.
static inline uint16_t fxRGB(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
// Expand RGB565 back to 8-bit channels (with low-bit replication for accuracy).
static inline void fxUnpack(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
  r = (uint8_t)((c >> 8) & 0xF8); r |= r >> 5;
  g = (uint8_t)((c >> 3) & 0xFC); g |= g >> 6;
  b = (uint8_t)((c << 3) & 0xF8); b |= b >> 5;
}
// Linear blend of two RGB565 colors; t=0→a, t=255→b.
static inline uint16_t fxMix(uint16_t a, uint16_t b, uint8_t t) {
  if (t == 0) return a;
  if (t >= 255) return b;
  uint8_t ar, ag, ab, br, bg, bb;
  fxUnpack(a, ar, ag, ab); fxUnpack(b, br, bg, bb);
  return fxRGB((uint8_t)(ar + (((int)br - ar) * t) / 255),
               (uint8_t)(ag + (((int)bg - ag) * t) / 255),
               (uint8_t)(ab + (((int)bb - ab) * t) / 255));
}
// Brighten/darken a color toward white/black by amount 0..255.
static inline uint16_t fxLighten(uint16_t c, uint8_t amt) { return fxMix(c, 0xFFFF, amt); }
static inline uint16_t fxDarken(uint16_t c, uint8_t amt)  { return fxMix(c, 0x0000, amt); }

static inline float fxClampf01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static inline float fxClampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static inline uint16_t* fxBuf() { return (spriteReady && canvas) ? canvas->getBuffer() : nullptr; }

// Alpha-blend one pixel onto the framebuffer (alpha 0..255).
static inline void fxBlend(int x, int y, uint16_t color, uint8_t alpha) {
  if ((unsigned)x >= (unsigned)SCREEN_WIDTH || (unsigned)y >= (unsigned)SCREEN_HEIGHT) return;
  if (alpha == 0) return;
  uint16_t* fb = fxBuf();
  if (!fb) { if (alpha > 110 && gfx) gfx->drawPixel(x, y, color); return; }
  uint16_t* p = &fb[y * SCREEN_WIDTH + x];
  *p = (alpha >= 255) ? color : fxMix(*p, color, alpha);
}
static inline void fxSet(int x, int y, uint16_t color) {
  if ((unsigned)x >= (unsigned)SCREEN_WIDTH || (unsigned)y >= (unsigned)SCREEN_HEIGHT) return;
  uint16_t* fb = fxBuf();
  if (!fb) { if (gfx) gfx->drawPixel(x, y, color); return; }
  fb[y * SCREEN_WIDTH + x] = color;
}

// Anti-aliased filled disc (smooth 1px feathered edge).
static void fxDiscAA(float cx, float cy, float r, uint16_t color, uint8_t alpha = 255) {
  if (r < 0.4f) return;
  cx += gFxOffX; cy += gFxOffY;
  int x0 = (int)floorf(cx - r - 1.0f), x1 = (int)ceilf(cx + r + 1.0f);
  int y0 = (int)floorf(cy - r - 1.0f), y1 = (int)ceilf(cy + r + 1.0f);
  if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
  if (x1 >= SCREEN_WIDTH) x1 = SCREEN_WIDTH - 1;
  if (y1 >= SCREEN_HEIGHT) y1 = SCREEN_HEIGHT - 1;
  const float rOut2 = (r + 0.5f) * (r + 0.5f);              // beyond → skip
  const float rIn = r - 0.5f, rIn2 = rIn > 0.0f ? rIn * rIn : -1.0f; // inside → solid, no sqrt
  for (int y = y0; y <= y1; y++) {
    float dy = (float)y - cy, dy2 = dy * dy;
    for (int x = x0; x <= x1; x++) {
      float dx = (float)x - cx;
      float d2 = dx * dx + dy2;
      if (d2 >= rOut2) continue;
      uint8_t a;
      if (d2 <= rIn2) a = 255;
      else { float cov = r - sqrtf(d2); a = cov >= 0.5f ? 255 : (uint8_t)((cov + 0.5f) * 255.0f); }
      fxBlend(x, y, color, alpha == 255 ? a : (uint8_t)(((uint16_t)a * alpha) / 255));
    }
  }
}

// Radial-gradient disc: inner color at center → outer color at rim, AA edge.
static void fxDiscRadial(float cx, float cy, float r, uint16_t inner, uint16_t outer, uint8_t alpha = 255) {
  if (r < 0.4f) return;
  cx += gFxOffX; cy += gFxOffY;
  int x0 = (int)floorf(cx - r - 1.0f), x1 = (int)ceilf(cx + r + 1.0f);
  int y0 = (int)floorf(cy - r - 1.0f), y1 = (int)ceilf(cy + r + 1.0f);
  if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
  if (x1 >= SCREEN_WIDTH) x1 = SCREEN_WIDTH - 1;
  if (y1 >= SCREEN_HEIGHT) y1 = SCREEN_HEIGHT - 1;
  const float r2 = r * r;
  const float rOut2 = (r + 0.5f) * (r + 0.5f);
  const float rIn = r - 0.5f, rIn2 = rIn > 0.0f ? rIn * rIn : -1.0f;
  for (int y = y0; y <= y1; y++) {
    float dy = (float)y - cy, dy2 = dy * dy;
    for (int x = x0; x <= x1; x++) {
      float dx = (float)x - cx;
      float d2 = dx * dx + dy2;
      if (d2 >= rOut2) continue;
      uint8_t a;
      if (d2 <= rIn2) a = 255;
      else { float cov = r - sqrtf(d2); a = cov >= 0.5f ? 255 : (uint8_t)((cov + 0.5f) * 255.0f); }
      uint8_t tt = (uint8_t)(fxClampf01(d2 / r2) * 255.0f);   // d²-based ramp avoids a sqrt
      fxBlend(x, y, fxMix(inner, outer, tt), alpha == 255 ? a : (uint8_t)(((uint16_t)a * alpha) / 255));
    }
  }
}

// Soft radial glow that fades to transparent at the rim (great over any bg).
// Uses squared-distance falloff — no per-pixel sqrt, cheap enough for every frame.
static void fxGlow(float cx, float cy, float r, uint16_t color, uint8_t maxAlpha) {
  if (r < 0.5f) return;
  cx += gFxOffX; cy += gFxOffY;
  float r2 = r * r;
  int x0 = (int)floorf(cx - r), x1 = (int)ceilf(cx + r);
  int y0 = (int)floorf(cy - r), y1 = (int)ceilf(cy + r);
  if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
  if (x1 >= SCREEN_WIDTH) x1 = SCREEN_WIDTH - 1;
  if (y1 >= SCREEN_HEIGHT) y1 = SCREEN_HEIGHT - 1;
  for (int y = y0; y <= y1; y++) {
    float dy = (float)y - cy;
    for (int x = x0; x <= x1; x++) {
      float dx = (float)x - cx;
      float d2 = dx * dx + dy * dy;
      if (d2 >= r2) continue;
      float f = 1.0f - d2 / r2;             // quadratic falloff
      fxBlend(x, y, color, (uint8_t)(f * maxAlpha));
    }
  }
}

// Anti-aliased filled triangle (edge-distance coverage). Honors the float offset
// so triangle-based flower petals sway sub-pixel-smoothly with everything else.
static void fxFillTriAA(float ax, float ay, float bx, float by, float cx2, float cy2,
                        uint16_t color, uint8_t alpha = 255) {
  ax += gFxOffX; ay += gFxOffY; bx += gFxOffX; by += gFxOffY; cx2 += gFxOffX; cy2 += gFxOffY;
  int x0 = (int)floorf(fminf(ax, fminf(bx, cx2)) - 1.0f);
  int x1 = (int)ceilf(fmaxf(ax, fmaxf(bx, cx2)) + 1.0f);
  int y0 = (int)floorf(fminf(ay, fminf(by, cy2)) - 1.0f);
  int y1 = (int)ceilf(fmaxf(ay, fmaxf(by, cy2)) + 1.0f);
  const float area = (bx - ax) * (cy2 - ay) - (by - ay) * (cx2 - ax);
  if (fabsf(area) < 0.01f) return;
  const float s = area < 0 ? -1.0f : 1.0f;
  const float l0 = fmaxf(0.001f, sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay)));
  const float l1 = fmaxf(0.001f, sqrtf((cx2 - bx) * (cx2 - bx) + (cy2 - by) * (cy2 - by)));
  const float l2 = fmaxf(0.001f, sqrtf((ax - cx2) * (ax - cx2) + (ay - cy2) * (ay - cy2)));
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      float d0 = (((bx - ax) * (y - ay) - (by - ay) * (x - ax)) * s) / l0;
      float d1 = (((cx2 - bx) * (y - by) - (cy2 - by) * (x - bx)) * s) / l1;
      float d2 = (((ax - cx2) * (y - cy2) - (ay - cy2) * (x - cx2)) * s) / l2;
      float md = fminf(d0, fminf(d1, d2));   // signed distance to nearest edge (>=0 inside)
      if (md <= -0.5f) continue;
      uint8_t a = md >= 0.5f ? 255 : (uint8_t)((md + 0.5f) * 255.0f);
      fxBlend(x, y, color, (uint8_t)(((uint16_t)a * alpha) / 255));
    }
  }
}

// Anti-aliased filled ellipse.
static void fxEllipseAA(float cx, float cy, float rx, float ry, uint16_t color, uint8_t alpha = 255) {
  if (rx < 0.4f || ry < 0.4f) return;
  cx += gFxOffX; cy += gFxOffY;
  int x0 = (int)floorf(cx - rx - 1.0f), x1 = (int)ceilf(cx + rx + 1.0f);
  int y0 = (int)floorf(cy - ry - 1.0f), y1 = (int)ceilf(cy + ry + 1.0f);
  if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
  if (x1 >= SCREEN_WIDTH) x1 = SCREEN_WIDTH - 1;
  if (y1 >= SCREEN_HEIGHT) y1 = SCREEN_HEIGHT - 1;
  const float rmean = (rx + ry) * 0.5f;
  const float invrx = 1.0f / rx, invry = 1.0f / ry;
  // Normalized radius² thresholds: solid below rIn², empty above rOut².
  const float rOutN = 1.0f + 0.5f / rmean, rOutN2 = rOutN * rOutN;
  const float rInN = 1.0f - 0.5f / rmean, rInN2 = rInN > 0.0f ? rInN * rInN : -1.0f;
  for (int y = y0; y <= y1; y++) {
    float ny = ((float)y - cy) * invry, ny2 = ny * ny;
    for (int x = x0; x <= x1; x++) {
      float nx = ((float)x - cx) * invrx;
      float n2 = nx * nx + ny2;
      if (n2 >= rOutN2) continue;
      uint8_t a;
      if (n2 <= rInN2) a = 255;
      else { float cov = (1.0f - sqrtf(n2)) * rmean; a = cov >= 0.5f ? 255 : (uint8_t)((cov + 0.5f) * 255.0f); }
      fxBlend(x, y, color, alpha == 255 ? a : (uint8_t)(((uint16_t)a * alpha) / 255));
    }
  }
}

// Anti-aliased ring/annulus (great for glasses lenses, halos, hat bands).
static void fxRingAA(float cx, float cy, float r, float thick, uint16_t color, uint8_t alpha = 255) {
  if (r < 0.5f) return;
  cx += gFxOffX; cy += gFxOffY;
  float half = thick * 0.5f;
  int x0 = (int)floorf(cx - r - half - 1.0f), x1 = (int)ceilf(cx + r + half + 1.0f);
  int y0 = (int)floorf(cy - r - half - 1.0f), y1 = (int)ceilf(cy + r + half + 1.0f);
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      float dx = (float)x - cx, dy = (float)y - cy;
      float cov = half - fabsf(sqrtf(dx * dx + dy * dy) - r);
      if (cov <= -0.5f) continue;
      uint8_t a = cov >= 0.5f ? 255 : (uint8_t)((cov + 0.5f) * 255.0f);
      fxBlend(x, y, color, (uint8_t)(((uint16_t)a * alpha) / 255));
    }
  }
}

// Anti-aliased thick line / capsule (round caps) — used for mouths, brows, stems.
static void fxThickLine(float x0, float y0, float x1, float y1, float thick, uint16_t color, uint8_t alpha = 255) {
  x0 += gFxOffX; y0 += gFxOffY; x1 += gFxOffX; y1 += gFxOffY;
  float hw = thick * 0.5f;
  int bx0 = (int)floorf(fminf(x0, x1) - hw - 1.0f), bx1 = (int)ceilf(fmaxf(x0, x1) + hw + 1.0f);
  int by0 = (int)floorf(fminf(y0, y1) - hw - 1.0f), by1 = (int)ceilf(fmaxf(y0, y1) + hw + 1.0f);
  float vx = x1 - x0, vy = y1 - y0;
  float len2 = vx * vx + vy * vy;
  for (int y = by0; y <= by1; y++) {
    for (int x = bx0; x <= bx1; x++) {
      float px = (float)x - x0, py = (float)y - y0;
      float t = len2 > 0.0001f ? fxClampf01((px * vx + py * vy) / len2) : 0.0f;
      float dx = px - vx * t, dy = py - vy * t;
      float cov = hw - sqrtf(dx * dx + dy * dy);
      if (cov <= -0.5f) continue;
      uint8_t a = cov >= 0.5f ? 255 : (uint8_t)((cov + 0.5f) * 255.0f);
      fxBlend(x, y, color, (uint8_t)(((uint16_t)a * alpha) / 255));
    }
  }
}

// Fast vertical-gradient rectangle (one color per row).
static void fxVGradient(int x, int y, int w, int h, uint16_t top, uint16_t bottom) {
  if (w <= 0 || h <= 0) return;
  uint16_t* fb = fxBuf();
  for (int j = 0; j < h; j++) {
    int yy = y + j;
    if ((unsigned)yy >= (unsigned)SCREEN_HEIGHT) continue;
    uint8_t t = (h <= 1) ? 0 : (uint8_t)((j * 255) / (h - 1));
    uint16_t c = fxMix(top, bottom, t);
    if (fb) {
      int xs = x < 0 ? 0 : x;
      int xe = (x + w > SCREEN_WIDTH) ? SCREEN_WIDTH : x + w;
      uint16_t* row = &fb[yy * SCREEN_WIDTH];
      for (int xx = xs; xx < xe; xx++) row[xx] = c;
    } else if (gfx) {
      gfx->drawFastHLine(x, yy, w, c);
    }
  }
}
static inline void fxScreenGradient(uint16_t top, uint16_t bottom) {
  fxVGradient(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, top, bottom);
}

// HSV (h 0..360, s/v 0..1) → RGB565. Handy for rainbows and twinkles.
static uint16_t fxHSV(float h, float s, float v) {
  h = fmodf(h, 360.0f); if (h < 0) h += 360.0f;
  float c = v * s, x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f)), m = v - c;
  float r, g, b;
  if      (h < 60)  { r = c; g = x; b = 0; }
  else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; }
  else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; }
  else              { r = c; g = 0; b = x; }
  return fxRGB((uint8_t)((r + m) * 255), (uint8_t)((g + m) * 255), (uint8_t)((b + m) * 255));
}

// Easing helpers for fluid, organic motion.
static inline float fxLerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float easeInOutSine(float t) { return -(cosf(3.14159265f * t) - 1.0f) * 0.5f; }
static inline float easeOutCubic(float t)  { float u = 1.0f - t; return 1.0f - u * u * u; }
static inline float easeInOutCubic(float t){ return t < 0.5f ? 4.0f*t*t*t : 1.0f - powf(-2.0f*t+2.0f,3.0f)*0.5f; }
static inline float easeOutBack(float t)   { const float c1 = 1.70158f, c3 = c1 + 1.0f; float u = t - 1.0f; return 1.0f + c3*u*u*u + c1*u*u; }
static inline float easeOutElastic(float t){ if (t <= 0) return 0; if (t >= 1) return 1; const float c4 = (2.0f*3.14159265f)/3.0f; return powf(2.0f,-10.0f*t)*sinf((t*10.0f-0.75f)*c4)+1.0f; }
// Smooth 0→1→0 pulse over a normalized phase t.
static inline float fxPulse(float t) { return sinf(t * 3.14159265f * 2.0f) * 0.5f + 0.5f; }

// Smooth parabolic mouth / eyebrow / arc curve. curve>0 bows downward (smile),
// curve<0 bows upward (frown / happy-arc eye). Anti-aliased with round caps.
static void drawMouthCurve(float cx, float cy, float w, float curve, float thick, uint16_t color, uint8_t alpha = 255) {
  const int N = 16;
  float hw = w * 0.5f;
  float px = 0, py = 0;
  for (int i = 0; i <= N; i++) {
    float t = (float)i / N;
    float u = 2.0f * t - 1.0f;            // -1..1
    float x = cx - hw + t * w;
    float y = cy + curve * (1.0f - u * u);
    if (i > 0) fxThickLine(px, py, x, y, thick, color, alpha);
    px = x; py = y;
  }
}

BLEServer* bleServer = nullptr;
BLECharacteristic* commandCharacteristic = nullptr;
BLECharacteristic* statusCharacteristic = nullptr;
BLECharacteristic* imageCharacteristic = nullptr;
static unsigned long lastBleAdvertRestartMs = 0;

enum DisplayMode {
  MODE_IDLE,
  MODE_NOTE,
  MODE_BANNER,
  MODE_IMAGE,
  MODE_COLOR_IMAGE,
  MODE_EXPRESSION,
  MODE_FLOWER,
  MODE_FIREWORKS,
  MODE_HEART_RAIN,
  MODE_SNOWFALL,
  MODE_STARFIELD,
  MODE_AURORA,
  MODE_FIREFLIES,
  MODE_FALLING_LEAVES,
  MODE_STORM,
  MODE_TORNADO,
  MODE_POMODORO,
  MODE_COUNTDOWN,
  MODE_WEATHER,
  MODE_SLEEP,
  MODE_ANIMATED_NOTE,
  MODE_MENU,
  MODE_CONFIRM_CLEAR,
  MODE_FIRECRACKER,
};

DisplayMode currentMode = MODE_IDLE;
String statusText = "Booting";
String currentFlower = "rose";
String currentScene  = "wave";
String currentNote = "hi honey";
String currentBanner = "hello from your desk buddy";
String currentExpression = "happy";
String currentSsid = "";
String ipAddress = "";
String relayUrl = "";
String deviceToken = "";
String petPersonality = "curious";
String activePetMode = "off";  // default off – no autonomous face animations
String activeCareAction = "";
String companionHair = "none";
String companionEars = "none";
String companionMustache = "none";
String companionGlasses = "none";
String companionHeadwear = "none";
String companionPiercing = "none";
String flowerArrangement = "single";
// User-configurable display colors (RGB565, saved to flash)
uint16_t userEyeColor    = COL_FG;       // eye fill
uint16_t userFaceColor   = COL_FG;       // face border / mouth / outline
uint16_t userAccentColor = COL_ACCENT;   // clock text, highlight elements
uint16_t userBodyColor   = COL_ROSE;     // stick-figure second body, cheek blush
uint16_t userHairColor   = COL_FG;       // hair / headwear
uint16_t userHatColor    = COL_FG;       // hat / headwear (separate from hair)
uint16_t userMustacheColor = COL_FG;     // mustache / facial hair
uint16_t userMouthColor  = COL_FG;       // mouth / lips
uint16_t flowerPetalColor = COL_ROSE;
uint16_t flowerCenterColor = COL_GOLD;
uint16_t flowerStemColor = COL_MINT;
int flowerCount = 1;
int flowerSize = 100;
bool flowerMixed = false;
int companionHeadwearSize = 100;
int companionHeadwearWidth = 100;
int companionHeadwearHeight = 100;
int companionHeadwearOffsetX = 0;
int companionHeadwearOffsetY = 0;
int companionHairSize = 100;
int companionMustacheSize = 100;
int companionHairWidth = 100;
int companionHairHeight = 100;
int companionHairThickness = 100;
int companionHairOffsetX = 0;
int companionHairOffsetY = 0;
int companionEyeOffsetX = 0;
int companionEyeOffsetY = 0;
int companionMouthOffsetX = 0;
int companionMouthOffsetY = 0;
int companionOffsetX = 0;
int companionOffsetY = 0;
int companionMustacheWidth = 100;
int companionMustacheHeight = 100;
int companionMustacheThickness = 100;
int companionMustacheOffsetX = 0;
int companionMustacheOffsetY = 0;
String currentNoteFlowerAccent = "";
int currentNoteFontSize = 1;
int currentNoteBorder = 0;
String currentNoteIcons = "";
uint16_t currentNoteTextColor = COL_FG;   // note text color (default white)
uint8_t currentNoteFontStyle = 0;   // 0 regular, 1 bold, 2 outline, 3 shadow
int currentNoteTextOffX = 0;   // horizontal nudge of the note text block (px, +right)
int currentNoteTextOffY = 0;   // vertical nudge of the note text block (px, +down)
int bannerSpeed = 35;
int bannerOffset = SCREEN_WIDTH;
unsigned long lastBannerTickMs = 0;
unsigned long lastDecorTickMs = 0;
unsigned long lastExpressionTickMs = 0;
unsigned long lastRelayPollMs = 0;
unsigned long lastRelayStatusPushMs = 0;
unsigned long lastRelaySuccessMs = 0;
unsigned long lastWifiCheckMs = 0;
unsigned long lastWifiBeginMs = 0;
unsigned long lastCompanionTickMs = 0;
unsigned long lastPetBeatMs = 0;
bool wifiJoinActive = false;
bool relayStatusDirty = true;
bool relayPreferPoll = true;

// ─── Firebase / Firestore relay config (provisioned via BLE, persisted to NVS) ──
// deviceToken (declared above) is reused as the Firestore document id: devices/{deviceToken}.
String fbProjectId      = "";
String fbApiKey         = "";
String fbDeviceEmail    = "";
String fbDevicePassword = "";
// Auth token state (Firebase Identity Toolkit).
String fbIdToken        = "";
String fbRefreshToken   = "";
unsigned long fbTokenAcquiredMs = 0;   // millis() when the id token was obtained
unsigned long fbTokenTtlMs      = 0;   // token lifetime in ms (expiresIn * 1000)
long fbLastCmdId        = -1;          // last applied cmdId from Firestore (latest-wins)
bool fbConfigured() {
  return fbProjectId.length() && fbApiKey.length() && fbDeviceEmail.length() &&
         fbDevicePassword.length() && deviceToken.length();
}

// ─── Async HTTPS task (runs on core 0, never blocks the render loop) ────
// Generic request descriptor so the one task can do Firebase auth + Firestore.
enum FbReqKind : uint8_t { FB_NONE = 0, FB_SIGNIN, FB_REFRESH, FB_POLL, FB_STATUS };
static TaskHandle_t s_relayTaskHandle  = nullptr;
static volatile bool s_relayBusy        = false;  // task is executing HTTP
static volatile bool s_relayResultReady = false;  // task has a result to process
static volatile int  s_relayResCode     = 0;
static volatile uint8_t s_reqKind       = FB_NONE;
static char s_reqMethod[8];
static char s_reqUrl[640];
static char s_reqBody[1200];
static char s_reqAuth[1400];    // bearer id-token (Firebase id tokens run ~900-1200 chars)
static char s_reqCtype[40];
static char s_relayResBody[3584];  // sign-in response carries idToken + refreshToken (~2.8 KB)
uint8_t idleOrbit = 0;
uint16_t rainDropOffset = 0;
float gTornadoX = SCREEN_WIDTH / 2;   // tornado funnel x; drifts back and forth
float gTornadoVX = 1.3f;
uint8_t gTornadoObject = 0;           // flung object: 0 none, 1 cow, 2 house
bool gTornadoTrees = false;           // planted bending trees (independent of the flung object)
uint8_t expressionPhase = 0;
// "Alive" autonomous idle behaviors — the home face occasionally does little
// things on its own (yawn, look around, glance at the clock, blink flurry, doze).
uint8_t gIdleBehavior = 0;            // 0 none, 1 yawn, 2 lookAround, 3 glanceClock, 4 blinkFlurry, 5 doze
unsigned long gIdleBehaviorStartMs = 0;
unsigned long gIdleBehaviorEndMs = 0;
unsigned long gNextIdleBehaviorMs = 0;
// Global gaze drift added to every eye so the companion always feels alive.
float gEyeDriftX = 0.0f, gEyeDriftY = 0.0f;
uint8_t petCycleStep = 0;
String availableWifiNetworks[10];
int availableWifiNetworkCount = 0;
DisplayMode transientResumeMode = MODE_IDLE;
String transientResumeStatus = "Ready";
String transientResumeExpression = "happy";
String transientResumeFlower = "rose";
bool transientActive = false;
unsigned long transientEndsAt = 0;
unsigned long noteDisplayEndsMs = 0;  // auto-expire note back to idle
int bondLevel = 50;
int energyLevel = 72;
int boredomLevel = 28;

bool wifiConnectPending = false;
String pendingWifiSsid = "";
String pendingWifiPass = "";

// BLE commands are queued here and processed in loop() to avoid running
// heavy operations (HTTPS, NVS, etc.) inside the BLE task's limited stack.
volatile bool bleCommandPending = false;
String pendingBleCommand = "";
bool wifiScanPending = false;
bool wifiWasConnected = false;
String storedWifiPass = "";
bool bootWifiRestorePending = false;
unsigned long bootCompletedAtMs = 0;

// Image buffer for 240x320 monochrome bitmap (9600 bytes)
uint8_t imageBuffer[SCREEN_WIDTH * SCREEN_HEIGHT / 8] = {0};
size_t expectedImageBytes = 0;
size_t receivedImageBytes = 0;
bool imageTransferActive = false;

// Color image buffer (RGB565, 153600 bytes, PSRAM-backed)
uint16_t* colorImageBuffer = nullptr;
uint16_t* idleBackgroundBuffer = nullptr;
size_t expectedColorBytes = 0;
size_t receivedColorBytes = 0;
bool colorImageTransferActive = false;
bool colorImageTransferForIdleBackground = false;

// Touch state for virtual buttons
bool touchActive = false;
bool holdFired  = false;
unsigned long touchStartMs = 0;
int touchStartX = 0;
int touchStartY = 0;
int touchRawX = 0;
int touchRawY = 0;
int lastMenuTapX = -1;
int lastMenuTapY = -1;
#define MENU_OPEN_COOLDOWN_MS 400
#define TOUCH_MIN_HOLD_MS     60

// Menu state
uint8_t menuPage = 0;
unsigned long menuOpenedMs = 0;
#define MENU_TIMEOUT_MS 10000UL
#define MENU_PAGES 4
DisplayMode menuResumeMode = MODE_IDLE;

// Weather
float weatherLat = 0.f;
float weatherLon = 0.f;
int   weatherCode       = -1;   // WMO code; -1 = no data
int   weatherTempTenths = 0;    // temp_2m * 10, e.g. 215 = 21.5 C
String weatherStatusText = "";
unsigned long lastWeatherFetchMs = 0;
// Forecast strip (next few hours + sunrise/sunset)
int forecastTempF[4] = { 0, 0, 0, 0 };
int forecastCode[4]  = { -1, -1, -1, -1 };
int forecastHourLbl[4] = { 0, 0, 0, 0 };   // hour-of-day label for each slot
uint8_t forecastCount = 0;
char sunriseStr[6] = "";                   // "HH:MM"
char sunsetStr[6]  = "";
bool idleShowForecast = false;
// Touch double-tap
unsigned long lastTapReleaseMs = 0;
// Daily greetings (Phase 9)
int  lastGreetingDay   = -1;
int   lastEveningDay    = -1;
unsigned long lastTimeCheckMs       = 0;
unsigned long lastIdleInteractionMs = 0;
// Emoji note reaction state
String lastNoteEmoji = "";
unsigned long emojiReactionEndsMs = 0;

// Particle system
struct Ptcl { int16_t x, y; int8_t vx, vy; uint8_t life; uint16_t color; };
Ptcl gPtcl[48];
uint8_t  gPtclCount        = 0;
unsigned long lastParticleTickMs = 0;
// Firework shape: 0=circle, 1=heart, 2=star
uint8_t fireworkShape = 0;
// Firework color palette: 0=rainbow, 1=warm, 2=cool, 3=mono
uint8_t fireworkPalette = 0;
// Firework size: 0=small, 1=medium, 2=large, 3=xl, 4=xxl, 5=random
uint8_t fireworkSize = 1;
// When true, fireworks only launch via manual fire_rocket commands (no auto-relaunch)
bool fwManualOnly = false;
// Multi-stage firework bursts: 1=single, 2=double, 3=triple
uint8_t fireworkStages = 1;
// Burst flash (bright bloom at the moment of explosion).
int16_t fwFlashX = 0, fwFlashY = 0;
uint8_t fwFlashLife = 0;
uint16_t fwFlashColor = COL_GOLD;
// Crackle effect toggle (staggered secondary pops + sparkle).
bool fireworkCrackle = true;
// Firecracker fuse
unsigned long firecrackerStartMs = 0;
unsigned long firecrackerDurationMs = 5000;
bool firecrackerExploded = false;
String firecrackerWord = "BOOM!";
bool firecrackerShowCountdown = true;
uint8_t firecrackerCount = 1;
unsigned long firecrackerWordDurationMs = 3000;  // how long word stays on screen
unsigned long firecrackerExplodeMs = 0;          // millis() when explosion started
// Note animation: 0=none, 1=flowing_water, 2=shooting_stars, 3=growing_flowers (original),
// 4=fireworks, 5=snowfall, 6=starfield, 7=blooming_garden (new realistic flowers)
uint8_t noteAnimType = 0;
bool animatedNoteUsesTextBackground = false;
Ptcl noteOvPtcl[24];
uint8_t noteOvCount = 0;
// Countdown end action: 0=fireworks, 1=heart_rain, 2=snowfall, 3=starfield
uint8_t countdownEndAction = 0;
// Pomodoro focus timer
unsigned long pomoStartMs = 0;
unsigned long pomoPhaseDurMs = 0;
unsigned long lastPomoTickMs = 0;
uint8_t pomoPhase = 0;        // 0 = focus, 1 = break
uint8_t pomoWorkMin = 25, pomoBreakMin = 5;
uint8_t pomoRounds = 4, pomoRoundDone = 0;
// Expression speed multiplier (1=slow, 2=normal, 4=fast, 8=super fast)
uint8_t expressionSpeedMul = 1;
// Companion scale: 10-300 percent
uint16_t companionScale = 100;
// Idle screen toggles (NVS-persisted)
bool idleShowClock   = true;
bool idleShowWeather = true;
bool idleShowFace    = true;
bool idleShowWifi    = true;
bool idleClock12Hour = false;
bool idleUseBackgroundImage = false;
String idleExpression = "neutral";   // home-screen face mood (neutral/happy/love/sleepy/wink/cool/excited)
uint32_t anniversaryEpoch = 0;       // unix seconds of "day 1"; 0 = off. Shows "N days" on home.
uint8_t idleClockFace = 0;           // 0 digital, 1 analog, 2 word — used as centerpiece when face is off
uint8_t idleBottomLine = 0;          // bottom info line: 0 none, 1 sunrise/sunset, 2 affirmation
uint8_t idleAffirmationSize = 2;     // affirmation text size: 1 small, 2 medium, 3 large
String idleStatusSign = "";          // desk placard text (empty = off), e.g. "FOCUSED"
int8_t weatherTextSize = 1;  // 1=small 2=medium 3=large
// Display brightness: 0-255 (PWM on TFT_BL pin)
uint8_t displayBrightness = 128;
// Countdown
long     countdownSeconds  = 0;
unsigned long countdownStartMs  = 0;
bool     countdownExpired  = false;
// NTP
int      ntpUtcOffsetSeconds = 0;

// Note queue
String noteQueue[NOTE_QUEUE_MAX];
int noteFontSizeQueue[NOTE_QUEUE_MAX] = {0};
int noteQueueCount = 0;
int noteQueueIndex = 0;

// Forward declarations
void relayBgTask(void*);
const char* modeName(DisplayMode mode);
int relayRequest(const char* method, const String& url, const String& body, String& response);
void clearImageBuffer();
bool decodeBase64IntoImage(const String& input);
bool appendBase64ColorChunk(const String& input);
bool decodeBase64IntoColorImage(const String& input);
bool relayColorTransferActive();
void publishStatus();
void publishStatusWithNetworks();
String buildSlimStatusJson();
void drawWrappedText(const String& text, int fontSize, int border, const String& icons, bool pushToScreen = true);
void renderBannerFrame();
void renderExpressionFrame();
void renderImage();
void renderColorImage();
void renderIdle();
void renderCurrentMode();
void drawFireworkParticle(int16_t x, int16_t y, int r, uint16_t c);
void renderAnimatedNoteFrame();
void drawCurrentNoteBackground(bool pushToScreen = true);
void initNoteOverlay();
String petDisplayLabel(const String& value);
String normalizePetPersonality(const String& value);
String normalizePetMode(const String& value);
String normalizeCareAction(const String& value);
String normalizeCompanionHair(const String& value);
String normalizeCompanionEars(const String& value);
String normalizeCompanionMustache(const String& value);
String normalizeCompanionGlasses(const String& value);
String normalizeCompanionHeadwear(const String& value);
String normalizeCompanionPiercing(const String& value);
String petAmbientStatus();
int clampLevel(int value);
int clampAppearancePercent(int value);
int clampAppearanceOffset(int value);
int clampCompanionScale(int value);
int scaleByPercent(int base, int percent);
String activePetBehavior();
String pickAutonomousPetExpression();
String pickReactionExpression(const String& trigger);
String currentAttentionStatus();
void persistPetState();
void setPetPersonality(const String& personality, bool persist = true);
void triggerPetMode(const String& petMode, bool persist = true);
void sendCareAction(const String& action);
void startTransientExpression(const String& expression, unsigned long durationMs, const String& nextStatus);
void restoreTransientMode();
void updateCompanionNeeds();
void updatePetBehavior();
void setIdleStatus(const String& value);
void setExpression(const String& expression);
void setNote(const String& text, int fontSize, int border, const String& icons, const String& flowerAccent = "");
void setBanner(const String& text, int speed);
void setImageReady();
void clearSavedNote();
void setFlower(const String& flowerType, int count = 1, int size = 100, const String& arrangement = "single", bool mixed = false, int petalColor = -1, int centerColor = -1, int stemColor = -1);
void saveFirebaseSettings(const String& projectId, const String& apiKey, const String& deviceEmail, const String& devicePassword, const String& deviceId);
bool connectToWifi(const String& ssid, const String& password);
bool wifiJoinInProgress();
void markWifiJoinStarted();
void markWifiJoinFinished();
void scanWifiNetworks();
void drawEye(int cx, int cy, int w, int h, int r, int pupilDx, int pupilDy);
void drawBlinkEye(int cx, int cy, int w, int h, int r);
void drawHappyArc(int cx, int cy, int w);
void drawSadArc(int cx, int cy, int w);
void drawSmile(int cx, int cy, int w);
void drawOvalMouth(int cx, int cy, int rw, int rh);
void drawIconHeart(int cx, int cy, int s);
void drawVeinMark(int cx, int cy, int s);
void drawTeethMouth(int cx, int cy, int w, int h);
void drawHeartEye(int cx, int cy, int s);
void drawSteamPuff(int cx, int cy, int r);
void drawCompanionAccessories(int leftX, int rightX, int eyeY, int mouthY);
void renderFlowerFrame();
void drawNoteWithFlowerAccent(const String& text, int fontSize, int border, const String& icons, const String& flowerType, bool pushToScreen = true);
void tryStoredPrefs();
void handleCommandJson(const String& body);
void pushRelayStatus();
void pollRelay();
void setupBle();
void configureWifiStaMode();
bool isRelayBackgroundMode();
unsigned long relayPollIntervalMs();
unsigned long relayStatusIntervalMs();

void configureWifiStaMode() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
}

bool isRelayBackgroundMode() {
  return currentMode == MODE_EXPRESSION ||
  currentMode == MODE_FLOWER ||
  currentMode == MODE_SLEEP ||
  currentMode == MODE_FIREWORKS ||
  currentMode == MODE_FIRECRACKER ||
  currentMode == MODE_HEART_RAIN ||
  currentMode == MODE_SNOWFALL ||
  currentMode == MODE_STARFIELD ||
  currentMode == MODE_AURORA ||
  currentMode == MODE_FIREFLIES ||
  currentMode == MODE_FALLING_LEAVES ||
  currentMode == MODE_STORM ||
  currentMode == MODE_TORNADO ||
  currentMode == MODE_ANIMATED_NOTE ||
  currentMode == MODE_COUNTDOWN ||
  currentMode == MODE_POMODORO;
}

bool isAnyAnimatedMode() {
  return currentMode == MODE_FIREWORKS ||
  currentMode == MODE_EXPRESSION ||
  currentMode == MODE_FLOWER ||
  currentMode == MODE_SLEEP ||
      currentMode == MODE_FIRECRACKER ||
      currentMode == MODE_HEART_RAIN ||
      currentMode == MODE_SNOWFALL ||
      currentMode == MODE_STARFIELD ||
      currentMode == MODE_AURORA ||
      currentMode == MODE_FIREFLIES ||
      currentMode == MODE_FALLING_LEAVES ||
      currentMode == MODE_STORM ||
      currentMode == MODE_TORNADO ||
      currentMode == MODE_ANIMATED_NOTE ||
      currentMode == MODE_COUNTDOWN ||
      currentMode == MODE_POMODORO;
}

unsigned long relayPollIntervalMs() {
  // Firestore reads are metered, so poll conservatively (≈2.5s ≈ 35k reads/day/device,
  // within the free tier). Images transfer over BLE, not Firestore.
  if (isRelayBackgroundMode()) return 5000UL;
  return 2500UL;
}

unsigned long relayStatusIntervalMs() {
  // Status writes are metered too — heartbeat every 30s.
  return 30000UL;
}
void setupDisplay();
void handleTouch();
void initFireworks();
void initHeartRain();
void initSnowfall();
void initStarfield();
void initFireflies();
void initFallingLeaves();
void renderFireworksFrame();
void renderHeartRainFrame();
void renderSnowfallFrame();
void renderAuroraFrame();
void renderStormFrame();
void renderTornadoFrame();
void initTornado();
void renderFirefliesFrame();
void renderFallingLeavesFrame();
void renderStarfieldFrame();
void setParticleMode(const String& name);
void renderCountdownFrame();
void renderPomodoroFrame();
void setCountdown(long seconds);
void syncNtp();
void renderMenuFrame();
void renderConfirmClear();
void executeMenuAction(uint8_t page, uint8_t item);
void explodeRocketStage(uint8_t rocketIdx, uint8_t stagesLeft);
float extractJsonFloatField(const String& body, const char* key, float fallback = 0.f);
void fetchWeather();
void drawWeatherBadge(int x, int y);
void renderWeatherFrame();
void checkTimeGreetings();
void renderSleepFrame();
void setSleepMode();
void drawStatusBar();
void detectEmojiReaction(const String& text);

// ─── BLE value helpers ───

String bleValueToString(const String& value) {
  return value;
}

String bleValueToString(const std::string& value) {
  return String(value.c_str());
}

size_t bleValueLength(const String& value) {
  return value.length();
}

size_t bleValueLength(const std::string& value) {
  return value.size();
}

const uint8_t* bleValueData(const String& value) {
  return reinterpret_cast<const uint8_t*>(value.c_str());
}

const uint8_t* bleValueData(const std::string& value) {
  return reinterpret_cast<const uint8_t*>(value.data());
}

// ─── JSON helpers ───

String jsonEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (c == '\\' || c == '"') {
      escaped += '\\';
      escaped += c;
    } else if (c == '\n') {
      escaped += "\\n";
    } else if (c == '\r') {
      escaped += "\\r";
    } else if (c == '\t') {
      escaped += "\\t";
    } else {
      escaped += c;
    }
  }
  return escaped;
}

int findJsonValueStart(const String& body, const char* key) {
  const String needle = String("\"") + key + "\"";
  const int keyPos = body.indexOf(needle);
  if (keyPos < 0) {
    return -1;
  }

  int cursor = body.indexOf(':', keyPos + needle.length());
  if (cursor < 0) {
    return -1;
  }

  cursor++;
  while (cursor < body.length() && isspace(static_cast<unsigned char>(body[cursor]))) {
    cursor++;
  }
  return cursor;
}

String extractJsonStringField(const String& body, const char* key, const String& fallback = "") {
  int cursor = findJsonValueStart(body, key);
  if (cursor < 0 || cursor >= body.length() || body[cursor] != '"') {
    return fallback;
  }

  cursor++;
  String value;
  bool escaped = false;
  while (cursor < body.length()) {
    const char c = body[cursor++];
    if (escaped) {
      if (c == 'n') {
        value += '\n';
      } else if (c == 'r') {
        value += '\r';
      } else if (c == 't') {
        value += '\t';
      } else {
        value += c;
      }
      escaped = false;
      continue;
    }

    if (c == '\\') {
      escaped = true;
      continue;
    }

    if (c == '"') {
      return value;
    }

    value += c;
  }

  return fallback;
}

int extractJsonIntField(const String& body, const char* key, int fallback = 0) {
  int cursor = findJsonValueStart(body, key);
  if (cursor < 0 || cursor >= body.length()) {
    return fallback;
  }

  int sign = 1;
  if (body[cursor] == '-') {
    sign = -1;
    cursor++;
  }

  bool foundDigit = false;
  long value = 0;
  while (cursor < body.length() && isdigit(static_cast<unsigned char>(body[cursor]))) {
    foundDigit = true;
    value = (value * 10) + (body[cursor] - '0');
    cursor++;
  }

  return foundDigit ? static_cast<int>(value * sign) : fallback;
}

float extractJsonFloatField(const String& body, const char* key, float fallback) {
  int cursor = findJsonValueStart(body, key);
  if (cursor < 0 || cursor >= (int)body.length()) return fallback;
  int start = cursor;
  if (body[cursor] == '-') cursor++;
  bool hasDigit = false;
  while (cursor < (int)body.length() &&
         (isdigit((unsigned char)body[cursor]) || body[cursor] == '.')) {
    hasDigit = true;
    cursor++;
  }
  return hasDigit ? body.substring(start, cursor).toFloat() : fallback;
}

// ─── Status JSON builders ───

String buildStatusJson() {
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\",";
  json += "\"deviceId\":\"" + jsonEscape(deviceToken) + "\",";
  json += "\"fbProject\":\"" + jsonEscape(fbProjectId) + "\",";
  json += "\"fbConfigured\":" + String(fbConfigured() ? "true" : "false") + ",";
  json += "\"hair\":\"" + jsonEscape(companionHair) + "\",";
  json += "\"ears\":\"" + jsonEscape(companionEars) + "\",";
  json += "\"mustache\":\"" + jsonEscape(companionMustache) + "\",";
  json += "\"glasses\":\"" + jsonEscape(companionGlasses) + "\",";
  json += "\"headwear\":\"" + jsonEscape(companionHeadwear) + "\",";
  json += "\"piercing\":\"" + jsonEscape(companionPiercing) + "\",";
  json += "\"headwearSize\":" + String(companionHeadwearSize) + ",";
  json += "\"headwearWidth\":" + String(companionHeadwearWidth) + ",";
  json += "\"headwearHeight\":" + String(companionHeadwearHeight) + ",";
  json += "\"headwearOffsetX\":" + String(companionHeadwearOffsetX) + ",";
  json += "\"headwearOffsetY\":" + String(companionHeadwearOffsetY) + ",";
  json += "\"hairSize\":" + String(companionHairSize) + ",";
  json += "\"mustacheSize\":" + String(companionMustacheSize) + ",";
  json += "\"hairWidth\":" + String(companionHairWidth) + ",";
  json += "\"hairHeight\":" + String(companionHairHeight) + ",";
  json += "\"hairThickness\":" + String(companionHairThickness) + ",";
  json += "\"hairOffsetX\":" + String(companionHairOffsetX) + ",";
  json += "\"hairOffsetY\":" + String(companionHairOffsetY) + ",";
  json += "\"eyeOffsetX\":" + String(companionEyeOffsetX) + ",";
  json += "\"eyeOffsetY\":" + String(companionEyeOffsetY) + ",";
  json += "\"mouthOffsetX\":" + String(companionMouthOffsetX) + ",";
  json += "\"mouthOffsetY\":" + String(companionMouthOffsetY) + ",";
  json += "\"companionScale\":" + String(companionScale) + ",";
  json += "\"companionOffsetX\":" + String(companionOffsetX) + ",";
  json += "\"companionOffsetY\":" + String(companionOffsetY) + ",";
  json += "\"mustacheWidth\":" + String(companionMustacheWidth) + ",";
  json += "\"mustacheHeight\":" + String(companionMustacheHeight) + ",";
  json += "\"mustacheThickness\":" + String(companionMustacheThickness) + ",";
  json += "\"mustacheOffsetX\":" + String(companionMustacheOffsetX) + ",";
  json += "\"mustacheOffsetY\":" + String(companionMustacheOffsetY) + ",";
  json += "\"bondLevel\":" + String(bondLevel) + ",";
  json += "\"energyLevel\":" + String(energyLevel) + ",";
  json += "\"boredomLevel\":" + String(boredomLevel);
  json += "}";
  return json;
}

String buildBleStatusJson() {
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\",";
  json += "\"hair\":\"" + jsonEscape(companionHair) + "\",";
  json += "\"ears\":\"" + jsonEscape(companionEars) + "\",";
  json += "\"mustache\":\"" + jsonEscape(companionMustache) + "\",";
  json += "\"glasses\":\"" + jsonEscape(companionGlasses) + "\",";
  json += "\"headwear\":\"" + jsonEscape(companionHeadwear) + "\",";
  json += "\"piercing\":\"" + jsonEscape(companionPiercing) + "\",";
  json += "\"headwearSize\":" + String(companionHeadwearSize) + ",";
  json += "\"headwearWidth\":" + String(companionHeadwearWidth) + ",";
  json += "\"headwearHeight\":" + String(companionHeadwearHeight) + ",";
  json += "\"headwearOffsetX\":" + String(companionHeadwearOffsetX) + ",";
  json += "\"headwearOffsetY\":" + String(companionHeadwearOffsetY) + ",";
  json += "\"hairSize\":" + String(companionHairSize) + ",";
  json += "\"mustacheSize\":" + String(companionMustacheSize) + ",";
  json += "\"hairWidth\":" + String(companionHairWidth) + ",";
  json += "\"hairHeight\":" + String(companionHairHeight) + ",";
  json += "\"hairThickness\":" + String(companionHairThickness) + ",";
  json += "\"hairOffsetX\":" + String(companionHairOffsetX) + ",";
  json += "\"hairOffsetY\":" + String(companionHairOffsetY) + ",";
  json += "\"eyeOffsetX\":" + String(companionEyeOffsetX) + ",";
  json += "\"eyeOffsetY\":" + String(companionEyeOffsetY) + ",";
  json += "\"mouthOffsetX\":" + String(companionMouthOffsetX) + ",";
  json += "\"mouthOffsetY\":" + String(companionMouthOffsetY) + ",";
  json += "\"companionScale\":" + String(companionScale) + ",";
  json += "\"companionOffsetX\":" + String(companionOffsetX) + ",";
  json += "\"companionOffsetY\":" + String(companionOffsetY) + ",";
  json += "\"mustacheWidth\":" + String(companionMustacheWidth) + ",";
  json += "\"mustacheHeight\":" + String(companionMustacheHeight) + ",";
  json += "\"mustacheThickness\":" + String(companionMustacheThickness) + ",";
  json += "\"mustacheOffsetX\":" + String(companionMustacheOffsetX) + ",";
  json += "\"mustacheOffsetY\":" + String(companionMustacheOffsetY) + ",";
  json += "\"bondLevel\":" + String(bondLevel) + ",";
  json += "\"energyLevel\":" + String(energyLevel) + ",";
  json += "\"boredomLevel\":" + String(boredomLevel);
  json += "}";
  return json;
}

String buildSlimStatusJson() {
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\",";
  json += "\"bondLevel\":" + String(bondLevel) + ",";
  json += "\"energyLevel\":" + String(energyLevel) + ",";
  json += "\"boredomLevel\":" + String(boredomLevel);
  json += "}";
  return json;
}

String buildBleStatusWithNetworksJson() {
  String json = "{";
  json += "\"mode\":\"" + jsonEscape(modeName(currentMode)) + "\",";
  json += "\"status\":\"" + jsonEscape(statusText) + "\",";
  json += "\"ssid\":\"" + jsonEscape(currentSsid) + "\",";
  json += "\"ip\":\"" + jsonEscape(ipAddress) + "\",";
  json += "\"wifiNetworks\":[";
  for (int i = 0; i < availableWifiNetworkCount; i++) {
    if (i > 0) {
      json += ",";
    }
    json += "\"" + jsonEscape(availableWifiNetworks[i]) + "\"";
  }
  json += "]";
  json += "}";
  return json;
}

const char* modeName(DisplayMode mode) {
  switch (mode) {
    case MODE_NOTE:
      return "note";
    case MODE_BANNER:
      return "banner";
    case MODE_IMAGE:
      return "image";
    case MODE_COLOR_IMAGE:
      return "color_image";
    case MODE_EXPRESSION:
      return "expression";
    case MODE_FLOWER:
      return "flower";
    case MODE_FIREWORKS:
      return "fireworks";
    case MODE_HEART_RAIN:
      return "heart_rain";
    case MODE_SNOWFALL:
      return "snowfall";
    case MODE_STARFIELD:
      return "starfield";
    case MODE_COUNTDOWN:
      return "countdown";
    case MODE_WEATHER:
      return "weather";
    case MODE_SLEEP:
      return "sleep";
    case MODE_MENU:
      return "menu";
    case MODE_CONFIRM_CLEAR:
      return "confirm_clear";
    case MODE_FIRECRACKER:
      return "firecracker";
    case MODE_IDLE:
    default:
      return "idle";
  }
}

// ─── Relay ───

int relayRequest(const char* method, const String& url, const String& body, String& response) {
  Serial.printf("[RELAY] %s %s (Heap: %u)\n", method, url.c_str(), ESP.getFreeHeap());
  if (body.length() > 0) {
    Serial.printf("[RELAY] Body: %.120s\n", body.c_str());
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure(); // Allow self-signed certs for local testing if needed
  HTTPClient http;

  bool started = url.startsWith("https://")
      ? http.begin(secureClient, url)
      : http.begin(url);

  if (!started) {
    response = "HTTP begin failed";
    Serial.println("[RELAY] ERROR: http.begin() failed.");
    return -1;
  }

  // Increased timeouts for more reliable connection, especially on cold start.
  http.setTimeout(5000); // 5 seconds total transaction timeout
  http.setConnectTimeout(2500); // 2.5 seconds to connect

  if (body.length() > 0) {
    http.addHeader("Content-Type", "application/json");
  }

  int code;
  unsigned long startMs = millis();
  if (strcmp(method, "GET") == 0) {
    code = http.GET();
  } else if (strcmp(method, "POST") == 0) {
    code = http.POST((uint8_t*)body.c_str(), body.length());
  } else {
    code = http.sendRequest(method, (uint8_t*)body.c_str(), body.length());
  }
  unsigned long durationMs = millis() - startMs;

  Serial.printf("[RELAY] Request completed in %lu ms, HTTP Code: %d\n", durationMs, code);

  if (code > 0) {
    response = http.getString();
    Serial.printf("[RELAY] Response Body: %.120s\n", response.c_str());
  } else {
    response = http.errorToString(code);
    Serial.printf("[RELAY] HTTP Error: %s\n", response.c_str());
  }

  http.end();
  return code;
}

void clearImageBuffer() {
  memset(imageBuffer, 0, sizeof(imageBuffer));
}

bool relayColorTransferActive() {
  return colorImageTransferActive &&
      expectedColorBytes > 0 &&
      receivedColorBytes < expectedColorBytes;
}

bool ensureIdleBackgroundBuffer() {
  if (idleBackgroundBuffer) return true;
  idleBackgroundBuffer = (uint16_t*)ps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 2);
  return idleBackgroundBuffer != nullptr;
}

// Rotate the idle background image 90° CW in-place, scale-to-fill (no black bars)
void rotateBackgroundCW90() {
  if (!idleBackgroundBuffer) return;
  const int W = SCREEN_WIDTH;   // 320
  const int H = SCREEN_HEIGHT;  // 240
  uint16_t* tmp = (uint16_t*)ps_malloc(W * H * 2);
  if (!tmp) return;
  // 90°CW + scale-to-fill: portrait (H×W=240×320) scaled by W/H=4/3 to fill W×H
  // display(xd,yd) → source(sc,sr):
  //   sc = yd*3/4 + 70   (portrait_y remapped to source column)
  //   sr = 239 - xd*3/4  (portrait_x remapped to source row)
  for (int yd = 0; yd < H; yd++) {
    const int sc = yd * 3 / 4 + 70;
    for (int xd = 0; xd < W; xd++) {
      tmp[yd * W + xd] = idleBackgroundBuffer[(239 - xd * 3 / 4) * W + sc];
    }
  }
  memcpy(idleBackgroundBuffer, tmp, W * H * 2);
  free(tmp);
}

bool saveIdleBackgroundFromColorBuffer() {
  if (!colorImageBuffer) return false;
  if (!ensureIdleBackgroundBuffer()) return false;
  memcpy(idleBackgroundBuffer, colorImageBuffer, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
  idleUseBackgroundImage = true;
  return true;
}

void refreshWeatherDisplayIfVisible() {
  if (currentMode == MODE_WEATHER || (currentMode == MODE_IDLE && idleShowWeather)) {
    renderCurrentMode();
  }
}

// ─── Pet logic (identical to mini) ───

String petDisplayLabel(const String& value) {
  String label = value;
  label.replace("_", " ");
  if (label.length() > 0) {
    label.setCharAt(0, toupper(static_cast<unsigned char>(label[0])));
  }
  return label;
}

String normalizePetPersonality(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "playful" || trimmed == "cuddly" || trimmed == "sleepy" || trimmed == "curious") {
    return trimmed;
  }
  return "curious";
}

String normalizePetMode(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "off" || trimmed == "hangout" || trimmed == "play" || trimmed == "cuddle" || trimmed == "nap" || trimmed == "party" || trimmed == "needy") {
    return trimmed;
  }
  return "hangout";
}

String normalizeCareAction(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "pet" || trimmed == "cheer" || trimmed == "comfort" || trimmed == "dance" || trimmed == "surprise") {
    return trimmed;
  }
  return "pet";
}

String normalizeCompanionHair(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "tuft" || trimmed == "bangs" || trimmed == "spiky" || trimmed == "swoop" || trimmed == "bob" || trimmed == "messy" || trimmed == "ponytail" || trimmed == "curly" || trimmed == "pigtails" || trimmed == "mohawk") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionEars(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "cat" || trimmed == "bear" || trimmed == "bunny") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionMustache(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "classic" || trimmed == "curled" || trimmed == "handlebar" || trimmed == "walrus" || trimmed == "pencil" || trimmed == "imperial" || trimmed == "goatee" || trimmed == "soul_patch") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionGlasses(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "round" || trimmed == "square" || trimmed == "visor") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionHeadwear(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "bow" || trimmed == "beanie" || trimmed == "crown" || trimmed == "top_hat" || trimmed == "halo" || trimmed == "flower_crown" || trimmed == "beret") {
    return trimmed;
  }
  return "none";
}

String normalizeCompanionPiercing(const String& value) {
  const String trimmed = value.length() == 0 ? "" : value;
  if (trimmed == "none" || trimmed == "brow" || trimmed == "nose" || trimmed == "lip") {
    return trimmed;
  }
  return "none";
}

String activePetBehavior() {
  if (activePetMode == "off") return "off";
  if (activePetMode != "hangout") return activePetMode;
  return petPersonality;
}

String petAmbientStatus() {
  if (!activeCareAction.isEmpty()) return currentAttentionStatus();
  if (activePetMode == "off") return "Ready";
  if (activePetMode == "play") return "Bright-eyed";
  if (activePetMode == "cuddle") return "Close by";
  if (activePetMode == "nap") return "Quiet";
  if (activePetMode == "party") return "Sparked up";
  if (activePetMode == "needy") return "Looking your way";
  return "Here with you";
}

int clampLevel(int value) {
  if (value < 0) return 0;
  if (value > 100) return 100;
  return value;
}

int clampAppearancePercent(int value) {
  if (value < 10) return 10;
  if (value > 400) return 400;
  return value;
}

int clampAppearanceOffset(int value) {
  if (value < -60) return -60;
  if (value > 60) return 60;
  return value;
}

int clampCompanionScale(int value) {
  if (value < 10) return 10;
  if (value > 300) return 300;
  return value;
}

int scaleByPercent(int base, int percent) {
  const int clampedPercent = clampAppearancePercent(percent);
  const long scaled = (static_cast<long>(base) * clampedPercent) / 100L;
  if (scaled == 0 && base != 0) return base > 0 ? 1 : -1;
  return static_cast<int>(scaled);
}

String currentAttentionStatus() {
  if (activeCareAction == "pet") return "Enjoying the moment";
  if (activeCareAction == "cheer") return "Lit up";
  if (activeCareAction == "comfort") return "Settled in";
  if (activeCareAction == "dance") return "All spark";
  if (activeCareAction == "surprise") return "Curious";
  if (bondLevel < 30) return "Nearby";
  if (energyLevel < 25) return "Taking it slow";
  if (boredomLevel > 70) return "Restless";
  return "Here with you";
}

String pickAutonomousPetExpression() {
  const String behavior = activePetBehavior();
  const uint8_t cycle = petCycleStep++ % 4;
  if (behavior == "off") return "happy";
  if (behavior == "party") {
    if (cycle == 0) return "star_eyes";
    if (cycle == 1) return "laugh";
    if (cycle == 2) return "excited";
    return "tongue";
  }
  if (behavior == "play") {
    if (cycle == 0) return "excited";
    if (cycle == 1) return "laugh";
    if (cycle == 2) return "wink";
    return "tongue";
  }
  if (behavior == "cuddle") {
    if (cycle == 0) return "love";
    if (cycle == 1) return "kiss";
    if (cycle == 2) return "heart";
    return "smile";
  }
  if (behavior == "nap" || behavior == "sleepy") {
    if (cycle == 0) return "sleepy";
    if (cycle == 1) return "smile";
    if (cycle == 2) return "thinking";
    return "sleepy";
  }
  if (behavior == "needy") {
    if (cycle == 0) return "sad";
    if (cycle == 1) return "look_around";
    if (cycle == 2) return "kiss";
    return "surprised";
  }
  if (behavior == "playful") {
    if (cycle == 0) return "excited";
    if (cycle == 1) return "laugh";
    if (cycle == 2) return "wink";
    return "tongue";
  }
  if (behavior == "cuddly") {
    if (cycle == 0) return "love";
    if (cycle == 1) return "heart";
    if (cycle == 2) return "kiss";
    return "smile";
  }
  if (cycle == 0) return "look_around";
  if (cycle == 1) return "thinking";
  if (cycle == 2) return "confused";
  return "surprised";
}

String pickReactionExpression(const String& trigger) {
  if (trigger == "dance") return "star_eyes";
  if (trigger == "pet") return petPersonality == "cuddly" ? "love" : "heart";
  if (trigger == "cheer") return "excited";
  if (trigger == "comfort") return petPersonality == "sleepy" ? "smile" : "kiss";
  if (trigger == "surprise") {
    const uint8_t cycle = petCycleStep++ % 4;
    if (cycle == 0) return "surprised";
    if (cycle == 1) return "star_eyes";
    if (cycle == 2) return "laugh";
    return "tongue";
  }
  if (trigger == "button_clear") return "sad";
  if (trigger == "button_next") return petPersonality == "playful" ? "wink" : "surprised";
  if (trigger == "note") return petPersonality == "cuddly" ? "love" : "thinking";
  if (trigger == "banner") return activePetMode == "party" ? "laugh" : "kiss";
  if (trigger == "personality" || trigger == "pet_mode") return pickAutonomousPetExpression();
  return "happy";
}

void persistPetState() {
  preferences.begin("desk-cfg", false);
  preferences.putString("pet_personality", petPersonality);
  preferences.putString("pet_mode", activePetMode);
  preferences.putString("companion_hair", companionHair);
  preferences.putString("companion_ears", companionEars);
  preferences.putString("companion_mustache", companionMustache);
  preferences.putString("companion_glasses", companionGlasses);
  preferences.putString("companion_headwear", companionHeadwear);
  preferences.putString("companion_piercing", companionPiercing);
  preferences.putInt("companion_headwear_size", companionHeadwearSize);
  preferences.putInt("companion_headwear_width", companionHeadwearWidth);
  preferences.putInt("companion_headwear_height", companionHeadwearHeight);
  preferences.putInt("companion_headwear_offset_x", companionHeadwearOffsetX);
  preferences.putInt("companion_headwear_offset_y", companionHeadwearOffsetY);
  preferences.putInt("companion_hair_size", companionHairSize);
  preferences.putInt("companion_mustache_size", companionMustacheSize);
  preferences.putInt("companion_hair_width", companionHairWidth);
  preferences.putInt("companion_hair_height", companionHairHeight);
  preferences.putInt("companion_hair_thickness", companionHairThickness);
  preferences.putInt("companion_hair_offset_x", companionHairOffsetX);
  preferences.putInt("companion_hair_offset_y", companionHairOffsetY);
  preferences.putInt("companion_eye_offset_x", companionEyeOffsetX);
  preferences.putInt("companion_eye_offset_y", companionEyeOffsetY);
  preferences.putInt("companion_mouth_offset_x", companionMouthOffsetX);
  preferences.putInt("companion_mouth_offset_y", companionMouthOffsetY);
  preferences.putInt("companion_scale", companionScale);
  preferences.putInt("companion_offset_x", companionOffsetX);
  preferences.putInt("companion_offset_y", companionOffsetY);
  preferences.putInt("companion_mustache_width", companionMustacheWidth);
  preferences.putInt("companion_mustache_height", companionMustacheHeight);
  preferences.putInt("companion_mustache_thickness", companionMustacheThickness);
  preferences.putInt("companion_mustache_offset_x", companionMustacheOffsetX);
  preferences.putInt("companion_mustache_offset_y", companionMustacheOffsetY);
  preferences.putInt("bond_level", bondLevel);
  preferences.putInt("energy_level", energyLevel);
  preferences.putInt("boredom_level", boredomLevel);
  preferences.end();
}

void restoreTransientMode() {
  if (!transientActive) return;
  // Don't yank user out of menu or confirm-clear
  if (currentMode == MODE_MENU || currentMode == MODE_CONFIRM_CLEAR) {
    transientActive = false;
    return;
  }
  transientActive = false;
  currentMode = transientResumeMode;
  currentExpression = transientResumeExpression;
  currentFlower = transientResumeFlower;
  statusText = currentMode == MODE_IDLE ? currentAttentionStatus() : transientResumeStatus;
  expressionPhase = 0;
  activeCareAction = "";
  renderCurrentMode();
  publishStatus();
}

void startTransientExpression(const String& expression, unsigned long durationMs, const String& nextStatus) {
  if (transientActive) restoreTransientMode();
  transientResumeMode = currentMode;
  transientResumeStatus = currentMode == MODE_IDLE ? currentAttentionStatus() : statusText;
  transientResumeExpression = currentExpression;
  transientResumeFlower = currentFlower;
  transientActive = true;
  transientEndsAt = millis() + durationMs;
  lastPetBeatMs = millis();
  currentExpression = expression;
  expressionPhase = 0;
  lastExpressionTickMs = 0;
  currentMode = MODE_EXPRESSION;
  statusText = nextStatus;
  renderCurrentMode();
  publishStatus();
}

void setPetPersonality(const String& personality, bool persist) {
  petPersonality = normalizePetPersonality(personality);
  if (persist) persistPetState();
  startTransientExpression(pickReactionExpression("personality"), 2200, petAmbientStatus());
}

void triggerPetMode(const String& petMode, bool persist) {
  activePetMode = normalizePetMode(petMode);
  activeCareAction = "";
  if (activePetMode == "party") {
    boredomLevel = clampLevel(boredomLevel - 20);
    energyLevel = clampLevel(energyLevel - 8);
  }
  if (persist) persistPetState();
  currentMode = MODE_IDLE;
  statusText = petAmbientStatus();
  lastPetBeatMs = 0;
  if (activePetMode == "off") {
    renderCurrentMode();
    publishStatus();
    return;
  }
  startTransientExpression(pickReactionExpression("pet_mode"), 3200, petAmbientStatus());
}

void sendCareAction(const String& action) {
  activeCareAction = normalizeCareAction(action);
  if (activeCareAction == "pet") {
    bondLevel = clampLevel(bondLevel + 8);
    boredomLevel = clampLevel(boredomLevel - 8);
  } else if (activeCareAction == "cheer") {
    energyLevel = clampLevel(energyLevel + 10);
    boredomLevel = clampLevel(boredomLevel - 6);
  } else if (activeCareAction == "comfort") {
    bondLevel = clampLevel(bondLevel + 5);
    energyLevel = clampLevel(energyLevel + 4);
  } else if (activeCareAction == "dance") {
    activePetMode = "party";
    energyLevel = clampLevel(energyLevel - 10);
    boredomLevel = clampLevel(boredomLevel - 18);
  } else if (activeCareAction == "surprise") {
    boredomLevel = clampLevel(boredomLevel - 12);
    energyLevel = clampLevel(energyLevel - 4);
  }
  persistPetState();
  startTransientExpression(pickReactionExpression(activeCareAction), activeCareAction == "dance" ? 3600 : 2200, currentAttentionStatus());
}

void updateCompanionNeeds() {
  if (millis() - lastCompanionTickMs < 60000UL) return;
  lastCompanionTickMs = millis();
  boredomLevel = clampLevel(boredomLevel + 2);
  energyLevel = clampLevel(energyLevel - 1);
  if (bondLevel > 10) bondLevel = clampLevel(bondLevel - 1);
  if (activePetMode != "off") {
    if (energyLevel <= 20) activePetMode = "nap";
    else if (boredomLevel >= 80) activePetMode = "needy";
    else if (activePetMode == "needy" && boredomLevel <= 40) activePetMode = "hangout";
  }
  persistPetState();
}

void updatePetBehavior() {
  updateCompanionNeeds();
  const unsigned long now = millis();
  if (transientActive && now >= transientEndsAt) {
    restoreTransientMode();
    return;
  }
  if (currentMode != MODE_IDLE || transientActive) return;
  if (activePetMode == "off") return;

  unsigned long intervalMs = 18000UL;
  if (activePetMode == "party") intervalMs = 9000UL;
  else if (activePetMode == "play") intervalMs = 12000UL;
  else if (activePetMode == "nap") intervalMs = 26000UL;
  else if (activePetMode == "needy") intervalMs = 14000UL;
  else if (petPersonality == "playful") intervalMs = 13000UL;
  else if (petPersonality == "sleepy") intervalMs = 24000UL;

  if (bondLevel < 30) intervalMs = 12000UL;
  if (boredomLevel > 75) intervalMs = 10000UL;
  if (now - lastPetBeatMs < intervalMs) return;

  startTransientExpression(pickAutonomousPetExpression(), 2200, currentAttentionStatus());
}

bool decodeBase64IntoImage(const String& input) {
  size_t decodedLength = 0;
  if (mbedtls_base64_decode(nullptr, 0, &decodedLength,
      reinterpret_cast<const unsigned char*>(input.c_str()), input.length()) != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
    return false;
  }
  if (decodedLength != sizeof(imageBuffer)) return false;
  size_t outputLength = 0;
  const int result = mbedtls_base64_decode(
    imageBuffer, sizeof(imageBuffer), &outputLength,
    reinterpret_cast<const unsigned char*>(input.c_str()), input.length()
  );
  return result == 0 && outputLength == sizeof(imageBuffer);
}

bool decodeBase64IntoColorImage(const String& input) {
  const size_t targetBytes = SCREEN_WIDTH * SCREEN_HEIGHT * 2;
  size_t decodedLength = 0;
  if (mbedtls_base64_decode(nullptr, 0, &decodedLength,
      reinterpret_cast<const unsigned char*>(input.c_str()), input.length()) != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
    return false;
  }
  if (decodedLength != targetBytes) {
    return false;
  }
  if (!colorImageBuffer) {
    colorImageBuffer = (uint16_t*)ps_malloc(targetBytes);
  }
  if (!colorImageBuffer) {
    return false;
  }

  size_t outputLength = 0;
  const int result = mbedtls_base64_decode(
    reinterpret_cast<unsigned char*>(colorImageBuffer),
    targetBytes,
    &outputLength,
    reinterpret_cast<const unsigned char*>(input.c_str()),
    input.length()
  );
  if (result != 0 || outputLength != targetBytes) {
    return false;
  }

  const size_t pixelCount = SCREEN_WIDTH * SCREEN_HEIGHT;
  for (size_t i = 0; i < pixelCount; i++) {
    uint8_t hi = ((uint8_t*)colorImageBuffer)[i * 2];
    uint8_t lo = ((uint8_t*)colorImageBuffer)[i * 2 + 1];
    colorImageBuffer[i] = (uint16_t)(hi << 8) | lo;
  }

  return true;
}

bool appendBase64ColorChunk(const String& input) {
  if (!colorImageTransferActive || !colorImageBuffer) {
    return false;
  }

  size_t decodedLength = 0;
  if (mbedtls_base64_decode(
          nullptr,
          0,
          &decodedLength,
          reinterpret_cast<const unsigned char*>(input.c_str()),
          input.length()) != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
    return false;
  }

  if (receivedColorBytes + decodedLength > expectedColorBytes) {
    return false;
  }

  size_t outputLength = 0;
  const int result = mbedtls_base64_decode(
      reinterpret_cast<unsigned char*>(colorImageBuffer) + receivedColorBytes,
      expectedColorBytes - receivedColorBytes,
      &outputLength,
      reinterpret_cast<const unsigned char*>(input.c_str()),
      input.length());
  if (result != 0 || outputLength != decodedLength) {
    return false;
  }

  receivedColorBytes += outputLength;
  return true;
}

void publishStatus() {
  if (statusCharacteristic != nullptr) {
    const String slim = buildSlimStatusJson();
    statusCharacteristic->setValue(slim.c_str());
    statusCharacteristic->notify();
    const String full = buildBleStatusJson();
    statusCharacteristic->setValue(full.c_str());
  }
  relayStatusDirty = true;
}

void publishStatusWithNetworks() {
  const String payload = buildBleStatusWithNetworksJson();
  if (statusCharacteristic != nullptr) {
    statusCharacteristic->setValue(payload.c_str());
    statusCharacteristic->notify();
  }
  relayStatusDirty = true;
}

// ─── TFT rendering helpers ───
// All coordinates are scaled ~2x from the 128×64 OLED version.
// Face occupies the center 240×160 area of the 240×320 screen.
// STATUS_Y is used for a status bar at the bottom.

static const int FACE_OFFSET_Y = 40;  // vertical offset to center the face area
static const int STATUS_BAR_Y = 222;  // y position for status text (within 240px screen)

// 5-point star (filled, anti-aliased) with optional soft glow. Shared by icons/effects.
static void fxStar(float cx, float cy, float r, uint16_t color, bool glow = false) {
  if (r < 2.0f) { fxDiscAA(cx, cy, r, color); return; }
  if (glow) fxGlow(cx, cy, r * 1.9f, color, 90);
  float inner = r * 0.42f;
  float vx[10], vy[10];
  for (int i = 0; i < 10; i++) {
    float rad = (i & 1) ? inner : r;
    float a = -1.5708f + i * 3.14159265f / 5.0f;
    vx[i] = cx + rad * cosf(a);
    vy[i] = cy + rad * sinf(a);
  }
  for (int i = 0; i < 10; i++) {
    int j = (i + 1) % 10;
    gfx->fillTriangle((int)cx, (int)cy, (int)vx[i], (int)vy[i], (int)vx[j], (int)vy[j], color);
  }
  for (int i = 0; i < 10; i++) {
    int j = (i + 1) % 10;
    fxThickLine(vx[i], vy[i], vx[j], vy[j], 1.4f, color);
  }
  fxDiscAA(cx - r * 0.12f, cy - r * 0.12f, r * 0.18f, fxLighten(color, 140), 160);
}

void drawIconHeart(int cx, int cy, int s) { drawBigHeart(cx, cy, s); }

void drawIconStar(int cx, int cy, int r) { fxStar(cx, cy, r, userAccentColor == COL_FG ? COL_GOLD : userAccentColor, true); }

void drawIconFlower(int cx, int cy, int r) {
  uint16_t petal = COL_PINK;
  for (int i = 0; i < 5; i++) {
    float angle = i * 3.14159f * 2.0f / 5.0f - 1.5708f;
    fxDiscAA(cx + r * cosf(angle), cy + r * sinf(angle), r * 0.56f, petal);
  }
  fxDiscRadial(cx, cy, r * 0.55f, COL_GOLD, fxDarken(COL_GOLD, 60));
}

void drawIconNote(int cx, int cy, int s) {
  uint16_t c = userAccentColor;
  fxDiscAA(cx - s / 2.0f, cy + s - 1, s - 2.0f, c);
  fxThickLine(cx - s / 2.0f + s - 3, cy + s - 1, cx - s / 2.0f + s - 3, cy - s, 2.0f, c);
  fxThickLine(cx - s / 2.0f + s - 3, cy - s, cx + s, cy - s + 2, 2.4f, c);
}

void drawIconMoon(int cx, int cy, int r) {
  fxGlow(cx, cy, r * 1.6f, COL_GOLD, 70);
  fxDiscAA(cx, cy, r, COL_GOLD);
  fxDiscAA(cx + r / 2.0f, cy - r / 3.0f, r - 1.0f, COL_BG);
}

// ═════════════════════════════════════════════════════════════════════════════
//  EMOJI ENGINE — UTF-8 decode + hand-drawn colored glyphs.
//  Real OS emoji fonts can't run on Adafruit_GFX, so the device decodes UTF-8
//  emoji from note text and renders a matching vector glyph in full color.
//  Common emoji also trigger a full-screen face reaction (see detectEmojiReaction).
// ═════════════════════════════════════════════════════════════════════════════

// Decode one UTF-8 codepoint at byte index i; sets len to bytes consumed (>=1).
static uint32_t utf8DecodeAt(const String& s, int i, int& len) {
  int n = s.length();
  if (i >= n) { len = 1; return 0; }
  uint8_t c = (uint8_t)s[i];
  if (c < 0x80) { len = 1; return c; }
  uint32_t cp; int extra;
  if      ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
  else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
  else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
  else { len = 1; return 0xFFFD; }
  if (i + extra >= n) { len = n - i; return 0xFFFD; }
  for (int k = 1; k <= extra; k++) {
    uint8_t cc = (uint8_t)s[i + k];
    if ((cc & 0xC0) != 0x80) { len = 1; return 0xFFFD; }
    cp = (cp << 6) | (cc & 0x3F);
  }
  len = 1 + extra;
  return cp;
}

static EmojiId emojiClassify(uint32_t cp) {
  // Combining / modifier codepoints we silently consume.
  if (cp == 0xFE0F || cp == 0x200D || (cp >= 0x1F3FB && cp <= 0x1F3FF)) return EMO_IGNORE;
  switch (cp) {
    case 0x2764: case 0x2665: case 0x1F495: case 0x1F496: case 0x1F497:
    case 0x1F498: case 0x1F499: case 0x1F49A: case 0x1F49B: case 0x1F49C:
    case 0x1F49D: case 0x1F49E: case 0x1F49F: case 0x1F5A4: case 0x1F90D:
    case 0x1F90E: return EMO_HEART;
    case 0x1F600: case 0x1F642: case 0x1F643: case 0x263A: return EMO_SMILE;
    case 0x1F601: case 0x1F603: case 0x1F604: case 0x1F60A: case 0x1F60B: return EMO_GRIN;
    case 0x1F605: case 0x1F606: case 0x1F602: case 0x1F923: return EMO_LAUGH;
    case 0x1F60D: case 0x1F970: case 0x1F929: return EMO_HEARTEYES;
    case 0x1F617: case 0x1F618: case 0x1F619: case 0x1F61A: return EMO_KISS;
    case 0x1F609: return EMO_WINK;
    case 0x1F60E: return EMO_COOL;
    case 0x1F614: case 0x1F61F: case 0x1F622: case 0x1F97A: return EMO_CRY;
    case 0x1F62D: return EMO_SOB;
    case 0x1F620: case 0x1F621: case 0x1F624: case 0x1F92C: return EMO_ANGRY;
    case 0x1F4A4: case 0x1F62A: case 0x1F634: return EMO_SLEEPY;
    case 0x1F914: case 0x1F928: return EMO_THINK;
    case 0x1F62E: case 0x1F62F: case 0x1F632: case 0x1F633: return EMO_SURPRISE;
    case 0x1F610: case 0x1F611: case 0x1F60F: return EMO_NEUTRAL;
    case 0x2B50: case 0x1F31F: case 0x1F4AB: return EMO_STAR;
    case 0x2728: return EMO_SPARKLE;
    case 0x1F525: return EMO_FIRE;
    case 0x1F389: case 0x1F38A: return EMO_PARTY;
    case 0x1F44D: return EMO_THUMBSUP;
    case 0x1F337: case 0x1F338: case 0x1F339: case 0x1F33A: case 0x1F33B:
    case 0x1F33C: case 0x1F490: case 0x1F4AE: return EMO_FLOWER;
    case 0x2600: case 0x1F31E: return EMO_SUN;
    case 0x1F319: case 0x1F31B: case 0x1F31C: return EMO_MOON;
    case 0x2601: case 0x26C5: return EMO_CLOUD;
    case 0x2744: case 0x2603: case 0x26C4: return EMO_SNOW;
    case 0x1F308: return EMO_RAINBOW;
    case 0x1F382: case 0x1F370: return EMO_CAKE;
    case 0x1F381: return EMO_GIFT;
    default: return EMO_NONE;
  }
}

// Round yellow emoji-face base with soft shading.
static void emojiFaceBase(int cx, int cy, float r) {
  fxGlow(cx, cy, r * 1.35f, COL_GOLD, 60);
  fxDiscRadial(cx, cy, r, fxRGB(255, 226, 120), fxRGB(236, 170, 40));
  fxDiscAA(cx, cy, r, fxRGB(150, 96, 0), 70);                  // soft outline
  fxDiscAA(cx - r * 0.34f, cy - r * 0.36f, r * 0.30f, fxRGB(255, 246, 200), 150); // sheen
}

// Draw a colored emoji glyph centered at (cx,cy) with nominal radius r.
// Returns true if cp was a recognised, drawable emoji.
bool drawEmoji(uint32_t cp, int cx, int cy, int r) {
  EmojiId id = emojiClassify(cp);
  if (id == EMO_NONE || id == EMO_IGNORE) return id == EMO_IGNORE;  // ignore = consumed, no draw
  const uint16_t eyeC = fxRGB(40, 26, 0);
  const float ex = r * 0.40f, ey = r * 0.18f, eR = r * 0.16f;
  switch (id) {
    case EMO_SMILE: case EMO_GRIN: {
      emojiFaceBase(cx, cy, r);
      fxDiscAA(cx - ex, cy - ey, eR, eyeC); fxDiscAA(cx + ex, cy - ey, eR, eyeC);
      drawMouthCurve(cx, cy + r * 0.18f, r * (id == EMO_GRIN ? 1.1f : 0.85f), r * 0.42f, 3.0f, eyeC);
      break;
    }
    case EMO_LAUGH: {
      emojiFaceBase(cx, cy, r);
      drawMouthCurve(cx - ex, cy - ey - 1, r * 0.4f, -r * 0.3f, 2.6f, eyeC);
      drawMouthCurve(cx + ex, cy - ey - 1, r * 0.4f, -r * 0.3f, 2.6f, eyeC);
      fxEllipseAA(cx, cy + r * 0.34f, r * 0.5f, r * 0.34f, fxRGB(120, 30, 40));
      fxEllipseAA(cx, cy + r * 0.46f, r * 0.42f, r * 0.2f, COL_ROSE);
      break;
    }
    case EMO_HEARTEYES: {
      emojiFaceBase(cx, cy, r);
      drawBigHeart(cx - ex, cy - ey, r * 0.34f);
      drawBigHeart(cx + ex, cy - ey, r * 0.34f);
      drawMouthCurve(cx, cy + r * 0.2f, r * 0.95f, r * 0.42f, 3.0f, eyeC);
      break;
    }
    case EMO_KISS: {
      emojiFaceBase(cx, cy, r);
      drawMouthCurve(cx - ex, cy - ey, r * 0.36f, -r * 0.28f, 2.4f, eyeC); // wink
      fxDiscAA(cx + ex, cy - ey, eR, eyeC);
      fxDiscAA(cx + r * 0.1f, cy + r * 0.4f, r * 0.18f, COL_ROSE);
      drawBigHeart(cx + r * 0.7f, cy - r * 0.5f, r * 0.22f);
      break;
    }
    case EMO_WINK: {
      emojiFaceBase(cx, cy, r);
      drawMouthCurve(cx - ex, cy - ey, r * 0.36f, -r * 0.28f, 2.4f, eyeC);
      fxDiscAA(cx + ex, cy - ey, eR, eyeC);
      drawMouthCurve(cx, cy + r * 0.18f, r * 0.85f, r * 0.4f, 3.0f, eyeC);
      break;
    }
    case EMO_COOL: {
      emojiFaceBase(cx, cy, r);
      fxThickLine(cx - r * 0.7f, cy - ey, cx + r * 0.7f, cy - ey, r * 0.34f, fxRGB(20, 20, 28));
      fxDiscAA(cx - ex, cy - ey, eR * 1.1f, fxRGB(20, 20, 28));
      fxDiscAA(cx + ex, cy - ey, eR * 1.1f, fxRGB(20, 20, 28));
      drawMouthCurve(cx, cy + r * 0.2f, r * 0.8f, r * 0.36f, 3.0f, eyeC);
      break;
    }
    case EMO_CRY: case EMO_SOB: {
      emojiFaceBase(cx, cy, r);
      fxDiscAA(cx - ex, cy - ey, eR, eyeC); fxDiscAA(cx + ex, cy - ey, eR, eyeC);
      drawTear(cx - ex, cy + r * 0.1f, r * 0.2f);
      if (id == EMO_SOB) drawTear(cx + ex, cy + r * 0.1f, r * 0.2f);
      drawMouthCurve(cx, cy + r * 0.5f, r * 0.6f, -r * 0.3f, 2.6f, eyeC);
      break;
    }
    case EMO_ANGRY: {
      fxGlow(cx, cy, r * 1.35f, COL_ROSE, 70);
      fxDiscRadial(cx, cy, r, fxRGB(255, 150, 120), fxRGB(220, 70, 50));
      fxDiscAA(cx, cy, r, fxRGB(120, 20, 10), 80);
      fxThickLine(cx - r * 0.6f, cy - r * 0.55f, cx - r * 0.1f, cy - ey, 2.6f, eyeC);
      fxThickLine(cx + r * 0.6f, cy - r * 0.55f, cx + r * 0.1f, cy - ey, 2.6f, eyeC);
      fxDiscAA(cx - ex, cy - ey + 1, eR, eyeC); fxDiscAA(cx + ex, cy - ey + 1, eR, eyeC);
      drawMouthCurve(cx, cy + r * 0.45f, r * 0.6f, -r * 0.25f, 2.6f, eyeC);
      break;
    }
    case EMO_SLEEPY: {
      emojiFaceBase(cx, cy, r);
      drawMouthCurve(cx - ex, cy - ey, r * 0.34f, r * 0.18f, 2.2f, eyeC);
      drawMouthCurve(cx + ex, cy - ey, r * 0.34f, r * 0.18f, 2.2f, eyeC);
      drawMouthCurve(cx, cy + r * 0.3f, r * 0.4f, r * 0.16f, 2.2f, eyeC);
      {  // little drawn "z" floating above (no font-size side effects)
        float zx = cx + r * 0.55f, zy = cy - r * 1.05f, zs = r * 0.3f;
        fxThickLine(zx - zs, zy - zs, zx + zs, zy - zs, 1.4f, COL_LAVENDER);
        fxThickLine(zx + zs, zy - zs, zx - zs, zy + zs, 1.4f, COL_LAVENDER);
        fxThickLine(zx - zs, zy + zs, zx + zs, zy + zs, 1.4f, COL_LAVENDER);
      }
      break;
    }
    case EMO_THINK: {
      emojiFaceBase(cx, cy, r);
      fxDiscAA(cx - ex, cy - ey, eR, eyeC); fxDiscAA(cx + ex, cy - ey, eR, eyeC);
      fxThickLine(cx - r * 0.4f, cy + r * 0.4f, cx + r * 0.4f, cy + r * 0.3f, 2.4f, eyeC);
      fxDiscAA(cx + r * 0.55f, cy + r * 0.45f, r * 0.22f, fxRGB(210, 170, 130)); // hand
      break;
    }
    case EMO_SURPRISE: {
      emojiFaceBase(cx, cy, r);
      fxDiscAA(cx - ex, cy - ey, eR * 1.1f, eyeC); fxDiscAA(cx + ex, cy - ey, eR * 1.1f, eyeC);
      fxDiscAA(cx, cy + r * 0.4f, r * 0.26f, fxRGB(90, 24, 32));
      break;
    }
    case EMO_NEUTRAL: {
      emojiFaceBase(cx, cy, r);
      fxDiscAA(cx - ex, cy - ey, eR, eyeC); fxDiscAA(cx + ex, cy - ey, eR, eyeC);
      fxThickLine(cx - r * 0.45f, cy + r * 0.4f, cx + r * 0.45f, cy + r * 0.4f, 2.4f, eyeC);
      break;
    }
    case EMO_HEART:    drawBigHeart(cx, cy, r * 0.9f); break;
    case EMO_STAR:     fxStar(cx, cy, r, COL_GOLD, true); break;
    case EMO_SPARKLE:
      fxStar(cx, cy, r * 0.9f, COL_GOLD, true);
      fxStar(cx - r * 0.7f, cy - r * 0.6f, r * 0.35f, fxLighten(COL_GOLD, 80));
      fxStar(cx + r * 0.6f, cy + r * 0.6f, r * 0.3f, fxLighten(COL_GOLD, 80));
      break;
    case EMO_FIRE: {
      fxGlow(cx, cy, r * 1.4f, fxRGB(255, 120, 0), 90);
      for (int yy = (int)(cy - r); yy <= (int)(cy + r); yy++) {
        float ty = ((float)yy - (cy - r)) / (2 * r);
        float hw = r * (0.2f + 0.8f * ty) * (1.0f - ty * 0.2f);
        uint16_t c = fxMix(fxRGB(255, 220, 60), fxRGB(255, 70, 0), (uint8_t)(ty * 255));
        fxThickLine(cx - hw, yy, cx + hw, yy, 1.4f, c);
      }
      fxDiscRadial(cx, cy + r * 0.45f, r * 0.4f, fxRGB(255, 240, 160), fxRGB(255, 140, 0));
      break;
    }
    case EMO_PARTY: {
      fxThickLine(cx - r * 0.7f, cy + r, cx + r * 0.5f, cy - r * 0.7f, r * 0.5f, fxRGB(210, 80, 200));
      fxThickLine(cx - r * 0.7f, cy + r, cx + r * 0.6f, cy - r * 0.5f, r * 0.5f, fxRGB(120, 120, 240));
      for (int k = 0; k < 7; k++) {
        float a = k * 0.9f;
        fxDiscAA(cx + r * cosf(a) * (0.4f + 0.07f * k), cy - r * 0.4f - r * 0.6f * fabsf(sinf(a)), 2.0f,
                 fxHSV(k * 51.0f, 0.9f, 1.0f));
      }
      break;
    }
    case EMO_THUMBSUP: {
      uint16_t skin = fxRGB(245, 200, 150);
      fxEllipseAA(cx, cy + r * 0.3f, r * 0.55f, r * 0.6f, skin);      // fist
      fxThickLine(cx - r * 0.1f, cy + r * 0.1f, cx - r * 0.1f, cy - r * 0.8f, r * 0.45f, skin); // thumb
      fxDiscAA(cx - r * 0.1f, cy - r * 0.8f, r * 0.22f, skin);
      break;
    }
    case EMO_FLOWER:   drawIconFlower(cx, cy, r); break;
    case EMO_SUN: {
      fxGlow(cx, cy, r * 1.5f, COL_GOLD, 90);
      for (int k = 0; k < 8; k++) {
        float a = k * 3.14159f / 4.0f;
        fxThickLine(cx + r * 0.85f * cosf(a), cy + r * 0.85f * sinf(a),
                    cx + r * 1.3f * cosf(a), cy + r * 1.3f * sinf(a), 2.4f, COL_GOLD);
      }
      fxDiscRadial(cx, cy, r * 0.8f, fxRGB(255, 250, 180), fxRGB(255, 190, 30));
      break;
    }
    case EMO_MOON: {
      fxGlow(cx, cy, r * 1.4f, COL_GOLD, 80);
      fxDiscAA(cx, cy, r * 0.9f, fxRGB(255, 235, 150));
      fxDiscAA(cx + r * 0.45f, cy - r * 0.25f, r * 0.78f, fxRGB(20, 18, 40));
      break;
    }
    case EMO_CLOUD:
      fxDiscAA(cx - r * 0.5f, cy + r * 0.2f, r * 0.5f, fxRGB(220, 230, 245));
      fxDiscAA(cx + r * 0.5f, cy + r * 0.2f, r * 0.5f, fxRGB(220, 230, 245));
      fxDiscAA(cx, cy - r * 0.2f, r * 0.6f, fxRGB(240, 246, 255));
      break;
    case EMO_SNOW: {
      uint16_t c = fxLighten(COL_SKYBLUE, 120);
      for (int k = 0; k < 3; k++) {
        float a = k * 3.14159f / 3.0f;
        fxThickLine(cx - r * cosf(a), cy - r * sinf(a), cx + r * cosf(a), cy + r * sinf(a), 1.8f, c);
      }
      fxDiscAA(cx, cy, r * 0.18f, c);
      break;
    }
    case EMO_RAINBOW:
      for (int k = 0; k < 6; k++)
        for (float a = 3.34f; a < 6.08f; a += 0.05f)
          fxBlend(cx + (r - k) * cosf(a), cy + r * 0.6f + (r - k) * sinf(a), fxHSV(k * 50.0f, 0.9f, 1.0f), 255);
      break;
    case EMO_CAKE: {
      fxThickLine(cx, cy - r * 0.9f, cx, cy - r * 0.4f, 2.0f, COL_GOLD);
      fxDiscAA(cx, cy - r * 0.95f, r * 0.16f, fxRGB(255, 180, 60));   // candle flame
      fxEllipseAA(cx, cy + r * 0.2f, r * 0.85f, r * 0.5f, fxRGB(255, 210, 230)); // frosting
      gfx->fillRect(cx - r * 0.85f, cy + r * 0.2f, r * 1.7f, r * 0.7f, fxRGB(245, 190, 130)); // base
      break;
    }
    case EMO_GIFT: {
      gfx->fillRect(cx - r * 0.8f, cy - r * 0.5f, r * 1.6f, r * 1.4f, fxRGB(220, 70, 90));
      fxThickLine(cx, cy - r * 0.5f, cx, cy + r * 0.9f, r * 0.22f, COL_GOLD);
      fxThickLine(cx - r * 0.8f, cy, cx + r * 0.8f, cy, r * 0.22f, COL_GOLD);
      fxDiscAA(cx, cy - r * 0.5f, r * 0.22f, COL_GOLD);
      break;
    }
    default: return false;
  }
  return true;
}

// Map an emoji codepoint to a full-screen face expression (or "" if none).
String emojiToExpression(uint32_t cp) {
  switch (emojiClassify(cp)) {
    case EMO_HEARTEYES: return "love";
    case EMO_HEART:     return "heart";
    case EMO_KISS:      return "kiss";
    case EMO_LAUGH:     return "laugh";
    case EMO_GRIN: case EMO_SMILE: return "happy";
    case EMO_CRY: case EMO_SOB:    return "crying";
    case EMO_ANGRY:     return "angry";
    case EMO_SLEEPY:    return "sleepy";
    case EMO_THINK:     return "thinking";
    case EMO_SURPRISE:  return "surprised";
    case EMO_WINK:      return "wink";
    case EMO_COOL:      return "proud";
    case EMO_STAR: case EMO_SPARKLE: return "star_eyes";
    case EMO_PARTY: case EMO_FIRE:   return "excited";
    case EMO_THUMBSUP:  return "proud";
    case EMO_FLOWER:    return "grateful";
    default: return "";
  }
}

void drawNoteBorder(int style) {
  switch (style) {
    case 1:
      gfx->drawRoundRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, 12, userFaceColor);
      break;
    case 2:
      for (int x = 10; x < SCREEN_WIDTH - 10; x += 12) {
        gfx->drawLine(x, 6, x + 6, 6, userFaceColor);
        gfx->drawLine(x, SCREEN_HEIGHT - 7, x + 6, SCREEN_HEIGHT - 7, userFaceColor);
      }
      for (int y = 10; y < SCREEN_HEIGHT - 10; y += 12) {
        gfx->drawLine(6, y, 6, y + 6, userFaceColor);
        gfx->drawLine(SCREEN_WIDTH - 7, y, SCREEN_WIDTH - 7, y + 6, userFaceColor);
      }
      break;
    case 3:
      drawIconHeart(16, 16, 5);
      drawIconHeart(SCREEN_WIDTH - 16, 16, 5);
      drawIconHeart(16, SCREEN_HEIGHT - 16, 5);
      drawIconHeart(SCREEN_WIDTH - 16, SCREEN_HEIGHT - 16, 5);
      break;
    case 4:
      for (int x = 8; x < SCREEN_WIDTH; x += 10) {
        gfx->fillCircle(x, 5, 2, COL_LAVENDER);
        gfx->fillCircle(x, SCREEN_HEIGHT - 6, 2, COL_LAVENDER);
      }
      for (int y = 14; y < SCREEN_HEIGHT - 10; y += 10) {
        gfx->fillCircle(5, y, 2, COL_LAVENDER);
        gfx->fillCircle(SCREEN_WIDTH - 6, y, 2, COL_LAVENDER);
      }
      break;
    default:
      break;
  }
}

void drawNoteIcons(const String& icons, int y, int count) {
  if (icons.length() == 0 || count == 0) return;
  String names[8];
  int nameCount = 0;
  String remaining = icons;
  while (remaining.length() > 0 && nameCount < 8) {
    int comma = remaining.indexOf(',');
    if (comma < 0) {
      names[nameCount++] = remaining;
      break;
    }
    names[nameCount++] = remaining.substring(0, comma);
    remaining = remaining.substring(comma + 1);
  }
  int drawCount = nameCount > count ? count : nameCount;
  int totalW = drawCount * 24;
  int startX = (SCREEN_WIDTH - totalW) / 2 + 12;
  for (int i = 0; i < drawCount; i++) {
    int cx = startX + i * 24;
    const String& name = names[i];
    if (name == "heart")  drawIconHeart(cx, y, 5);
    else if (name == "star")   drawIconStar(cx, y, 8);
    else if (name == "flower") drawIconFlower(cx, y, 7);
    else if (name == "note")   drawIconNote(cx, y, 8);
    else if (name == "moon")   drawIconMoon(cx, y, 7);
  }
}

// Draw one character honoring the note font style (regular/bold/outline/shadow).
// Stays within the fixed 6px cell so wrapping + emoji layout are unaffected.
static void drawStyledNoteChar(int cx, int cy, char c, int fontSize) {
  const int off = fontSize < 2 ? 1 : 2;
  switch (currentNoteFontStyle) {
    case 1:  // bold — double strike
      gfx->setCursor(cx, cy);       gfx->print(c);
      gfx->setCursor(cx + 1, cy);   gfx->print(c);
      break;
    case 2:  // outline — contrasting ring, then the glyph on top
      gfx->setTextColor(fxDarken(currentNoteTextColor, 150));
      for (int dy = -off; dy <= off; dy += off)
        for (int dx = -off; dx <= off; dx += off)
          if (dx || dy) { gfx->setCursor(cx + dx, cy + dy); gfx->print(c); }
      gfx->setTextColor(currentNoteTextColor);
      gfx->setCursor(cx, cy);       gfx->print(c);
      break;
    case 3:  // shadow — soft offset drop shadow, then the glyph on top
      gfx->setTextColor(fxLighten(currentNoteTextColor == COL_FG ? COL_LAVENDER : currentNoteTextColor, 10));
      gfx->setCursor(cx + off, cy + off); gfx->print(c);
      gfx->setTextColor(currentNoteTextColor);
      gfx->setCursor(cx, cy);       gfx->print(c);
      break;
    default:
      gfx->setCursor(cx, cy);       gfx->print(c);
  }
}

void drawLineWithSymbols(const String& line, int startX, int startY, int fontSize) {
  int cx = startX;
  int cy = startY;
  int charW = 6 * fontSize;
  int charH = 8 * fontSize;
  int iconS = charH - 2;
  int emojiW = (int)(charH * 1.35f);
  int i = 0;
  while (i < (int)line.length()) {
    if (line[i] == '<' && i + 1 < (int)line.length()) {
      if (line[i+1] == '3') {
        drawIconHeart(cx + charW, cy + charH / 2, iconS / 3);
        cx += charW * 2;
        i += 2;
        continue;
      }
      if (i + 2 < (int)line.length() && line[i+2] == '>') {
        char sym = line[i+1];
        if (sym == '*') { drawIconStar(cx + charW, cy + charH / 2, iconS / 2); cx += charW * 2; i += 3; continue; }
        if (sym == '~') { drawIconFlower(cx + charW, cy + charH / 2, iconS / 3); cx += charW * 2; i += 3; continue; }
        if (sym == 'n') { drawIconNote(cx + charW, cy + charH / 2, iconS / 3); cx += charW * 2; i += 3; continue; }
        if (sym == 'm') { drawIconMoon(cx + charW, cy + charH / 2, iconS / 3); cx += charW * 2; i += 3; continue; }
      }
    }
    // Multi-byte UTF-8 → emoji glyph
    if ((uint8_t)line[i] >= 0x80) {
      int len = 1;
      uint32_t cp = utf8DecodeAt(line, i, len);
      EmojiId id = emojiClassify(cp);
      if (id != EMO_NONE && id != EMO_IGNORE) {
        drawEmoji(cp, cx + emojiW / 2, cy + charH / 2, emojiW / 2 - 1);
        cx += emojiW;
      }
      // EMO_IGNORE / EMO_NONE consume their bytes with no advance/width.
      i += len;
      continue;
    }
    drawStyledNoteChar(cx, cy, line[i], fontSize);
    cx += charW;
    i++;
  }
}

int lineVisualWidth(const String& line, int fontSize) {
  int charW = 6 * fontSize;
  int charH = 8 * fontSize;
  int emojiW = (int)(charH * 1.35f);
  int w = 0;
  int i = 0;
  while (i < (int)line.length()) {
    if (line[i] == '<' && i + 1 < (int)line.length()) {
      if (line[i+1] == '3') { w += charW * 2; i += 2; continue; }
      if (i + 2 < (int)line.length() && line[i+2] == '>') {
        char sym = line[i+1];
        if (sym == '*' || sym == '~' || sym == 'n' || sym == 'm') { w += charW * 2; i += 3; continue; }
      }
    }
    if ((uint8_t)line[i] >= 0x80) {
      int len = 1;
      uint32_t cp = utf8DecodeAt(line, i, len);
      EmojiId id = emojiClassify(cp);
      if (id != EMO_NONE && id != EMO_IGNORE) w += emojiW;
      i += len;
      continue;
    }
    w += charW;
    i++;
  }
  return w;
}

void drawWrappedText(const String& text, int fontSize, int border, const String& icons, bool pushToScreen) {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  drawNoteBorder(border);
  gfx->setTextColor(currentNoteTextColor);
  // Scale font up: size 1 on OLED → size 2 on TFT for readability
  const int safeFontSize = fontSize < 1 ? 2 : (fontSize > 4 ? 4 : fontSize + 1);
  const bool hasIcons = icons.length() > 0;
  const int topPad = hasIcons ? 30 : 10;
  const int horizontalPadding = (border > 0) ? 16 : 8;
  const int availH = SCREEN_HEIGHT - topPad - 10;
  const int lineHeight = (8 * safeFontSize) + 4;
  const int maxLines = availH / lineHeight;
  const int maxChars = (SCREEN_WIDTH - (horizontalPadding * 2)) / (6 * safeFontSize);
  String lines[maxLines > 20 ? 20 : maxLines];
  int lineCount = 0;
  String remaining = text;
  remaining.trim();

  int usableMaxLines = maxLines > 20 ? 20 : maxLines;
  while (remaining.length() > 0 && lineCount < usableMaxLines) {
    int lineLength = (int)remaining.length() > maxChars ? maxChars : (int)remaining.length();
    if (lineLength < (int)remaining.length()) {
      const int split = remaining.lastIndexOf(' ', lineLength);
      if (split > 0) lineLength = split;
    }
    String line = remaining.substring(0, lineLength);
    line.trim();
    if (line.isEmpty()) {
      line = remaining.substring(0, maxChars);
      lineLength = line.length();
    }
    lines[lineCount++] = line;
    remaining = remaining.substring(lineLength);
    remaining.trim();
  }

  gfx->setTextSize(safeFontSize);
  gfx->setTextWrap(false);
  const int totalHeight = lineCount * lineHeight;
  int cursorY = topPad + (availH - totalHeight) / 2;
  if (cursorY < topPad) cursorY = topPad;
  // User-positioned text: nudge the whole block, kept within the visible area.
  cursorY += currentNoteTextOffY;
  if (cursorY < 2) cursorY = 2;
  if (cursorY + totalHeight > SCREEN_HEIGHT - 2) cursorY = SCREEN_HEIGHT - 2 - totalHeight;
  if (cursorY < 2) cursorY = 2;

  for (int i = 0; i < lineCount; i++) {
    int w = lineVisualWidth(lines[i], safeFontSize);
    int startX = (SCREEN_WIDTH - w) / 2 + currentNoteTextOffX;
    if (startX < 2) startX = 2;
    if (startX + w > SCREEN_WIDTH - 2) startX = SCREEN_WIDTH - 2 - w;
    if (startX < 2) startX = 2;
    drawLineWithSymbols(lines[i], startX, cursorY, safeFontSize);
    cursorY += lineHeight;
  }

  if (hasIcons) drawNoteIcons(icons, 14, 8);
  if (pushToScreen) pushCanvas();
}

void renderBannerFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  gfx->setTextSize(4);
  gfx->setTextWrap(false);
  gfx->setTextColor(COL_PINK);
  gfx->setCursor(bannerOffset, SCREEN_HEIGHT / 2 - 16);
  gfx->print(currentBanner);
  pushCanvas();
}

void renderImage() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  gfx->drawBitmap(0, 0, imageBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, userFaceColor);
  pushCanvas();
}

void renderColorImage() {
  if (!displayAvailable || !colorImageBuffer) return;
  if (spriteReady && canvas) {
    // Push RGB565 pixel data directly into the canvas buffer, then to screen
    memcpy(canvas->getBuffer(), colorImageBuffer, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
    pushCanvas();
  } else {
    // Direct push to display without canvas
    tft.drawRGBBitmap(0, 0, colorImageBuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
  }
}

// ─── Face rendering (scaled 2× from 128×64) ───
// OLED face area 128×64 → TFT face area ~240×120 centered at FACE_OFFSET_Y.
// Scale factor S=1.875 for coordinates.

static inline float ease(int phase, int max) {
  float t = (float)phase / (float)max;
  return t < 0.5f ? 2.0f * t : 2.0f * (1.0f - t);
}

// ── Realistic eye: soft sclera, gradient iris, pupil, twin catchlights, lid shadow.
void drawEye(int cx, int cy, int w, int h, int r, int pupilDx, int pupilDy) {
  int ch = h < 4 ? 4 : h;
  float rx = w * 0.5f, ry = ch * 0.5f;
  uint16_t sclera = userEyeColor;
  uint16_t iris   = userAccentColor;

  // Nearly-closed eye → soft curved lid line only.
  if (ch < 13) {
    drawMouthCurve(cx, cy, w * 0.92f, 2.2f, 3.4f, fxDarken(sclera, 20));
    return;
  }

  // Clean, solid sclera — no translucent shading overlay (that read as a gray
  // hue smudged over the eyes on this panel). Depth comes from the iris + catchlights.
  fxEllipseAA(cx, cy, rx, ry, sclera);

  // Iris, clamped so it stays seated within the sclera as the gaze shifts.
  float irisR = fminf(rx, ry) * 0.78f;
  if (irisR < 4.0f) irisR = 4.0f;
  float maxDx = fmaxf(0.0f, rx - irisR - 1.0f);
  float maxDy = fmaxf(0.0f, ry - irisR - 1.0f);
  float ix = cx + fxClampf((float)pupilDx + gEyeDriftX, -maxDx, maxDx);
  float iy = cy + fxClampf((float)pupilDy + gEyeDriftY, -maxDy, maxDy);

  fxDiscAA(ix, iy, irisR, fxDarken(iris, 150));                       // limbal ring
  fxDiscRadial(ix, iy, irisR - 1.2f, fxLighten(iris, 80), fxDarken(iris, 90)); // iris body
  fxDiscAA(ix, iy, irisR * 0.46f, fxRGB(12, 10, 18));                 // pupil
  fxDiscAA(ix - irisR * 0.30f, iy - irisR * 0.34f, irisR * 0.24f, 0xFFFF);     // main catchlight
  fxDiscAA(ix + irisR * 0.28f, iy + irisR * 0.22f, irisR * 0.12f, fxLighten(iris, 200), 180); // glint
}

void drawBlinkEye(int cx, int cy, int w, int h, int r) {
  drawMouthCurve(cx, cy, w * 0.92f, 2.2f, 3.4f, fxDarken(userEyeColor, 20));
}

// Closed happy "^" eye.
void drawHappyArc(int cx, int cy, int w) {
  drawMouthCurve(cx, cy + 5, w * 0.62f, -10.0f, 4.0f, userMouthColor);
}

void drawSadArc(int cx, int cy, int w) {
  drawMouthCurve(cx, cy, w * 0.5f, 8.0f, 4.2f, userMouthColor);
  drawMouthCurve(cx, cy - 2.4f, w * 0.42f, 6.0f, 1.4f, fxLighten(userMouthColor, 90), 140); // soft top sheen
}

// Warm smile with a soft lower-lip highlight.
void drawSmile(int cx, int cy, int w) {
  drawMouthCurve(cx, cy, w, 13.0f, 4.6f, userMouthColor);
  drawMouthCurve(cx, cy + 3.0f, w * 0.84f, 9.0f, 1.6f, fxLighten(userMouthColor, 90), 150);
}

void drawOvalMouth(int cx, int cy, int rw, int rh) {
  fxEllipseAA(cx, cy, rw, rh, userMouthColor);
  fxEllipseAA(cx, cy + 1, rw - 2.0f, rh - 2.0f, fxRGB(60, 14, 24));   // dark interior
  fxEllipseAA(cx, cy + rh * 0.45f, rw * 0.5f, rh * 0.32f, COL_ROSE, 200); // tongue hint
}

void drawKissLips(int cx, int cy) {
  uint16_t lip = userMouthColor == COL_FG ? COL_ROSE : userMouthColor;
  fxDiscAA(cx - 5, cy, 7, lip);
  fxDiscAA(cx + 5, cy, 7, lip);
  fxDiscAA(cx, cy, 6, fxDarken(lip, 60));
  fxThickLine(cx - 4, cy, cx + 4, cy, 2.0f, fxDarken(lip, 110));
  fxDiscAA(cx - 4, cy - 2, 2, fxLighten(lip, 140), 180);
}

void drawBigHeart(int cx, int cy, int s) {
  uint16_t c = COL_ROSE;
  fxDiscAA(cx - s * 0.55f, cy - s * 0.35f, s * 0.72f, c);
  fxDiscAA(cx + s * 0.55f, cy - s * 0.35f, s * 0.72f, c);
  // body
  for (int yy = (int)(cy - s * 0.3f); yy <= (int)(cy + s * 1.7f); yy++) {
    float ty = ((float)yy - (cy - s * 0.3f)) / (s * 2.0f);
    float hw = s * 1.25f * (1.0f - ty);
    if (hw < 0) hw = 0;
    fxThickLine(cx - hw, yy, cx + hw, yy, 1.4f, c);
  }
  fxDiscAA(cx - s * 0.5f, cy - s * 0.45f, s * 0.26f, fxLighten(c, 150), 200); // highlight
}

void drawTear(int cx, int cy, int s) {
  uint16_t c = COL_SKYBLUE;
  fxDiscAA(cx, cy + s * 0.4f, s, c);
  fxThickLine(cx, cy - s, cx, cy + s * 0.4f, s * 0.9f, c);
  fxDiscAA(cx - s * 0.35f, cy + s * 0.2f, s * 0.32f, fxLighten(c, 160), 200);
}

// Tapered brow: thick through the middle, feathering to soft points at each end —
// far more refined than a flat bar, so it reads as crafted as the eyes.
void drawBrow(int x1, int y1, int x2, int y2) {
  const uint16_t col = userFaceColor;
  const int N = 8;
  float px = (float)x1, py = (float)y1;
  for (int i = 1; i <= N; i++) {
    float t = (float)i / N;
    float x = x1 + (x2 - x1) * t;
    float y = y1 + (y2 - y1) * t;
    float tc = (i - 0.5f) / N;                       // segment-center fraction
    float taper = 1.0f - fabsf(tc - 0.5f) * 2.0f;    // 0 at the ends, 1 at the middle
    fxThickLine(px, py, x, y, 2.2f + taper * 3.2f, col);   // ~2.2px ends → ~5.4px center
    px = x; py = y;
  }
}

// Soft rosy cheek blush — used by affectionate/shy expressions.
static void drawBlush(int cx, int cy, int s) {
  fxGlow(cx, cy, s, COL_ROSE, 150);
  fxGlow(cx, cy, s * 0.6f, COL_PINK, 120);
}

void drawCompanionAccessories(int leftX, int rightX, int eyeY, int mouthY, int headwearLift = 0) {
  const int faceCenterX = (leftX + rightX) / 2;
  const int headwearSize = clampAppearancePercent(companionHeadwearSize);
  const int headwearWidth = scaleByPercent(headwearSize, companionHeadwearWidth);
  const int headwearHeight = scaleByPercent(headwearSize, companionHeadwearHeight);
  const int headwearCenterX = faceCenterX + clampAppearanceOffset(companionHeadwearOffsetX) * 2;
  // Per-expression lift keeps hats clear of raised brows / wide eyes / crown bursts.
  const int headwearBaseY = eyeY - headwearLift + clampAppearanceOffset(companionHeadwearOffsetY) * 2;
  const int hairSize = clampAppearancePercent(companionHairSize);
  const int mustacheSize = clampAppearancePercent(companionMustacheSize);
  const int hairWidth = scaleByPercent(hairSize, companionHairWidth);
  const int hairHeight = scaleByPercent(hairSize, companionHairHeight);
  const int hairThickness = clampAppearancePercent(companionHairThickness);
  const int hairStroke = 2 + scaleByPercent(3, hairThickness);
  const int hairCenterX = faceCenterX + clampAppearanceOffset(companionHairOffsetX) * 2;
  const int hairCenterY = eyeY + clampAppearanceOffset(companionHairOffsetY) * 2;
  const int mustacheWidth = scaleByPercent(mustacheSize, companionMustacheWidth);
  const int mustacheHeight = scaleByPercent(mustacheSize, companionMustacheHeight);
  const int mustacheThickness = clampAppearancePercent(companionMustacheThickness);
  const int mustacheStroke = 2 + scaleByPercent(3, mustacheThickness);
  const int mustacheCenterX = faceCenterX + clampAppearanceOffset(companionMustacheOffsetX) * 2;
  const int mustacheCenterY = mouthY + clampAppearanceOffset(companionMustacheOffsetY) * 2;

  // Shared shading helpers for premium add-ons.
  const float  hairW    = (float)hairStroke + 2.0f;
  const float  stacheW  = (float)mustacheStroke + 1.6f;
  const uint16_t hairLite = fxLighten(userHairColor, 80);
  const uint16_t hairDark = fxDarken(userHairColor, 55);
  const uint16_t hatLite  = fxLighten(userHatColor, 85);
  const uint16_t hatDark  = fxDarken(userHatColor, 60);
  const uint16_t earInner = COL_PINK;

  // ── Ears (filled + soft inner) ──
  if (companionEars == "cat") {
    gfx->fillTriangle(leftX - 30, eyeY - 34, leftX - 14, eyeY - 56, leftX + 4, eyeY - 34, userHairColor);
    gfx->fillTriangle(rightX - 4, eyeY - 34, rightX + 14, eyeY - 56, rightX + 30, eyeY - 34, userHairColor);
    gfx->fillTriangle(leftX - 22, eyeY - 37, leftX - 14, eyeY - 50, leftX - 6, eyeY - 37, earInner);
    gfx->fillTriangle(rightX + 6, eyeY - 37, rightX + 14, eyeY - 50, rightX + 22, eyeY - 37, earInner);
  } else if (companionEars == "bear") {
    fxDiscRadial(leftX - 22, eyeY - 38, 11, hairLite, hairDark);
    fxDiscRadial(rightX + 22, eyeY - 38, 11, hairLite, hairDark);
    fxDiscAA(leftX - 22, eyeY - 38, 5, earInner);
    fxDiscAA(rightX + 22, eyeY - 38, 5, earInner);
  } else if (companionEars == "bunny") {
    fxEllipseAA(leftX - 21, eyeY - 49, 7, 16, userHairColor);
    fxEllipseAA(rightX + 21, eyeY - 49, 7, 16, userHairColor);
    fxEllipseAA(leftX - 21, eyeY - 49, 3.4f, 12, earInner);
    fxEllipseAA(rightX + 21, eyeY - 49, 3.4f, 12, earInner);
  }

  // ── Hair (anti-aliased strands + highlights) ──
  if (companionHair == "tuft") {
    const int lift = scaleByPercent(22, hairHeight);
    const int spread = scaleByPercent(9, hairWidth);
    fxThickLine(hairCenterX - spread, hairCenterY - 22, hairCenterX, hairCenterY - 22 - lift, hairW, userHairColor);
    fxThickLine(hairCenterX, hairCenterY - 22 - lift, hairCenterX + spread, hairCenterY - 22, hairW, userHairColor);
    fxThickLine(hairCenterX, hairCenterY - 22 - lift, hairCenterX + 2, hairCenterY - 14, hairW, userHairColor);
    fxThickLine(hairCenterX - 1, hairCenterY - 24, hairCenterX, hairCenterY - 22 - lift + 4, hairW * 0.45f, hairLite, 150);
  } else if (companionHair == "bangs") {
    const int topY = hairCenterY - 22 - scaleByPercent(5, hairHeight);
    const int leftEdge = hairCenterX - (scaleByPercent(68, hairWidth) / 2);
    const int rightEdge = hairCenterX + (scaleByPercent(68, hairWidth) / 2);
    gfx->fillRoundRect(leftEdge, topY - 4, rightEdge - leftEdge, 14, 6, userHairColor);
    for (int x = leftX - 26; x <= rightX + 26; x += 13) {
      const int shiftedX = hairCenterX + (x - faceCenterX);
      fxThickLine(shiftedX, topY + 2, shiftedX + scaleByPercent(5, hairWidth), hairCenterY - 12, hairW, userHairColor);
    }
    fxThickLine(leftEdge + 6, topY, rightEdge - 6, topY, 2.0f, hairLite, 150);
  } else if (companionHair == "spiky") {
    for (int x = leftX - 26; x <= rightX + 26; x += 18) {
      const int shiftedX = hairCenterX + (x - faceCenterX);
      const int peak = hairCenterY - 18 - scaleByPercent(16, hairHeight);
      gfx->fillTriangle(shiftedX, hairCenterY - 16, shiftedX + scaleByPercent(7, hairWidth), peak, shiftedX + scaleByPercent(14, hairWidth), hairCenterY - 16, userHairColor);
      fxThickLine(shiftedX + scaleByPercent(7, hairWidth), peak, shiftedX + scaleByPercent(6, hairWidth), hairCenterY - 16, 1.4f, hairLite, 140);
    }
  } else if (companionHair == "swoop") {
    const int topY = hairCenterY - 22 - scaleByPercent(11, hairHeight);
    fxThickLine(hairCenterX - scaleByPercent(34, hairWidth), hairCenterY - 16, hairCenterX + scaleByPercent(22, hairWidth), topY, hairW + 1.0f, userHairColor);
    fxThickLine(hairCenterX + scaleByPercent(22, hairWidth), topY, hairCenterX + scaleByPercent(52, hairWidth), hairCenterY - 9, hairW + 1.0f, userHairColor);
    fxThickLine(hairCenterX - scaleByPercent(18, hairWidth), hairCenterY - 18, hairCenterX + scaleByPercent(7, hairWidth), topY + 3, hairW, hairLite, 160);
  } else if (companionHair == "bob") {
    const int topY = hairCenterY - 20 - scaleByPercent(7, hairHeight);
    const int width = scaleByPercent((rightX - leftX) + 68, hairWidth);
    gfx->fillRoundRect(hairCenterX - width / 2, topY, width, 18 + scaleByPercent(7, hairHeight), 9, userHairColor);
    fxThickLine(hairCenterX - width / 2, hairCenterY - 2, hairCenterX - width / 2 + scaleByPercent(11, hairWidth), hairCenterY + 14, hairW + 1.0f, userHairColor);
    fxThickLine(hairCenterX + width / 2, hairCenterY - 2, hairCenterX + width / 2 - scaleByPercent(11, hairWidth), hairCenterY + 14, hairW + 1.0f, userHairColor);
    fxThickLine(hairCenterX - width / 2 + 6, topY + 4, hairCenterX + width / 2 - 6, topY + 4, 2.4f, hairLite, 150);
  } else if (companionHair == "messy") {
    for (int x = leftX - 18; x <= rightX + 22; x += 16) {
      const int shiftedX = hairCenterX + (x - faceCenterX);
      const int peak = hairCenterY - 14 - scaleByPercent(13, hairHeight) + ((x / 16) % 2 == 0 ? 0 : 5);
      fxThickLine(shiftedX, hairCenterY - 14, shiftedX + scaleByPercent(5, hairWidth), peak, hairW, userHairColor);
      fxThickLine(shiftedX + scaleByPercent(5, hairWidth), peak, shiftedX + scaleByPercent(11, hairWidth), hairCenterY - 16, hairW, userHairColor);
    }
  } else if (companionHair == "ponytail") {
    const int topY = hairCenterY - 22 - scaleByPercent(5, hairHeight);
    const int leftEdge = hairCenterX - (scaleByPercent(68, hairWidth) / 2);
    const int rightEdge = hairCenterX + (scaleByPercent(68, hairWidth) / 2);
    gfx->fillRoundRect(leftEdge, topY - 3, rightEdge - leftEdge, 11, 5, userHairColor);
    fxThickLine(rightEdge, topY, rightEdge + scaleByPercent(15, hairWidth), topY + 8, hairW + 1.0f, userHairColor);
    fxThickLine(rightEdge + scaleByPercent(15, hairWidth), topY + 8, rightEdge + scaleByPercent(11, hairWidth), topY + 30, hairW + 1.0f, userHairColor);
    fxDiscAA(rightEdge, topY + 2, 4, fxDarken(userHairColor, 30));
  } else if (companionHair == "curly") {
    for (int i = 0; i < 5; i++) {
      int cx = hairCenterX - scaleByPercent(22, hairWidth) + i * scaleByPercent(11, hairWidth);
      int cy = hairCenterY - 22 - scaleByPercent(5, hairHeight) + (i % 2 == 1 ? 3 : 0);
      int cr = scaleByPercent(6, hairHeight) + i;
      fxDiscRadial(cx, cy, cr, hairLite, hairDark);
    }
  } else if (companionHair == "pigtails") {
    const int topY = hairCenterY - 20;
    const int leftPigX = hairCenterX - scaleByPercent(40, hairWidth);
    const int rightPigX = hairCenterX + scaleByPercent(40, hairWidth);
    fxThickLine(leftPigX + 8, topY - 2, leftPigX, topY + scaleByPercent(18, hairHeight), hairW + 1.0f, userHairColor);
    fxThickLine(rightPigX - 8, topY - 2, rightPigX, topY + scaleByPercent(18, hairHeight), hairW + 1.0f, userHairColor);
    fxDiscAA(leftPigX + 8, topY - 5, 4, fxDarken(userHairColor, 30));
    fxDiscAA(rightPigX - 8, topY - 5, 4, fxDarken(userHairColor, 30));
  } else if (companionHair == "mohawk") {
    const int height = scaleByPercent(22, hairHeight);
    int mw = scaleByPercent(11, hairWidth);
    gfx->fillTriangle(hairCenterX - mw, hairCenterY - 16, hairCenterX, hairCenterY - 18 - height, hairCenterX + mw, hairCenterY - 16, userHairColor);
    fxThickLine(hairCenterX, hairCenterY - 16, hairCenterX, hairCenterY - 18 - height, 1.6f, hairLite, 150);
  }

  // ── Headwear (filled + shading + accents) ──
  if (companionHeadwear == "bow") {
    const int knotR = scaleByPercent(5, headwearSize);
    const int outerWing = scaleByPercent(30, headwearWidth) / 2;
    const int innerWing = scaleByPercent(14, headwearWidth) / 2;
    const int topY = headwearBaseY - scaleByPercent(18, headwearHeight);
    const int midY = headwearBaseY - scaleByPercent(8, headwearHeight);
    const int bottomY = headwearBaseY + scaleByPercent(4, headwearHeight);
    gfx->fillTriangle(headwearCenterX - knotR * 2, topY, headwearCenterX - outerWing, midY, headwearCenterX - innerWing, bottomY, userHatColor);
    gfx->fillTriangle(headwearCenterX + knotR * 2, topY, headwearCenterX + outerWing, midY, headwearCenterX + innerWing, bottomY, userHatColor);
    fxThickLine(headwearCenterX - outerWing + 2, midY, headwearCenterX - knotR, midY, 1.6f, hatLite, 140);
    fxDiscRadial(headwearCenterX, midY, knotR, hatLite, hatDark);
  } else if (companionHeadwear == "beanie") {
    const int beanieW = scaleByPercent(88, headwearWidth);
    const int beanieH = scaleByPercent(24, headwearHeight);
    int top = headwearBaseY - scaleByPercent(28, headwearHeight);
    gfx->fillRoundRect(headwearCenterX - beanieW / 2, top, beanieW, beanieH, 10, userHatColor);
    gfx->fillRect(headwearCenterX - beanieW / 2, headwearBaseY - scaleByPercent(8, headwearHeight), beanieW, scaleByPercent(6, headwearHeight), hatDark);  // brim band
    fxThickLine(headwearCenterX - beanieW / 2 + 6, top + 5, headwearCenterX + beanieW / 2 - 6, top + 5, 2.0f, hatLite, 130);
    fxDiscRadial(headwearCenterX, top - 2, scaleByPercent(6, headwearSize), hatLite, hatDark);  // pom
  } else if (companionHeadwear == "crown") {
    const int baseY = headwearBaseY - scaleByPercent(8, headwearHeight);
    const int lx = headwearCenterX - scaleByPercent(38, headwearWidth);
    const int rx = headwearCenterX + scaleByPercent(38, headwearWidth);
    uint16_t gold = (userHatColor == COL_FG) ? COL_GOLD : userHatColor;
    gfx->fillTriangle(lx, baseY, headwearCenterX - scaleByPercent(22, headwearWidth), headwearBaseY - scaleByPercent(30, headwearHeight), headwearCenterX - scaleByPercent(4, headwearWidth), baseY, gold);
    gfx->fillTriangle(headwearCenterX - scaleByPercent(4, headwearWidth), baseY, headwearCenterX + scaleByPercent(11, headwearWidth), headwearBaseY - scaleByPercent(34, headwearHeight), headwearCenterX + scaleByPercent(26, headwearWidth), baseY, gold);
    gfx->fillTriangle(headwearCenterX + scaleByPercent(4, headwearWidth), baseY, headwearCenterX + scaleByPercent(26, headwearWidth), headwearBaseY - scaleByPercent(30, headwearHeight), rx, baseY, gold);
    gfx->fillRect(lx, baseY - 2, rx - lx, 5, gold);
    fxDiscAA(headwearCenterX - scaleByPercent(22, headwearWidth), headwearBaseY - scaleByPercent(28, headwearHeight), 2.4f, COL_ROSE);
    fxDiscAA(headwearCenterX + scaleByPercent(11, headwearWidth), headwearBaseY - scaleByPercent(32, headwearHeight), 2.6f, COL_SKYBLUE);
    fxDiscAA(headwearCenterX + scaleByPercent(26, headwearWidth), headwearBaseY - scaleByPercent(28, headwearHeight), 2.4f, COL_MINT);
  } else if (companionHeadwear == "top_hat") {
    const int hatW = scaleByPercent(44, headwearWidth);
    const int hatH = scaleByPercent(44, headwearHeight);
    const int brimW = scaleByPercent(76, headwearWidth);
    int top = headwearBaseY - scaleByPercent(52, headwearHeight);
    gfx->fillRoundRect(headwearCenterX - hatW / 2, top, hatW, hatH, 4, userHatColor);
    gfx->fillRect(headwearCenterX - hatW / 2, headwearBaseY - scaleByPercent(20, headwearHeight), hatW, scaleByPercent(8, headwearHeight), COL_ROSE);  // band
    fxThickLine(headwearCenterX - brimW / 2, headwearBaseY - scaleByPercent(8, headwearHeight), headwearCenterX + brimW / 2, headwearBaseY - scaleByPercent(8, headwearHeight), 4.0f, userHatColor);
    fxThickLine(headwearCenterX - hatW / 2 + 3, top + 4, headwearCenterX - hatW / 2 + 3, top + hatH - 4, 2.0f, hatLite, 120);
  } else if (companionHeadwear == "halo") {
    int cy = headwearBaseY - scaleByPercent(26, headwearHeight);
    fxGlow(headwearCenterX, cy, scaleByPercent(46, headwearWidth) / 2.0f, COL_GOLD, 80);
    fxRingAA(headwearCenterX, cy, scaleByPercent(40, headwearWidth) / 2.0f, 4.0f, COL_GOLD);
  } else if (companionHeadwear == "flower_crown") {
    static const uint16_t FC[] = { COL_ROSE, COL_GOLD, COL_PINK, COL_LAVENDER, COL_SKYBLUE };
    for (int i = 0; i < 5; i++) {
      int fx = headwearCenterX - scaleByPercent(32, headwearWidth) + i * scaleByPercent(16, headwearWidth);
      int fy = headwearBaseY - scaleByPercent(12, headwearHeight) + (i % 2 == 1 ? scaleByPercent(3, headwearHeight) : 0);
      int pr = scaleByPercent(4, headwearSize); if (pr < 2) pr = 2;
      for (int p = 0; p < 5; p++) {
        float a = p * 1.2566f - 1.5708f;
        fxDiscAA(fx + pr * cosf(a), fy + pr * sinf(a), pr * 0.6f, FC[i]);
      }
      fxDiscAA(fx, fy, pr * 0.55f, COL_GOLD);
    }
  } else if (companionHeadwear == "beret") {
    int top = headwearBaseY - scaleByPercent(26, headwearHeight);
    gfx->fillRoundRect(headwearCenterX - scaleByPercent(44, headwearWidth), top, scaleByPercent(88, headwearWidth), scaleByPercent(22, headwearHeight), 11, userHatColor);
    fxThickLine(headwearCenterX - scaleByPercent(30, headwearWidth), top + 5, headwearCenterX + scaleByPercent(10, headwearWidth), top + 5, 2.4f, hatLite, 130);
    fxDiscAA(headwearCenterX - scaleByPercent(7, headwearWidth), top - 2, scaleByPercent(3, headwearSize) + 1, hatDark);
  }

  // ── Glasses (anti-aliased lenses + faint tint + bridge) ──
  if (companionGlasses == "round") {
    fxDiscAA(leftX, eyeY, 20, fxRGB(90, 140, 170), 35);
    fxDiscAA(rightX, eyeY, 20, fxRGB(90, 140, 170), 35);
    fxRingAA(leftX, eyeY, 21, 3.0f, userFaceColor);
    fxRingAA(rightX, eyeY, 21, 3.0f, userFaceColor);
    fxThickLine(leftX + 21, eyeY, rightX - 21, eyeY, 2.4f, userFaceColor);
    fxThickLine(leftX - 21, eyeY, leftX - 30, eyeY - 4, 2.0f, userFaceColor);
    fxThickLine(rightX + 21, eyeY, rightX + 30, eyeY - 4, 2.0f, userFaceColor);
  } else if (companionGlasses == "square") {
    gfx->drawRoundRect(leftX - 26, eyeY - 20, 52, 40, 8, userFaceColor);
    gfx->drawRoundRect(leftX - 25, eyeY - 19, 50, 38, 7, userFaceColor);
    gfx->drawRoundRect(rightX - 26, eyeY - 20, 52, 40, 8, userFaceColor);
    gfx->drawRoundRect(rightX - 25, eyeY - 19, 50, 38, 7, userFaceColor);
    fxThickLine(leftX + 26, eyeY, rightX - 26, eyeY, 2.4f, userFaceColor);
  } else if (companionGlasses == "visor") {
    int vx = leftX - 34, vw = (rightX - leftX) + 68;
    gfx->fillRoundRect(vx, eyeY - 18, vw, 32, 12, fxRGB(40, 70, 95));
    fxThickLine(vx + 8, eyeY - 12, vx + vw - 8, eyeY - 12, 2.4f, fxRGB(150, 200, 230), 160);  // sheen
    gfx->drawRoundRect(vx, eyeY - 18, vw, 32, 12, userFaceColor);
  }

  // ── Mustache (anti-aliased) ──
  if (companionMustache == "classic") {
    const int wing = scaleByPercent(22, mustacheWidth);
    const int inner = scaleByPercent(7, mustacheWidth);
    const int rise = scaleByPercent(7, mustacheHeight);
    fxThickLine(mustacheCenterX - wing, mustacheCenterY - rise, mustacheCenterX - inner, mustacheCenterY, stacheW + 1.0f, userMustacheColor);
    fxThickLine(mustacheCenterX + inner, mustacheCenterY, mustacheCenterX + wing, mustacheCenterY - rise, stacheW + 1.0f, userMustacheColor);
  } else if (companionMustache == "curled") {
    const int wing = scaleByPercent(22, mustacheWidth);
    const int rise = scaleByPercent(4, mustacheHeight);
    fxThickLine(mustacheCenterX - wing, mustacheCenterY - rise, mustacheCenterX - 4, mustacheCenterY - 2, stacheW, userMustacheColor);
    fxThickLine(mustacheCenterX + 4, mustacheCenterY - 2, mustacheCenterX + wing, mustacheCenterY - rise, stacheW, userMustacheColor);
    fxRingAA(mustacheCenterX - wing - 3, mustacheCenterY - rise - 1, 4, 2.0f, userMustacheColor);
    fxRingAA(mustacheCenterX + wing + 3, mustacheCenterY - rise - 1, 4, 2.0f, userMustacheColor);
  } else if (companionMustache == "handlebar") {
    const int wing = scaleByPercent(26, mustacheWidth);
    const int curl = scaleByPercent(9, mustacheHeight);
    fxThickLine(mustacheCenterX - wing, mustacheCenterY - 1, mustacheCenterX - 4, mustacheCenterY, stacheW, userMustacheColor);
    fxThickLine(mustacheCenterX + 4, mustacheCenterY, mustacheCenterX + wing, mustacheCenterY - 1, stacheW, userMustacheColor);
    fxThickLine(mustacheCenterX - wing, mustacheCenterY - 1, mustacheCenterX - wing - scaleByPercent(7, mustacheWidth), mustacheCenterY - curl, stacheW, userMustacheColor);
    fxThickLine(mustacheCenterX + wing, mustacheCenterY - 1, mustacheCenterX + wing + scaleByPercent(7, mustacheWidth), mustacheCenterY - curl, stacheW, userMustacheColor);
  } else if (companionMustache == "walrus") {
    const int width = scaleByPercent(26, mustacheWidth);
    const int height = scaleByPercent(7, mustacheHeight);
    fxEllipseAA(mustacheCenterX, mustacheCenterY - 6, width, height + 4, userMustacheColor);
    fxThickLine(mustacheCenterX, mustacheCenterY - 8, mustacheCenterX, mustacheCenterY + height, 2.0f, fxDarken(userMustacheColor, 50));
  } else if (companionMustache == "pencil") {
    const int width = scaleByPercent(24, mustacheWidth);
    fxThickLine(mustacheCenterX - width, mustacheCenterY - 3, mustacheCenterX + width, mustacheCenterY - 3, stacheW * 0.8f, userMustacheColor);
  } else if (companionMustache == "imperial") {
    const int wing = scaleByPercent(22, mustacheWidth);
    const int rise = scaleByPercent(16, mustacheHeight);
    fxThickLine(mustacheCenterX - wing, mustacheCenterY - 3, mustacheCenterX - 2, mustacheCenterY - 1, stacheW, userMustacheColor);
    fxThickLine(mustacheCenterX + 2, mustacheCenterY - 1, mustacheCenterX + wing, mustacheCenterY - 3, stacheW, userMustacheColor);
    fxThickLine(mustacheCenterX - wing, mustacheCenterY - 3, mustacheCenterX - wing - scaleByPercent(4, mustacheWidth), mustacheCenterY - rise, stacheW, userMustacheColor);
    fxThickLine(mustacheCenterX + wing, mustacheCenterY - 3, mustacheCenterX + wing + scaleByPercent(4, mustacheWidth), mustacheCenterY - rise, stacheW, userMustacheColor);
  } else if (companionMustache == "goatee") {
    const int gWidth = scaleByPercent(11, mustacheWidth);
    const int gHeight = scaleByPercent(16, mustacheHeight);
    gfx->fillTriangle(mustacheCenterX - gWidth, mustacheCenterY + 2, mustacheCenterX + gWidth, mustacheCenterY + 2, mustacheCenterX, mustacheCenterY + gHeight + 5, userMustacheColor);
  } else if (companionMustache == "soul_patch") {
    const int pWidth = scaleByPercent(5, mustacheWidth);
    const int pHeight = scaleByPercent(8, mustacheHeight);
    fxEllipseAA(mustacheCenterX, mustacheCenterY + 4 + pHeight / 2.0f, pWidth, pHeight / 2.0f + 1, userMustacheColor);
  }

  // ── Piercings (metallic studs with a highlight) ──
  uint16_t metal = fxRGB(220, 224, 235);
  if (companionPiercing == "brow") {
    fxThickLine(rightX + 11, eyeY - 26, rightX + 26, eyeY - 22, 2.0f, fxDarken(metal, 40));
    fxDiscAA(rightX + 12, eyeY - 25, 2.4f, metal); fxDiscAA(rightX + 25, eyeY - 22, 2.4f, metal);
    fxDiscAA(rightX + 12, eyeY - 26, 0.9f, 0xFFFF);
  } else if (companionPiercing == "nose") {
    fxRingAA(faceCenterX + 7, mouthY - 18, 4, 1.8f, metal);
    fxDiscAA(faceCenterX + 7 - 2, mouthY - 20, 0.9f, 0xFFFF);
  } else if (companionPiercing == "lip") {
    fxDiscAA(faceCenterX + 14, mouthY + 4, 3, metal);
    fxDiscAA(faceCenterX + 13, mouthY + 3, 1.0f, 0xFFFF);
  }
}

void drawZzz(int x, int y, int phase) {
  int p = phase % 64;
  gfx->setTextSize(2);
  gfx->setTextColor(COL_LAVENDER);
  int drift = p / 4;
  if (p >= 8)  { gfx->setCursor(x,      y + 22 - drift * 2); gfx->print('z'); }
  if (p >= 24) { gfx->setCursor(x + 12, y + 10 - drift);     gfx->print('z'); }
  if (p >= 40) { gfx->setCursor(x + 26, y - drift / 2);      gfx->print('Z'); }
}

// ─── Expressive helpers (soft-shaded) ───

void drawVeinMark(int cx, int cy, int s) {
  // Anime-style frustration cross — bold red, anti-aliased.
  uint16_t c = COL_ROSE;
  fxThickLine(cx - s, cy - s / 3, cx + s, cy + s / 3, 2.6f, c);
  fxThickLine(cx - s, cy + s / 3, cx + s, cy - s / 3, 2.6f, c);
  fxThickLine(cx - s / 3, cy - s, cx + s / 3, cy + s, 2.6f, c);
  fxThickLine(cx + s / 3, cy - s, cx - s / 3, cy + s, 2.6f, c);
}

void drawTeethMouth(int cx, int cy, int w, int h) {
  // Open laughing mouth: dark cavity, top teeth, pink tongue.
  float rw = w * 0.5f, rh = h * 0.5f;
  fxEllipseAA(cx, cy, rw, rh, fxDarken(userMouthColor, 40));
  fxEllipseAA(cx, cy + 1, rw - 2.0f, rh - 2.0f, fxRGB(70, 16, 26));
  // Tongue
  fxEllipseAA(cx, cy + rh * 0.5f, rw * 0.6f, rh * 0.42f, COL_ROSE);
  fxEllipseAA(cx, cy + rh * 0.5f, rw * 0.6f, rh * 0.42f, fxDarken(COL_ROSE, 40), 90);
  // Top teeth band
  fxThickLine(cx - rw * 0.78f, cy - rh * 0.62f, cx + rw * 0.78f, cy - rh * 0.62f, fmaxf(3.0f, rh * 0.34f), 0xFFFF);
  int teeth = w > 36 ? 4 : 3;
  for (int i = 1; i < teeth; i++) {
    float tx = cx - rw * 0.78f + (i * (rw * 1.56f) / teeth);
    fxThickLine(tx, cy - rh * 0.78f, tx, cy - rh * 0.42f, 1.0f, fxRGB(180, 180, 190));
  }
}

void drawHeartEye(int cx, int cy, int s) {
  drawBigHeart(cx, cy, s);
}

void drawSteamPuff(int cx, int cy, int r) {
  // Puffy cloud of frustration steam.
  fxDiscAA(cx, cy, r, fxLighten(COL_SKYBLUE, 60), 220);
  fxDiscAA(cx + r, cy - r / 2, r - 1, fxLighten(COL_SKYBLUE, 60), 200);
  fxDiscAA(cx - r, cy - r / 2, r - 1, fxLighten(COL_SKYBLUE, 60), 200);
}

// ─── Expression renderer (scaled 2× from mini, face centered at FACE_OFFSET_Y) ───

// Soft dimensional backdrop: deep gradient + warm glow behind the face for depth.
static void drawFaceBackdrop(int faceCX, int faceCY, uint16_t glowColor, uint8_t glowAlpha) {
  // Cheap mood-tinted gradient only — NO big radial glow. The full-screen glow
  // looked like bloom and cost ~70k PSRAM read-modify-writes per frame (the cause
  // of the low framerate). A subtle top tint gives mood without the cost.
  (void)faceCX; (void)faceCY; (void)glowAlpha; (void)glowColor;
  // True black — premium AMOLED look that makes the eyes/colors pop, no banding,
  // and (being byte-symmetric 0x0000) fills via fast memset.
  gfx->fillScreen(COL_BG);
}

void renderExpressionFrame() {
  if (!displayAvailable) return;

  // Companion scale (50-200%) applied to face size
  const float cScale = companionScale / 100.0f;
  const int ph = expressionPhase % 64;
  const float t = (float)ph / 63.0f;

  // ── "Alive" idle motion applied to EVERY expression ──
  // Smooth, SUB-PIXEL breathing/float via gFxOff* (the AA primitives offset by it),
  // driven by a continuous millis() clock — not the 64-step phase — so it glides
  // instead of stepping. Plus a gentle wandering gaze so the eyes never freeze.
  const float fclk = (float)millis() * 0.0019f;
  gFxOffY = sinf(fclk) * 5.0f;
  gFxOffX = sinf(fclk + 1.1f) * 2.5f;
  gEyeDriftX = sinf(t * 3.14159f * 2.0f) * 3.0f + sinf(t * 3.14159f * 6.0f) * 1.0f;
  gEyeDriftY = cosf(t * 3.14159f * 4.0f) * 2.0f;

  const int companionXShift = clampAppearanceOffset(companionOffsetX) * 2;
  const int companionYShift = clampAppearanceOffset(companionOffsetY) * 2;

  // Scaled face coordinates. Eyes are placed SYMMETRICALLY about the face center
  // (CX) so the eye cluster, brows, accessories and mouth all share one centerline.
  // (Previously the eyes were centered at x≈120 while the mouth sat at 160, which
  // left every face looking off — the mouth read as off-center.)
  const int baseLX = 68, baseRX = 172;
  const int eyeHalf = (int)((baseRX - baseLX) * 0.5f * cScale);  // 52*scale apart from center
  const int centerX = SCREEN_WIDTH / 2;
  const int CX = centerX + companionXShift;
  const int eyeXShift = clampAppearanceOffset(companionEyeOffsetX) * 2;
  const int LX = CX - eyeHalf + eyeXShift;
  const int RX = CX + eyeHalf + eyeXShift;
  const int eyeYShift = clampAppearanceOffset(companionEyeOffsetY) * 2;
  const int mouthXShift = clampAppearanceOffset(companionMouthOffsetX) * 2;
  const int mouthYShift = clampAppearanceOffset(companionMouthOffsetY) * 2;
  const int EY = FACE_OFFSET_Y + companionYShift + (int)(45 * cScale) + eyeYShift;
  const int MX = CX + mouthXShift;
  const int MY = FACE_OFFSET_Y + companionYShift + (int)(100 * cScale) + mouthYShift;
  const int EW = (int)(52 * cScale);
  const int EH = (int)(40 * cScale);
  const int ER = (int)(13 * cScale);

  // Mood-tinted dimensional backdrop behind the face.
  uint16_t glowCol = userAccentColor;
  if (currentExpression == "love" || currentExpression == "heart" ||
      currentExpression == "kiss" || currentExpression == "blushing") glowCol = COL_ROSE;
  else if (currentExpression == "sleepy" || currentExpression == "peaceful") glowCol = COL_LAVENDER;
  else if (currentExpression == "angry") glowCol = COL_ROSE;
  else if (currentExpression == "excited" || currentExpression == "star_eyes" ||
           currentExpression == "proud" || currentExpression == "laugh") glowCol = COL_GOLD;
  else if (currentExpression == "sad" || currentExpression == "crying" ||
           currentExpression == "nervous") glowCol = COL_SKYBLUE;
  const int faceCenterY = FACE_OFFSET_Y + companionYShift + (int)(72 * cScale);
  drawFaceBackdrop(CX, faceCenterY, glowCol, 70);

  if (currentExpression == "heart") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int s = 14 + (int)(10.0f * wave);
    drawBigHeart(CX, FACE_OFFSET_Y + companionYShift + 80, s);
  } else if (currentExpression == "love") {
    float beat = sin(t * 3.14159f * 4.0f) * 0.5f + 0.5f;
    int heartS = 14 + (int)(beat * 4.0f);
    drawBlush(LX - 14, EY + 22, 15);
    drawBlush(RX + 14, EY + 22, 15);
    // Heart-shaped eyes that pulse
    drawHeartEye(LX, EY, heartS);
    drawHeartEye(RX, EY, heartS);
    // Wide love smile
    drawSmile(MX, MY - 4, 52);
    // Floating hearts from both sides
    float r1 = fmod(t * 2.0f, 1.0f);
    float r2 = fmod(t * 2.0f + 0.5f, 1.0f);
    float r3 = fmod(t * 1.5f + 0.25f, 1.0f);
    if (r1 < 0.85f) {
      int y1 = MY - 8 - (int)(r1 * 80.0f);
      drawBigHeart(companionXShift + 24 + (int)(r1 * 20.0f), y1, 5 - (int)(r1 * 2.0f));
    }
    if (r2 < 0.85f) {
      int y2 = MY - 8 - (int)(r2 * 72.0f);
      drawBigHeart(companionXShift + 216 - (int)(r2 * 20.0f), y2, 4 - (int)(r2 * 2.0f));
    }
    if (r3 < 0.8f) {
      int y3 = MY - 14 - (int)(r3 * 60.0f);
      drawBigHeart(MX + (int)(sinf(r3 * 3.14159f) * 30.0f), y3, 3);
    }
  } else if (currentExpression == "surprised") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int eyeH = EH + (int)(wave * 14.0f);
    int browLift = (int)(wave * 7.0f);
    int mouthR = 12 + (int)(wave * 6.0f);
    drawEye(LX, EY, EW + 7, eyeH, ER + 3, 0, 0);
    drawEye(RX, EY, EW + 7, eyeH, ER + 3, 0, 0);
    drawBrow(LX - 26, EY - 32 - browLift, LX + 26, EY - 32 - browLift);
    drawBrow(RX - 26, EY - 32 - browLift, RX + 26, EY - 32 - browLift);
    drawOvalMouth(MX, MY, mouthR, mouthR);
    // Exclamation marks floating above
    gfx->setTextSize(2);
    gfx->setTextColor(userAccentColor);
    int bounce1 = (int)(wave * 6.0f);
    int bounce2 = (int)((1.0f - wave) * 5.0f);
    gfx->setCursor(LX - 36, EY - 44 - bounce1);
    gfx->print("!");
    gfx->setCursor(RX + 22, EY - 40 - bounce2);
    gfx->print("!");
  } else if (currentExpression == "angry") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int lidH = 7 + (int)(wave * 5.0f);
    int twitch = (int)(wave * 4.0f);
    int mouthShift = (int)(sin(t * 3.14159f * 6.0f) * 4.0f);
    drawEye(LX, EY + 4, EW, EH - 4, ER, 0, 4);
    drawEye(RX, EY + 4, EW, EH - 4, ER, 0, 4);
    gfx->fillRect(LX - EW / 2, EY + 4 - (EH - 4) / 2, EW, lidH, COL_BG);
    gfx->fillRect(RX - EW / 2, EY + 4 - (EH - 4) / 2, EW, lidH, COL_BG);
    drawBrow(LX - 26, EY - 30, LX + 14, EY - 11 - twitch);
    drawBrow(RX + 26, EY - 30, RX - 14, EY - 11 - twitch);
    // Vein mark on forehead
    drawVeinMark(RX + 20, EY - 36, 7 + (int)(wave * 2.0f));
    // Gritted teeth mouth
    int mouthW = 36 + (int)(wave * 4.0f);
    drawTeethMouth(MX, MY + mouthShift, mouthW, 18);
    // Steam puffs from sides
    float puff = fmod(t * 3.0f, 1.0f);
    if (puff < 0.7f) {
      int px = LX - 36 - (int)(puff * 20.0f);
      int py = EY - 10 - (int)(puff * 15.0f);
      int pr = 4 + (int)((1.0f - puff) * 4.0f);
      drawSteamPuff(px, py, pr);
    }
    float puff2 = fmod(t * 3.0f + 0.4f, 1.0f);
    if (puff2 < 0.7f) {
      int px = RX + 36 + (int)(puff2 * 20.0f);
      int py = EY - 10 - (int)(puff2 * 15.0f);
      int pr = 3 + (int)((1.0f - puff2) * 4.0f);
      drawSteamPuff(px, py, pr);
    }
  } else if (currentExpression == "sad") {
    drawEye(LX, EY + 6, EW, EH - 7, ER, 0, 7);
    drawEye(RX, EY + 6, EW, EH - 7, ER, 0, 7);
    drawBrow(LX - 26, EY - 11, LX + 18, EY - 30);
    drawBrow(RX + 26, EY - 11, RX - 18, EY - 30);
    drawSadArc(MX, MY - 4, 30);
    if (ph >= 12) {
      float tearT = (float)(ph - 12) / 19.0f;
      int tearY = EY + 26 + (int)(tearT * 44.0f);
      int tearS = tearT < 0.5f ? 5 : 4;
      drawTear(LX + EW / 2 + 4, tearY, tearS);
    }
    if (ph >= 20) {
      float tearT = (float)(ph - 20) / 11.0f;
      int tearY = EY + 26 + (int)(tearT * 40.0f);
      drawTear(RX + EW / 2 + 4, tearY, 4);
    }
  } else if (currentExpression == "sleepy") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int eyeH = 7 + (int)((1.0f - wave) * 7.0f);
    // Heavy droopy eyes
    drawEye(LX, EY + 4, EW, eyeH, ER, 0, 0);
    drawEye(RX, EY + 4, EW, eyeH, ER, 0, 0);
    // Droopy eyelid overlay
    int lidDroop = EH/2 - eyeH/2 + 4;
    gfx->fillRect(LX - EW/2, EY + 4 - EH/2, EW, lidDroop, COL_BG);
    gfx->fillRect(RX - EW/2, EY + 4 - EH/2, EW, lidDroop, COL_BG);
    // Flat sleepy mouth
    drawMouthCurve(MX, MY, 26, 2.0f, 3.0f, userMouthColor);
    // Big floating Zzz
    drawZzz(companionXShift + 180, companionYShift + 18 + eyeYShift, expressionPhase);
    // Tiny drool drop
    float drool = fmod(t * 1.5f, 1.0f);
    if (drool > 0.3f) {
      int dy = MY + 6 + (int)((drool - 0.3f) * 20.0f);
      gfx->fillCircle(MX + 10, dy, 2, userFaceColor);
    }
  } else if (currentExpression == "thinking") {
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int px = 4 + (int)(wave * 9.0f);
    int py = -4 + (int)(wave * 5.0f);
    int bubble = 4 + (int)(wave * 4.0f);
    drawEye(LX, EY, EW - 11, 22, ER, px, py);
    drawEye(RX, EY, EW, EH, ER, px, py);
    // Thought bubble chain (small to large)
    gfx->fillCircle(companionXShift + 200, FACE_OFFSET_Y + companionYShift + 76 + eyeYShift, bubble, userAccentColor);
    gfx->fillCircle(companionXShift + 214, FACE_OFFSET_Y + companionYShift + 58 + eyeYShift, bubble + 3, userAccentColor);
    // Large thought bubble with "..."
    int bx = companionXShift + 228, by = FACE_OFFSET_Y + companionYShift + 32 + eyeYShift;
    int br = bubble + 8;
    gfx->fillCircle(bx, by, br, userAccentColor);
    gfx->fillCircle(bx, by, br - 2, COL_BG);
    // Dots inside bubble
    int dotPhase = (ph / 12) % 4;
    gfx->setTextSize(1);
    gfx->setTextColor(userAccentColor);
    const char* dots[] = {".", "..", "...", ".."};
    gfx->setCursor(bx - 8, by - 4);
    gfx->print(dots[dotPhase]);
    // Tilted thoughtful mouth
    fxThickLine(MX - 14, MY + 2, MX + 11, MY - 4, 3.2f, userMouthColor);
  } else if (currentExpression == "happy") {
    int eyeH = EH;
    int py = (int)(sin(t * 3.14159f * 2.0f) * 4.0f);
    if (ph >= 24 && ph <= 27) {
      float blinkT = (float)(ph - 24) / 3.0f;
      if (ph <= 25) {
        eyeH = EH - (int)(blinkT * (EH - 5));
      } else {
        eyeH = 5 + (int)(blinkT * (EH - 5));
      }
    }
    drawEye(LX, EY, EW, eyeH, ER, 0, py);
    drawEye(RX, EY, EW, eyeH, ER, 0, py);
    drawSmile(MX, MY - 5, 44);
  } else if (currentExpression == "smile") {
    drawHappyArc(LX, EY, EW);
    drawHappyArc(RX, EY, EW);
    drawSmile(MX, MY - 4, 44);
  } else if (currentExpression == "confused") {
    float wave = sin(t * 3.14159f * 2.0f);
    int browTwitch = (int)(wave * 5.0f);
    drawEye(LX, EY - 4, EW, EH + 4, ER, -4, 0);
    drawEye(RX, EY + 6, EW - 11, EH - 11, ER - 4, 4, 0);
    drawBrow(LX - 26, EY - 28 - browTwitch, LX + 18, EY - 20);
    drawBrow(RX - 14, EY - 16, RX + 26, EY - 24 + browTwitch);
    // Crooked unsure mouth
    fxThickLine(MX - 22, MY + 9, MX + 22, MY - 2, 3.2f, userMouthColor);
    // Floating "?" that bobs
    gfx->setTextSize(3);
    gfx->setTextColor(userAccentColor);
    float bob = sin(t * 3.14159f * 2.0f) * 4.0f;
    gfx->setCursor(RX + 18, EY - 48 + (int)bob);
    gfx->print("?");
  } else if (currentExpression == "look_around") {
    float sx = sin(t * 3.14159f * 2.0f);
    float sy = cos(t * 3.14159f * 4.0f) * 0.4f;
    int px = (int)(sx * 9.0f);
    int py = (int)(sy * 5.0f);
    drawEye(LX, EY, EW, EH, ER, px, py);
    drawEye(RX, EY, EW, EH, ER, px, py);
    drawMouthCurve(MX, MY, 28, 3.0f, 3.2f, userMouthColor);
  } else if (currentExpression == "kiss") {
    float r1 = fmod(t * 1.5f, 1.0f);
    float r2 = fmod(t * 1.5f + 0.45f, 1.0f);
    float r3 = fmod(t * 1.8f + 0.2f, 1.0f);
    drawBlush(LX - 14, EY + 20, 14);
    drawBlush(RX + 14, EY + 20, 14);
    drawBlinkEye(LX, EY, EW, 7, ER);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    // Pucker lips (bigger)
    drawKissLips(MX, MY);
    // Three streams of hearts
    if (r1 < 0.85f) {
      int hx = MX - 20 + (int)(sin(r1 * 3.14159f) * 16.0f);
      int hy = MY - 20 - (int)(r1 * 75.0f);
      drawBigHeart(hx, hy, 5 - (int)(r1 * 2.0f));
    }
    if (r2 < 0.85f) {
      int hx = MX + 24 - (int)(sin(r2 * 3.14159f) * 12.0f);
      int hy = MY - 16 - (int)(r2 * 66.0f);
      drawBigHeart(hx, hy, 4 - (int)(r2 * 2.0f));
    }
    if (r3 < 0.8f) {
      int hx = MX + (int)(sin(r3 * 3.14159f * 2.0f) * 10.0f);
      int hy = MY - 24 - (int)(r3 * 60.0f);
      drawBigHeart(hx, hy, 3);
    }
  } else if (currentExpression == "wink") {
    drawMouthCurve(LX, EY, EW * 0.82f, -7.0f, 3.6f, userEyeColor);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    drawSmile(MX, MY - 4, 38);
    // Sparkle near the winking eye
    float twinkle = sin(t * 3.14159f * 6.0f) * 0.5f + 0.5f;
    int sr = 4 + (int)(twinkle * 4.0f);
    drawIconStar(LX + EW/2 + 8, EY - 12, sr);
  } else if (currentExpression == "laugh") {
    float wave = sin(t * 3.14159f * 4.0f) * 0.5f + 0.5f;
    int shakeX = (int)(wave * 5.0f) - 2;
    int mouthW = 42 + (int)(wave * 10.0f);
    int mouthH = 24 + (int)(wave * 6.0f);
    drawHappyArc(LX + shakeX, EY, EW);
    drawHappyArc(RX + shakeX, EY, EW);
    // Big open mouth with teeth
    drawTeethMouth(MX + shakeX, MY, mouthW, mouthH);
    // Floating "HA" text rising from mouth
    gfx->setTextSize(1);
    gfx->setTextColor(userAccentColor);
    float r1 = fmod(t * 2.0f, 1.0f);
    float r2 = fmod(t * 2.0f + 0.5f, 1.0f);
    if (r1 < 0.85f) {
      int ty = MY - 10 - (int)(r1 * 80.0f);
      int tx = MX - 20 + (int)(sinf(r1 * 3.14159f) * 10.0f);
      gfx->setCursor(tx, ty);
      gfx->print("HA");
    }
    if (r2 < 0.85f) {
      int ty = MY - 10 - (int)(r2 * 70.0f);
      int tx = MX + 6 - (int)(sinf(r2 * 3.14159f) * 8.0f);
      gfx->setCursor(tx, ty);
      gfx->print("HA");
    }
  } else if (currentExpression == "star_eyes") {
    float twinkle = sin(t * 3.14159f * 4.0f) * 0.5f + 0.5f;
    int starR = 11 + (int)(twinkle * 6.0f);
    drawEye(LX, EY, EW, EH, ER, 0, 0);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    // Large pulsing stars over eyes
    drawIconStar(LX, EY, starR);
    drawIconStar(RX, EY, starR);
    // Big smile
    drawSmile(MX, MY - 4, 48);
    // Floating sparkle particles
    float sp1 = fmod(t * 2.5f, 1.0f);
    float sp2 = fmod(t * 2.5f + 0.4f, 1.0f);
    float sp3 = fmod(t * 2.0f + 0.7f, 1.0f);
    if (sp1 < 0.8f) drawIconStar(companionXShift + 28 + (int)(sp1 * 20.0f), EY - 20 - (int)(sp1 * 40.0f), 3);
    if (sp2 < 0.8f) drawIconStar(216 - (int)(sp2 * 16.0f), EY - 16 - (int)(sp2 * 36.0f), 3);
    if (sp3 < 0.7f) drawIconStar(MX + (int)(sinf(sp3 * 6.28f) * 30.0f), EY - 44 - (int)(sp3 * 20.0f), 2);
  } else if (currentExpression == "excited") {
    float bounce = sin(t * 3.14159f * 4.0f) * 0.5f + 0.5f;
    int eyeShift = (int)(bounce * 9.0f);
    drawEye(LX, EY - eyeShift, EW + 9, EH + 9, ER, 0, 0);
    drawEye(RX, EY - eyeShift, EW + 9, EH + 9, ER, 0, 0);
    drawBrow(LX - 26, EY - 40 - eyeShift, LX + 26, EY - 40 - eyeShift);
    drawBrow(RX - 26, EY - 40 - eyeShift, RX + 26, EY - 40 - eyeShift);
    // Big wide grin
    drawSmile(MX, MY - 4 - (int)(bounce * 5.0f), 56);
    // Sparkle stars bouncing around
    float sp1 = fmod(t * 3.0f, 1.0f);
    float sp2 = fmod(t * 3.0f + 0.33f, 1.0f);
    float sp3 = fmod(t * 3.0f + 0.66f, 1.0f);
    int sr1 = 4 + (int)((1.0f - sp1) * 5.0f);
    int sr2 = 3 + (int)((1.0f - sp2) * 4.0f);
    int sr3 = 3 + (int)((1.0f - sp3) * 5.0f);
    drawIconStar(companionXShift + 22 + (int)(sp1 * 16.0f), EY - 20 - (int)(sp1 * 40.0f), sr1);
    drawIconStar(companionXShift + 220 - (int)(sp2 * 14.0f), EY - 16 - (int)(sp2 * 44.0f), sr2);
    drawIconStar(MX + (int)(sinf(sp3 * 3.14159f) * 40.0f), EY - 50 - (int)(sp3 * 20.0f), sr3);
    // Floating "!" marks
    gfx->setTextSize(2);
    gfx->setTextColor(userAccentColor);
    if (sp1 < 0.7f) {
      gfx->setCursor(LX - 32, EY - 48 - (int)(sp1 * 20.0f));
      gfx->print("!");
    }
  } else if (currentExpression == "tongue") {
    // Playful winking eye
    drawMouthCurve(LX, EY, EW * 0.82f, -7.0f, 3.6f, userEyeColor);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    // Open playful grin (V shape)
    fxThickLine(MX - 18, MY - 2, MX, MY + 6, 3.4f, userMouthColor);
    fxThickLine(MX, MY + 6, MX + 18, MY - 2, 3.4f, userMouthColor);
    // Animated tongue that wobbles side to side
    float wobble = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    float sway = sin(t * 3.14159f * 3.0f) * 5.0f;
    int tongueH = 16 + (int)(wobble * 6.0f);
    int tongueX = MX + (int)sway;
    gfx->fillRoundRect(tongueX - 12, MY + 5, 24, tongueH, 8, userMouthColor);
    gfx->fillCircle(tongueX, MY + 5 + tongueH - 6, 5, COL_BG);

  } else if (currentExpression == "grateful") {
    // Soft closed eyes (arcs) + warm smile + glow
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    drawHappyArc(LX, EY, EW);
    drawHappyArc(RX, EY, EW);
    // Wide warm smile
    drawSmile(MX, MY - 4, 52);
    // Gentle sparkles floating upward
    int sp = (int)(wave * 5.0f);
    drawIconStar(companionXShift + 26, FACE_OFFSET_Y + companionYShift + 30 + sp, 4);
    drawIconStar(companionXShift + 214, FACE_OFFSET_Y + companionYShift + 30 - sp, 3);
    // Gentle light particles rising
    float p1 = fmod(t * 1.5f, 1.0f);
    float p2 = fmod(t * 1.5f + 0.5f, 1.0f);
    gfx->fillCircle(companionXShift + 50 + (int)(p1 * 12.0f), FACE_OFFSET_Y + companionYShift + 80 - (int)(p1 * 60.0f), 2, userAccentColor);
    gfx->fillCircle(companionXShift + 200 - (int)(p2 * 10.0f), FACE_OFFSET_Y + companionYShift + 85 - (int)(p2 * 55.0f), 2, userAccentColor);

  } else if (currentExpression == "crying") {
    // Squished sad eyes, heavy tears from both sides
    drawEye(LX, EY + 6, EW, EH - 9, ER, 0, 7);
    drawEye(RX, EY + 6, EW, EH - 9, ER, 0, 7);
    drawBrow(LX - 26, EY - 11, LX + 18, EY - 30);
    drawBrow(RX + 26, EY - 11, RX - 18, EY - 30);
    // Open wailing mouth
    float wobble = sin(t * 3.14159f * 6.0f) * 2.0f;
    int mouthW = 30 + (int)(fabs(wobble) * 4.0f);
    int mouthH = 18 + (int)(fabs(wobble) * 3.0f);
    drawOvalMouth(MX + (int)wobble, MY - 2, mouthW / 2, mouthH / 2);
    // Heavy left tear streams (4 tears)
    for (int j = 0; j < 4; j++) {
      float tOff = fmod(t + j * 0.25f, 1.0f);
      int ty = EY + 26 + (int)(tOff * 65.0f);
      if (ty < MY + 35) drawTear(LX + EW / 2 + 3 - j * 3, ty, 5 - j);
    }
    // Heavy right tear streams (4 tears)
    for (int j = 0; j < 4; j++) {
      float tOff = fmod(t + j * 0.25f + 0.12f, 1.0f);
      int ty = EY + 26 + (int)(tOff * 65.0f);
      if (ty < MY + 35) drawTear(RX + EW / 2 + 3 + j * 3, ty, 5 - j);
    }

  } else if (currentExpression == "blushing") {
    // Averted gaze (pupils shifted), rosy cheeks
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int shift = 4 + (int)(wave * 4.0f);
    drawBlush(LX - 16, EY + 20, 16);
    drawBlush(RX + 16, EY + 20, 16);
    drawEye(LX, EY, EW, EH - 6, ER, -shift, 3);
    drawEye(RX, EY, EW, EH - 6, ER, -shift, 3);
    // Shy wobbly smile
    float smileShy = sin(t * 3.14159f * 3.0f) * 2.0f;
    for (int line = 0; line < 3; line++) {
      gfx->drawLine(MX - 12, MY + (int)smileShy + line, MX + 12, MY - (int)smileShy + line, userMouthColor);
    }
    // Tiny sparkle of embarrassment
    if (wave > 0.7f) {
      gfx->fillCircle(LX - 20, EY - 24, 2, userAccentColor);
    }

  } else if (currentExpression == "nervous") {
    // Wide darting eyes, wobbly mouth
    float sx = sin(t * 3.14159f * 6.0f);
    int px = (int)(sx * 7.0f);
    drawEye(LX, EY, EW + 4, EH + 4, ER, px, 0);
    drawEye(RX, EY, EW + 4, EH + 4, ER, px, 0);
    // Raised uneven brows
    float wave = sin(t * 3.14159f * 4.0f) * 3.0f;
    drawBrow(LX - 26, EY - 32 - (int)wave, LX + 26, EY - 30);
    drawBrow(RX - 26, EY - 30, RX + 26, EY - 32 + (int)wave);
    // Wobbly crooked small mouth
    int mShift = (int)(sin(t * 3.14159f * 8.0f) * 3.0f);
    drawMouthCurve(MX, MY + mShift, 30, -2.0f - mShift, 3.0f, userMouthColor);
    // Sweat drops (multiple, sliding down)
    float sweatT = fmod(t * 2.0f, 1.0f);
    int sweatY = EY - 18 + (int)(sweatT * 30.0f);
    drawTear(RX + EW / 2 + 10, sweatY, 5);
    if (sweatT > 0.3f) {
      drawTear(LX - EW / 2 - 8, sweatY - 10, 4);
    }
    // Slight body tremble (subtle eye jitter)
    int jitter = (ph % 4 < 2) ? 1 : -1;
    gfx->drawPixel(LX + jitter, EY - EH/2, COL_BG);
    gfx->drawPixel(RX + jitter, EY - EH/2, COL_BG);

  } else if (currentExpression == "proud") {
    // Closed eyes lifted, confident grin
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    // Confident arc eyes (slightly lifted, wider)
    drawHappyArc(LX, EY - 6, EW + 6);
    drawHappyArc(RX, EY - 6, EW + 6);
    // Proud wide grin with teeth showing
    drawTeethMouth(MX, MY - 4, 48, 20);
    // Crown sparkles above head (3 stars)
    int sp = (int)(wave * 5.0f);
    int starPulse = 5 + (int)(wave * 3.0f);
    drawIconStar(CX - 24, FACE_OFFSET_Y + companionYShift + 14 - sp, starPulse);
    drawIconStar(CX, FACE_OFFSET_Y + companionYShift + 8 - sp, starPulse + 2);
    drawIconStar(CX + 24, FACE_OFFSET_Y + companionYShift + 14 - sp, starPulse);

  } else if (currentExpression == "skeptical") {
    // One raised brow, squinting side-eye
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int browLift = 6 + (int)(wave * 4.0f);
    // Left eye squints, right eye wide
    drawEye(LX, EY + 4, EW - 8, EH - 12, ER - 2, 4, 4);
    drawEye(RX, EY, EW + 4, EH + 4, ER + 2, 4, 0);
    // Left brow flat, right brow raised
    drawBrow(LX - 22, EY - 18, LX + 22, EY - 20);
    drawBrow(RX - 22, EY - 32 - browLift, RX + 22, EY - 28 - browLift);
    // Flat angled mouth
    fxThickLine(MX - 16, MY + 4, MX + 16, MY - 2, 3.2f, userMouthColor);

  } else if (currentExpression == "peaceful") {
    // Gently closed eyes, serene breathing
    float breathe = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int lift = (int)(breathe * 3.0f);
    // Soft closed eye arcs
    drawHappyArc(LX, EY + 2 - lift, EW - 4);
    drawHappyArc(RX, EY + 2 - lift, EW - 4);
    // Gentle serene smile
    drawMouthCurve(MX, MY - lift, 26, 6.0f, 3.0f, userMouthColor);
    // Tiny sparkles that slowly drift
    int sp1 = (int)(t * 40.0f) % 20;
    int sp2 = (int)(t * 30.0f + 10) % 20;
    gfx->fillCircle(companionXShift + 40, FACE_OFFSET_Y + companionYShift + 40 + sp1, 2, userAccentColor);
    gfx->fillCircle(companionXShift + 210, FACE_OFFSET_Y + companionYShift + 50 + sp2, 2, userAccentColor);
    gfx->fillCircle(companionXShift + 260, FACE_OFFSET_Y + companionYShift + 30 + sp1, 1, userAccentColor);

  } else if (currentExpression == "determined") {
    // Focused eyes, firm set mouth
    float wave = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
    int focus = (int)(wave * 3.0f);
    // Slightly narrowed focused eyes
    drawEye(LX, EY + 2, EW, EH - 6, ER, 0, focus);
    drawEye(RX, EY + 2, EW, EH - 6, ER, 0, focus);
    // Angled determined brows
    drawBrow(LX - 22, EY - 20, LX + 22, EY - 28);
    drawBrow(RX - 22, EY - 28, RX + 22, EY - 20);
    // Firm determined mouth
    fxThickLine(MX - 20, MY + 1, MX + 20, MY + 1, 4.0f, userMouthColor);

  } else if (currentExpression == "cool") {
    // Sunglasses + confident smirk + sparkle glint.
    drawEye(LX, EY, EW, EH, ER, 0, 0);
    drawEye(RX, EY, EW, EH, ER, 0, 0);
    uint16_t shade = fxRGB(18, 18, 26);
    fxEllipseAA(LX, EY, EW * 0.62f, EH * 0.52f, shade);
    fxEllipseAA(RX, EY, EW * 0.62f, EH * 0.52f, shade);
    fxThickLine(LX + EW * 0.5f, EY - 2, RX - EW * 0.5f, EY - 2, 3.0f, shade);  // bridge
    fxThickLine(LX - EW * 0.6f, EY - 2, LX - EW * 0.6f - 10, EY - 6, 2.4f, shade); // arm
    // lens glints
    fxThickLine(LX - 8, EY - 6, LX - 2, EY + 2, 2.0f, fxRGB(150, 180, 210), 180);
    fxThickLine(RX - 8, EY - 6, RX - 2, EY + 2, 2.0f, fxRGB(150, 180, 210), 180);
    drawMouthCurve(MX - 4, MY, 34, 8.0f, 3.6f, userMouthColor);  // smirk (off-center)
    float tw = fxPulse(t);
    if (tw > 0.6f) fxStar(RX + EW * 0.6f + 6, EY - 12, 4 + (int)(tw * 3), COL_GOLD, true);

  } else if (currentExpression == "mind_blown") {
    // Huge eyes + open mouth + bursting particles from the top of the head.
    float wave = fxPulse(t);
    int eh = EH + 10 + (int)(wave * 8);
    drawEye(LX, EY + 4, EW + 8, eh, ER + 4, 0, 3);
    drawEye(RX, EY + 4, EW + 8, eh, ER + 4, 0, 3);
    drawBrow(LX - 26, EY - 34, LX + 26, EY - 34);
    drawBrow(RX - 26, EY - 34, RX + 26, EY - 34);
    drawOvalMouth(MX, MY + 2, 14, 12);
    // explosion of particles from the crown
    for (int i = 0; i < 10; i++) {
      float a = -3.14159f + i * 3.14159f / 9.0f;
      float r = 18.0f + wave * 26.0f + (i % 3) * 6.0f;
      int bx = CX + (int)(cosf(a) * r);
      int by = FACE_OFFSET_Y + companionYShift + 8 + (int)(sinf(a) * r * 0.5f);
      fxStar(bx, by, 2 + (i % 3), (i & 1) ? COL_GOLD : userAccentColor, false);
    }

  } else if (currentExpression == "dizzy") {
    // Spiral eyes + wobbly mouth + circling stars.
    float spin = t * 6.2832f;
    for (int r = 3; r <= 11; r += 3) {
      float a = spin + r;
      fxDiscAA(LX + cosf(a) * (r * 0.4f), EY + sinf(a) * (r * 0.4f), 1.6f, userAccentColor);
      fxDiscAA(RX + cosf(a + 1.0f) * (r * 0.4f), EY + sinf(a + 1.0f) * (r * 0.4f), 1.6f, userAccentColor);
    }
    fxRingAA(LX, EY, 12, 2.0f, fxDarken(userEyeColor, 20));
    fxRingAA(RX, EY, 12, 2.0f, fxDarken(userEyeColor, 20));
    float wob = sinf(t * 3.14159f * 6.0f) * 4.0f;
    drawMouthCurve(MX, MY + (int)wob, 28, -3.0f, 3.0f, userMouthColor);
    for (int i = 0; i < 3; i++) {
      float a = spin * 0.7f + i * 2.094f;
      fxStar(CX + cosf(a) * 70, FACE_OFFSET_Y + companionYShift + 16 + sinf(a) * 10, 3, COL_GOLD, false);
    }

  } else if (currentExpression == "mischievous") {
    // Half-lidded side-eye + one-sided grin + tiny sparkle.
    float wave = fxPulse(t);
    int look = 5 + (int)(wave * 3);
    drawEye(LX, EY + 4, EW, EH - 14, ER, look, 2);
    drawEye(RX, EY + 4, EW, EH - 14, ER, look, 2);
    gfx->fillRect(LX - EW / 2, EY + 4 - (EH - 14) / 2 - 2, EW, 5, COL_BG);
    gfx->fillRect(RX - EW / 2, EY + 4 - (EH - 14) / 2 - 2, EW, 5, COL_BG);
    drawBrow(LX - 22, EY - 20, LX + 18, EY - 12);
    drawBrow(RX - 18, EY - 12, RX + 22, EY - 20);
    // sly one-sided grin
    fxThickLine(MX - 18, MY + 4, MX + 6, MY + 4, 3.4f, userMouthColor);
    fxThickLine(MX + 6, MY + 4, MX + 18, MY - 4, 3.4f, userMouthColor);

  } else if (currentExpression == "shy") {
    // Looking down/away, big blush, tiny bashful smile, fidget sparkle.
    float wave = fxPulse(t);
    drawBlush(LX - 12, EY + 18, 17);
    drawBlush(RX + 12, EY + 18, 17);
    int look = -3 - (int)(wave * 3);
    drawEye(LX, EY + 2, EW, EH - 10, ER, look, 6);
    drawEye(RX, EY + 2, EW, EH - 10, ER, look, 6);
    drawMouthCurve(MX + look, MY, 18, 4.0f, 2.6f, userMouthColor);
    if (wave > 0.7f) fxDiscAA(LX - 22, EY - 18, 2, userAccentColor);

  } else if (currentExpression == "yawn") {
    // Big slow yawn: squinted eyes, mouth opens wide, a sleepy Zzz at the peak.
    float wave = fxPulse(t);
    int mh = 10 + (int)(wave * 16);
    drawEye(LX, EY, EW, 6, ER, 0, 0);
    drawEye(RX, EY, EW, 6, ER, 0, 0);
    drawBrow(LX - 24, EY - 22, LX + 22, EY - 18);
    drawBrow(RX - 22, EY - 18, RX + 24, EY - 22);
    drawOvalMouth(MX, MY + 4, 13, mh);
    if (wave > 0.5f) drawZzz(companionXShift + 190, companionYShift + 20 + eyeYShift, expressionPhase);

  } else if (currentExpression == "sneeze") {
    // Build-up then "achoo!" — scrunch up for most of the loop, burst at the end.
    bool burst = ph >= 48;
    if (!burst) {
      int sniff = (int)(fxPulse(t) * 3.0f);
      drawEye(LX, EY, EW, 8, ER, 0, -3 - sniff);
      drawEye(RX, EY, EW, 8, ER, 0, -3 - sniff);
      drawBrow(LX - 24, EY - 30 - sniff, LX + 22, EY - 24);
      drawBrow(RX - 22, EY - 24, RX + 24, EY - 30 - sniff);
      drawOvalMouth(MX, MY - 2, 9, 9);
    } else {
      drawHappyArc(LX, EY, EW);
      drawHappyArc(RX, EY, EW);
      drawOvalMouth(MX, MY + 6, 16, 14);
      float bt = (float)(ph - 48) / 15.0f;
      for (int i = 0; i < 9; i++) {
        float a = 1.0f + i * 0.13f;
        float r = bt * 64.0f + i * 3.0f;
        fxDiscAA(MX + cosf(a) * r, MY + 12 + sinf(a) * r * 0.6f, 2.4f - bt * 1.4f,
                 fxLighten(COL_SKYBLUE, 50), (uint8_t)(210 * (1.0f - bt)));
      }
    }

  } else if (currentExpression == "eye_roll") {
    // Pupils sweep a full circle (biased up) with an unimpressed flat mouth.
    float a = t * 6.2832f;
    int pdx = (int)(cosf(a) * 7.0f);
    int pdy = (int)(sinf(a) * 6.0f) - 4;
    drawEye(LX, EY, EW, EH, ER, pdx, pdy);
    drawEye(RX, EY, EW, EH, ER, pdx, pdy);
    drawBrow(LX - 24, EY - 26, LX + 22, EY - 24);
    drawBrow(RX - 22, EY - 24, RX + 24, EY - 26);
    drawMouthCurve(MX, MY, 30, -2.0f, 3.2f, userMouthColor);

  } else if (currentExpression == "smug") {
    // Half-lidded satisfied eyes, one raised brow, a slow self-satisfied smirk.
    drawEye(LX, EY + 4, EW, EH - 16, ER, 3, 3);
    drawEye(RX, EY + 4, EW, EH - 16, ER, 3, 3);
    gfx->fillRect(LX - EW / 2, EY + 4 - (EH - 16) / 2 - 2, EW, 4, COL_BG);
    gfx->fillRect(RX - EW / 2, EY + 4 - (EH - 16) / 2 - 2, EW, 4, COL_BG);
    drawBrow(LX - 22, EY - 16, LX + 22, EY - 16);
    drawBrow(RX - 24, EY - 18, RX + 22, EY - 28);
    drawMouthCurve(MX - 4, MY, 30, 6.0f, 3.4f, userMouthColor);
    if (fxPulse(t) > 0.7f) fxStar(MX + 24, MY - 6, 3, COL_GOLD, true);

  } else if (currentExpression == "party") {
    // Beaming grin under a steady rain of multi-color confetti.
    drawHappyArc(LX, EY, EW);
    drawHappyArc(RX, EY, EW);
    drawSmile(MX, MY - 4, 48);
    for (int i = 0; i < 14; i++) {
      float p2 = fmodf(t + i * 0.0719f, 1.0f);
      int cxp = (i * 47 + 13) % SCREEN_WIDTH;
      int cyp = (int)(p2 * SCREEN_HEIGHT);
      uint16_t cc = (i % 4 == 0) ? COL_GOLD : (i % 4 == 1) ? COL_ROSE : (i % 4 == 2) ? COL_SKYBLUE : COL_MINT;
      gfx->fillRect(cxp, cyp, 4, 4, cc);
    }

  } else if (currentExpression == "cold") {
    // Shivering: teeth chatter, blue cheeks, a frosty breath puff, drifting snow.
    gFxOffX += sinf(t * 3.14159f * 16.0f) * 2.5f;
    drawEye(LX, EY, EW, EH - 6, ER, 0, 2);
    drawEye(RX, EY, EW, EH - 6, ER, 0, 2);
    drawBrow(LX - 24, EY - 24, LX + 22, EY - 20);
    drawBrow(RX - 22, EY - 20, RX + 24, EY - 24);
    drawTeethMouth(MX, MY, 30, 14);
    drawBlush(LX - 12, EY + 18, 13);
    drawBlush(RX + 12, EY + 18, 13);
    float bp = fmodf(t * 1.5f, 1.0f);
    if (bp < 0.6f) fxDiscAA(MX + 18 + bp * 20.0f, MY - bp * 14.0f, 4.0f + bp * 4.0f,
                            fxLighten(COL_SKYBLUE, 60), (uint8_t)(160 * (1.0f - bp)));
    for (int i = 0; i < 10; i++) {
      int sx = (i * 53 + 7) % SCREEN_WIDTH;
      int sy = (int)(fmodf(t + i * 0.1f, 1.0f) * SCREEN_HEIGHT);
      fxDiscAA(sx, sy, 1.6f, COL_FG, 200);
    }

  } else if (currentExpression == "heartbroken") {
    // Teary downcast eyes under a small heart splitting down a jagged crack.
    drawEye(LX, EY + 6, EW, EH - 8, ER, 0, 6);
    drawEye(RX, EY + 6, EW, EH - 8, ER, 0, 6);
    drawBrow(LX - 26, EY - 8, LX + 18, EY - 26);
    drawBrow(RX + 26, EY - 8, RX - 18, EY - 26);
    drawSadArc(MX, MY - 2, 30);
    int hy = FACE_OFFSET_Y + companionYShift + 22;
    drawBigHeart(CX, hy, 7);
    fxThickLine(CX, hy - 8, CX - 3, hy, 2.2f, COL_BG);
    fxThickLine(CX - 3, hy, CX + 3, hy + 5, 2.2f, COL_BG);
    fxThickLine(CX + 3, hy + 5, CX, hy + 13, 2.2f, COL_BG);
    if (ph >= 14) { float tt = (float)(ph - 14) / 17.0f; drawTear(LX + EW / 2 + 4, EY + 24 + (int)(tt * 40.0f), 4); }

  } else if (currentExpression == "singing") {
    // Closed happy eyes, an open round mouth, eighth-notes floating up and away.
    float wave = fxPulse(t);
    drawHappyArc(LX, EY, EW);
    drawHappyArc(RX, EY, EW);
    drawOvalMouth(MX, MY + 2, 11, 10 + (int)(wave * 8.0f));
    for (int i = 0; i < 3; i++) {
      float r = fmodf(t * 1.3f + i * 0.33f, 1.0f);
      int nx = MX + 30 + i * 8 + (int)(sinf(r * 6.28f) * 8.0f);
      int ny = MY - (int)(r * 72.0f);
      uint16_t nc = (i & 1) ? COL_GOLD : userAccentColor;
      fxDiscAA(nx, ny, 3.5f, nc, (uint8_t)(255 * (1.0f - r * 0.4f)));
      fxThickLine(nx + 3, ny, nx + 3, ny - 12, 1.8f, nc, (uint8_t)(255 * (1.0f - r * 0.4f)));
    }

  } else if (currentExpression == "zen") {
    // Serene meditation: gently closed eyes, faint smile, a breathing aura ring.
    float br = fxPulse(t * 0.5f);
    fxRingAA(CX, faceCenterY, 70.0f + br * 14.0f, 2.0f, fxLighten(COL_LAVENDER, 30), (uint8_t)(70 + br * 70));
    drawHappyArc(LX, EY, EW);
    drawHappyArc(RX, EY, EW);
    drawMouthCurve(MX, MY, 22, 3.0f, 2.6f, userMouthColor);
    fxDiscAA(CX, FACE_OFFSET_Y + companionYShift + 14, 3, COL_GOLD);

  } else if (currentExpression == "suspicious") {
    // Narrowed eyes scanning side to side, flat wary mouth.
    int pdx = (int)(sinf(t * 3.14159f * 2.0f) * 8.0f);
    drawEye(LX, EY + 4, EW, EH - 18, ER, pdx, 1);
    drawEye(RX, EY + 4, EW, EH - 18, ER, pdx, 1);
    gfx->fillRect(LX - EW / 2, EY + 4 - (EH - 18) / 2 - 2, EW, 5, COL_BG);
    gfx->fillRect(RX - EW / 2, EY + 4 - (EH - 18) / 2 - 2, EW, 5, COL_BG);
    drawBrow(LX - 24, EY - 16, LX + 22, EY - 20);
    drawBrow(RX - 22, EY - 20, RX + 24, EY - 16);
    drawMouthCurve(MX, MY, 22, -1.0f, 3.2f, userMouthColor);

  } else {
    float drift = sin(t * 3.14159f * 2.0f);
    int px = (int)(drift * 4.0f);
    int py = (int)(cos(t * 3.14159f * 3.0f) * 2.0f);
    int eyeH = EH;
    if (ph >= 28 && ph <= 30) {
      float blinkT = ph == 29 ? 1.0f : 0.5f;
      eyeH = EH - (int)(blinkT * (EH - 5));
    }
    drawEye(LX, EY, EW, eyeH, ER, px, py);
    drawEye(RX, EY, EW, eyeH, ER, px, py);
    drawMouthCurve(MX, MY, 30, 4.0f, 3.4f, userMouthColor);
  }

  // Smart accessory placement: anchor hats/glasses/mustache to where THIS expression
  // actually puts the eyes/mouth, and lift the hat extra for tall / raised-brow /
  // crown-burst faces so it never clips the brows or the effect above the head.
  int accEyeDy = 0, accMouthDy = 0, headwearLift = 0;
  if (currentExpression == "sad" || currentExpression == "crying" || currentExpression == "heartbroken") accEyeDy = 6;
  else if (currentExpression == "sleepy" || currentExpression == "yawn" || currentExpression == "skeptical") accEyeDy = 4;
  else if (currentExpression == "angry") accEyeDy = 4;
  else if (currentExpression == "determined" || currentExpression == "shy" || currentExpression == "cold") accEyeDy = 2;
  else if (currentExpression == "excited" || currentExpression == "proud") accEyeDy = -5;

  // Expressions whose brows rise, eyes balloon, or that erupt above the crown need
  // the hat lifted clear; ones with low/level brows can sit normally.
  if (currentExpression == "mind_blown") headwearLift = 18;       // crown explosion
  else if (currentExpression == "surprised") headwearLift = 12;   // brows + wide eyes shoot up
  else if (currentExpression == "starstruck" || currentExpression == "party") headwearLift = 8;
  else if (currentExpression == "excited" || currentExpression == "proud") headwearLift = 6;
  else if (currentExpression == "sneeze") headwearLift = 6;       // brows jump on the build-up

  drawCompanionAccessories(LX, RX, EY + accEyeDy, MY + accMouthDy, headwearLift);
  gFxOffX = gFxOffY = 0.0f;   // clear float so it never leaks into other modes
  pushCanvas();
}

// ─── Weather background effects drawn behind the companion ───
void drawWeatherBackground() {
  if (!idleShowWeather || weatherCode < 0) return;
  const int cat = weatherCodeCategory(weatherCode);
  if (cat == 0) {
    // Clear sky: warm sun in bottom-left corner
    const int sx = 28, sy = SCREEN_HEIGHT - 40;
    const int r = 18;
    const uint16_t sunCol = 0xFEE0;
    gfx->fillCircle(sx, sy, r, sunCol);
    for (int i = 0; i < 8; i++) {
      float ang = i * 0.7854f;
      int x1 = sx + (int)(cosf(ang) * (r + 3));
      int y1 = sy + (int)(sinf(ang) * (r + 3));
      int x2 = sx + (int)(cosf(ang) * (r + 11));
      int y2 = sy + (int)(sinf(ang) * (r + 11));
      gfx->drawLine(x1, y1, x2, y2, sunCol);
      gfx->drawLine(x1, y1 + 1, x2, y2 + 1, sunCol);
    }
  } else if (cat == 2 || cat == 4) {
    const bool storm = (cat == 4);
    // Storm clouds banked across the top.
    if (storm) {
      const uint16_t cloud = fxRGB(58, 62, 76), cloudHi = fxRGB(92, 98, 116);
      for (int cxp = 10; cxp < SCREEN_WIDTH; cxp += 72) {
        fxDiscAA(cxp, 16, 16, cloud);
        fxDiscAA(cxp + 24, 20, 21, cloud);
        fxDiscAA(cxp + 48, 16, 15, cloud);
        fxDiscAA(cxp + 12, 9, 9, cloudHi, 120);
      }
      gfx->fillRect(0, 0, SCREEN_WIDTH, 6, fxRGB(40, 44, 56));
    }
    // Rain — heavier and faster during storms.
    rainDropOffset = (rainDropOffset + (storm ? 5 : 3)) % SCREEN_HEIGHT;
    const uint16_t rainCol = storm ? 0x6C9F : 0x4A7F;
    const int drops = storm ? 56 : 36;
    const int len   = storm ? 11 : 8;
    for (int i = 0; i < drops; i++) {
      int x = (i * 41 + 7) % SCREEN_WIDTH;
      int y = (i * 53 + rainDropOffset) % SCREEN_HEIGHT;
      gfx->drawLine(x, y, x - 3, y + len, rainCol);
    }
    if (storm) {
      // Forked lightning + a quick flash, on a couple of orbit steps.
      const bool flash = (idleOrbit == 3 || idleOrbit == 4 || idleOrbit == 11);
      if (flash) {
        const uint16_t lt = 0xFFFF, ltDim = fxRGB(225, 235, 170);
        // Bright flash band fading down from the top.
        for (int yy = 0; yy < 44; yy++)
          fxThickLine(0, yy, SCREEN_WIDTH, yy, 1.0f, ltDim, (uint8_t)(70 - yy));
        // Jagged bolt (with a branch).
        float zx = (idleOrbit == 11) ? SCREEN_WIDTH - 66 : 58, zy = 6;
        static const float dxs[] = { -9, 11, -7, 10, -6, 8 };
        static const float dys[] = { 13, 15, 13, 12, 15, 14 };
        for (int s = 0; s < 6; s++) {
          float nx = zx + dxs[s], ny = zy + dys[s];
          fxThickLine(zx, zy, nx, ny, 3.0f, lt);
          if (s == 2) {
            fxThickLine(nx, ny, nx + 13, ny + 16, 2.0f, ltDim);
            fxThickLine(nx + 13, ny + 16, nx + 7, ny + 31, 2.0f, ltDim);
          }
          zx = nx; zy = ny;
        }
      }
    }
  }
}

// Local hour (0-23) from the synced clock, or -1 if time isn't known yet.
int currentLocalHour() {
  struct tm ti;
  if (getLocalTime(&ti, 5)) return ti.tm_hour;
  return -1;
}

// Drives the autonomous idle-behavior state machine. Picks a new little behavior
// after a randomized quiet gap, biased by the time of day (sleepier at night).
void updateIdleBehavior() {
  const unsigned long now = millis();
  if (gIdleBehavior != 0) {
    if (now >= gIdleBehaviorEndMs) {
      gIdleBehavior = 0;
      gNextIdleBehaviorMs = now + 6000UL + (unsigned long)random(12000);
    }
    return;
  }
  if (gNextIdleBehaviorMs == 0) gNextIdleBehaviorMs = now + 5000UL + (unsigned long)random(8000);
  if (now < gNextIdleBehaviorMs) return;
  const int hour = currentLocalHour();
  const bool night = (hour >= 22 || (hour >= 0 && hour < 6));
  uint8_t pick;
  if (night) {
    const uint8_t r = (uint8_t)random(10);
    pick = (r < 5) ? 5 : (r < 8) ? 1 : 2;          // doze, yawn, look around
  } else {
    const uint8_t r = (uint8_t)random(10);
    pick = (r < 3) ? 2 : (r < 5) ? 3 : (r < 7) ? 4 : (r < 9) ? 1 : 5;
  }
  gIdleBehavior = pick;
  gIdleBehaviorStartMs = now;
  const unsigned long dur = (pick == 5) ? 4200UL : (pick == 1) ? 2200UL
                          : (pick == 2) ? 2600UL : (pick == 3) ? 1600UL : 1100UL;
  gIdleBehaviorEndMs = now + dur;
}

// A rotating affirmation/quote, advanced every hour (see render). Big pool so it
// rarely repeats. Keep each line short-ish; rendering wraps to fit any font size.
static const char* AFFIRMATIONS[] = {
  "you've got this today", "you are loved", "breathe, you're doing great",
  "small steps still count", "be kind to yourself", "today is yours",
  "you make things better", "progress over perfection", "you are enough",
  "good things are coming", "trust yourself", "you bring the sunshine",
  "rest is productive too", "your best is plenty", "stay soft, stay strong",
  "one thing at a time", "you matter", "make today gentle",
  "keep going, you're close", "someone's proud of you", "you are capable of amazing things",
  "your feelings are valid", "it's okay to slow down", "you deserve good things",
  "you are stronger than you think", "today, choose joy", "you light up the room",
  "be proud of how far you've come", "you can do hard things", "your presence is a gift",
  "take it one breath at a time", "you are exactly where you need to be", "kindness looks good on you",
  "you're allowed to take up space", "your story matters", "you handle things so well",
  "let today be easy", "you are worthy of love", "your effort is enough",
  "good morning, sunshine", "you make people feel seen", "trust the timing of your life",
  "you are a work of art", "be gentle with your heart", "you've survived every hard day so far",
  "you are someone's reason to smile", "your dreams are valid", "keep your head up",
  "you are doing better than you think", "celebrate the small wins", "you are not behind",
  "your kindness ripples out", "you bring calm to chaos", "you are deeply appreciated",
  "let yourself rest tonight", "you are a good friend", "the world is better with you in it",
  "your warmth is contagious", "you are allowed to begin again", "soft hearts are strong hearts",
  "you are growing every day", "you handled that with grace", "your patience is a superpower",
  "you are more than enough", "be the calm in your own storm", "you are safe, you are okay",
  "you are someone's favorite person", "your laugh is the best sound", "you make ordinary days special",
  "you're doing your best and it shows", "lead with love today", "you are wonderfully made",
  "your courage inspires others", "give yourself some credit", "you are a bright spot",
  "trust that you are enough", "you carry so much light", "let go of what you can't control",
  "you are worth slowing down for", "your heart is in the right place", "today is a fresh start",
  "you are quietly brave", "you deserve a soft life", "your hard work matters",
  "you are loved more than you know", "keep being you", "you make the world warmer",
  "you are allowed to feel proud", "rest, you've earned it", "your gentleness is strength",
  "you are a gift to those around you", "good things take time, and so do you", "you shine without trying",
  "you are doing beautifully", "be proud of small progress", "you are wanted and needed",
  "your smile changes the room", "you are stronger than yesterday", "take heart, you're not alone",
  "you make hard days lighter", "you are full of possibility", "trust yourself, you know the way",
  "you are a calm in someone's storm", "your love makes a difference", "today, be soft with you",
  "you are healing and that's enough", "you are someone's safe place", "keep that beautiful heart open",
  "you are exactly enough today", "your best self is already here", "let today be kind to you",
  "you are worthy just as you are", "you bring people peace", "you are a deep breath of fresh air",
};
static const int AFFIRMATION_COUNT = sizeof(AFFIRMATIONS) / sizeof(AFFIRMATIONS[0]);

// Wrapped + centered text block, vertically centered on centerY (used for affirmations).
void drawCenteredWrapped(const String& text, int sizeChars, int centerY, int maxLines, uint16_t color) {
  if (sizeChars < 1) sizeChars = 1;
  const int charW = 6 * sizeChars, lh = 8 * sizeChars + 3;
  int maxChars = (SCREEN_WIDTH - 12) / charW;
  if (maxChars < 4) maxChars = 4;
  String lines[5]; int n = 0;
  String rem = text; rem.trim();
  while (rem.length() > 0 && n < maxLines && n < 5) {
    int len = (int)rem.length() > maxChars ? maxChars : (int)rem.length();
    if (len < (int)rem.length()) { int sp = rem.lastIndexOf(' ', len); if (sp > 0) len = sp; }
    String ln = rem.substring(0, len); ln.trim();
    if (ln.isEmpty()) { ln = rem.substring(0, maxChars); len = ln.length(); }
    lines[n++] = ln;
    rem = rem.substring(len); rem.trim();
  }
  gfx->setTextSize(sizeChars);
  gfx->setTextColor(color);
  gfx->setTextWrap(false);
  int y = centerY - (n * lh) / 2;
  for (int i = 0; i < n; i++) {
    int tw = (int)lines[i].length() * charW;
    gfx->setCursor((SCREEN_WIDTH - tw) / 2, y);
    gfx->print(lines[i]);
    y += lh;
  }
}

// Centerpiece analog clock (used when the companion face is hidden).
void drawAnalogClock(int cx, int cy, int r, int h24, int m) {
  fxDiscAA(cx, cy, r + 3, fxDarken(userAccentColor, 150), 60);   // soft base
  fxRingAA(cx, cy, r, 3.0f, userAccentColor);
  for (int i = 0; i < 12; i++) {
    float a = i * 0.5236f;                                       // 30° per hour
    float t = (i % 3 == 0) ? r - 9 : r - 5;                      // longer ticks at 12/3/6/9
    fxThickLine(cx + cosf(a) * t, cy + sinf(a) * t, cx + cosf(a) * (r - 2), cy + sinf(a) * (r - 2),
                (i % 3 == 0) ? 3.0f : 1.6f, userFaceColor);
  }
  float ma = (m / 60.0f) * 6.2832f - 1.5708f;
  float ha = ((h24 % 12) / 12.0f + m / 720.0f) * 6.2832f - 1.5708f;
  fxThickLine(cx, cy, cx + cosf(ha) * (r * 0.5f), cy + sinf(ha) * (r * 0.5f), 4.0f, userFaceColor);   // hour
  fxThickLine(cx, cy, cx + cosf(ma) * (r * 0.78f), cy + sinf(ma) * (r * 0.78f), 2.6f, userAccentColor); // minute
  fxDiscAA(cx, cy, 4, userAccentColor);
}

// Word clock phrase, e.g. "half past ten" / "quarter to nine".
String wordClockText(int h24, int m) {
  static const char* nums[] = { "twelve", "one", "two", "three", "four", "five", "six",
                                "seven", "eight", "nine", "ten", "eleven", "twelve" };
  static const char* mw[]   = { "", "five", "ten", "quarter", "twenty", "twenty-five", "half" };
  int h12 = h24 % 12; if (h12 == 0) h12 = 12;
  int nextH = (h12 % 12) + 1;
  int r = (m + 2) / 5;                       // nearest 5-minute slot, 0..12
  if (r == 0) return String(nums[h12]) + " o'clock";
  if (r == 12) return String(nums[nextH]) + " o'clock";
  if (r <= 6) return String(mw[r]) + " past " + nums[h12];
  return String(mw[12 - r]) + " to " + nums[nextH];
}

// Render the word-clock phrase as big stacked words, centered.
void drawWordClock(int cx, int cy, int h24, int m) {
  String phrase = wordClockText(h24, m);
  String words[4]; int nw = 0;
  int start = 0;
  while (nw < 4) {
    int sp = phrase.indexOf(' ', start);
    if (sp < 0) { words[nw++] = phrase.substring(start); break; }
    words[nw++] = phrase.substring(start, sp);
    start = sp + 1;
  }
  const int sz = 3, lh = 8 * sz + 8;
  int totalH = nw * lh;
  int y = cy - totalH / 2;
  gfx->setTextSize(sz);
  for (int i = 0; i < nw; i++) {
    gfx->setTextColor(i == nw - 1 ? userAccentColor : userFaceColor);
    int tw = words[i].length() * 6 * sz;
    gfx->setCursor(cx - tw / 2, y + i * lh);
    gfx->print(words[i]);
  }
}

// A tiny weather glyph (~14px) for the forecast strip, keyed by WMO code.
void drawMiniWeatherIcon(int cx, int cy, int code) {
  if (code < 0) return;
  const uint16_t sun = 0xFEE0, cloud = fxRGB(150, 156, 170), rain = COL_SKYBLUE, snow = COL_FG;
  if (code <= 1) {                                  // clear / mostly clear
    fxDiscAA(cx, cy, 5, sun);
    for (int i = 0; i < 8; i++) { float a = i * 0.7854f; gfx->drawLine(cx + cosf(a) * 7, cy + sinf(a) * 7, cx + cosf(a) * 9, cy + sinf(a) * 9, sun); }
  } else if (code <= 3 || code == 45 || code == 48) { // cloudy / fog
    fxDiscAA(cx - 4, cy + 1, 5, cloud); fxDiscAA(cx + 4, cy + 1, 5, cloud); fxDiscAA(cx, cy - 2, 6, cloud);
  } else if (code >= 71 && code <= 77) {            // snow
    fxDiscAA(cx, cy - 2, 6, cloud);
    fxDiscAA(cx - 4, cy + 6, 1.4f, snow); fxDiscAA(cx + 4, cy + 6, 1.4f, snow); fxDiscAA(cx, cy + 8, 1.4f, snow);
  } else if (code >= 95) {                          // thunderstorm
    fxDiscAA(cx, cy - 2, 6, cloud);
    fxThickLine(cx, cy + 2, cx - 3, cy + 7, 2.0f, COL_GOLD); fxThickLine(cx - 3, cy + 7, cx + 2, cy + 9, 2.0f, COL_GOLD);
  } else {                                          // rain / drizzle / showers
    fxDiscAA(cx, cy - 2, 6, cloud);
    gfx->drawLine(cx - 4, cy + 4, cx - 5, cy + 9, rain); gfx->drawLine(cx, cy + 4, cx - 1, cy + 9, rain); gfx->drawLine(cx + 4, cy + 4, cx + 3, cy + 9, rain);
  }
}

void renderIdle() {
  if (!displayAvailable) return;
  if (idleUseBackgroundImage && idleBackgroundBuffer) {
    gfx->drawRGBBitmap(0, 0, idleBackgroundBuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
  } else {
    // True black — clean premium base, no banding.
    gfx->fillScreen(COL_BG);
  }
  drawWeatherBackground();

  // ── Companion face (idle: gentle float + blink) ──
  if (idleShowFace) {
    // Slow, continuous gaze wander (periods ~14s / ~20s) so the eyes drift gently
    // instead of wiggling on the fast 0.8s idleOrbit cycle.
    gEyeDriftX = sinf((float)millis() * 0.00045f) * 2.5f;
    gEyeDriftY = sinf((float)millis() * 0.00031f) * 1.2f;
    const float cScale  = companionScale / 100.0f;
    const int   CY_shift = clampAppearanceOffset(companionOffsetY) * 2;
    const int   CX      = SCREEN_WIDTH / 2 + clampAppearanceOffset(companionOffsetX) * 2;
    const int   eyeXShift   = clampAppearanceOffset(companionEyeOffsetX) * 2;
    const int   eyeYShift   = clampAppearanceOffset(companionEyeOffsetY) * 2;
    const int   mouthXShift = clampAppearanceOffset(companionMouthOffsetX) * 2;
    const int   mouthYShift = clampAppearanceOffset(companionMouthOffsetY) * 2;

    // Smooth SUB-PIXEL breathing float (continuous millis clock → no stepping).
    const float iclk = (float)millis() * 0.0016f;
    gFxOffY = sinf(iclk) * 4.0f;
    gFxOffX = sinf(iclk + 1.1f) * 2.0f;

    const int eyeHalf = (int)(52 * cScale);   // eyes symmetric about the face center
    const int LX = CX - eyeHalf + eyeXShift;
    const int RX = CX + eyeHalf + eyeXShift;
    const int EY = FACE_OFFSET_Y + CY_shift + (int)(45 * cScale) + eyeYShift;
    const int MX = CX + mouthXShift;
    const int MY = FACE_OFFSET_Y + CY_shift + (int)(100 * cScale) + mouthYShift;
    const int EW = (int)(52 * cScale);
    const int EH = (int)(40 * cScale);
    const int ER = (int)(13 * cScale);

    // A natural slow blink: one quick ~150ms blink roughly every 4.2s — not on
    // every fast cycle (which read as constant twitching).
    bool blink = (millis() % 4200UL) < 150UL;
    int eyeH = blink ? max(3, EH / 5) : EH;
    int gaze = 0;   // base gaze; the slow wander above + behaviors move the eyes

    // ── Autonomous idle behavior overrides ──
    float behT = 0.0f;
    if (gIdleBehavior != 0 && gIdleBehaviorEndMs > gIdleBehaviorStartMs)
      behT = (float)(millis() - gIdleBehaviorStartMs) / (float)(gIdleBehaviorEndMs - gIdleBehaviorStartMs);
    if (gIdleBehavior == 2) gaze = (int)(sinf(behT * 6.2832f) * 7.0f);          // one gentle look-around sweep
    else if (gIdleBehavior == 3) { gaze = -7; gFxOffY -= 2.0f; }                // glance at clock
    else if (gIdleBehavior == 4 && sinf(behT * 6.2832f * 2.0f) > 0.6f) eyeH = max(3, EH / 5); // soft double-blink

    // ── Weather-reactive startle: eyes pop wide briefly (~every 9s) in a storm ──
    const int wxCat = (idleShowWeather && weatherCode >= 0) ? weatherCodeCategory(weatherCode) : -1;
    if (wxCat == 4 && (millis() % 9000UL) < 220UL) { eyeH = EH + 8; blink = false; }

    if (gIdleBehavior == 1) {
      // Yawn: squinted eyes, a mouth that opens wide then closes, a sleepy Zzz.
      float op = sinf(behT * 3.14159f);
      drawEye(LX, EY, EW, 6, ER, 0, 0);
      drawEye(RX, EY, EW, 6, ER, 0, 0);
      drawOvalMouth(MX, MY + 4, 12, 8 + (int)(op * 14.0f));
      drawZzz(RX + 18, EY - 24, idleOrbit * 4);
    } else if (gIdleBehavior == 5) {
      // Doze: head sinks, eyes droop nearly shut, Zzz drifts up.
      gFxOffY += 3.0f;
      drawEye(LX, EY + 6, EW, 4, ER, 0, 0);
      drawEye(RX, EY + 6, EW, 4, ER, 0, 0);
      drawMouthCurve(MX, MY, 20, 1.5f, 3.0f, userMouthColor);
      drawZzz(RX + 18, EY - 20, idleOrbit * 4);
    } else if (idleExpression == "love") {
      drawBlush(LX - 14, EY + 18, 15);
      drawBlush(RX + 14, EY + 18, 15);
      if (blink) { drawBlinkEye(LX, EY, EW, 7, ER); drawBlinkEye(RX, EY, EW, 7, ER); }
      else { drawHeartEye(LX, EY, 9); drawHeartEye(RX, EY, 9); }
      drawSmile(MX, MY - 2, 42);
    } else if (idleExpression == "happy") {
      drawEye(LX, EY, EW, eyeH, ER, gaze, 0);
      drawEye(RX, EY, EW, eyeH, ER, gaze, 0);
      drawSmile(MX, MY, 32);
    } else if (idleExpression == "sleepy") {
      drawEye(LX, EY + 4, EW, blink ? 4 : 8, ER, 0, 0);
      drawEye(RX, EY + 4, EW, blink ? 4 : 8, ER, 0, 0);
      drawMouthCurve(MX, MY, 22, 2.0f, 3.0f, userMouthColor);
      drawZzz(RX + 18, EY - 24, idleOrbit * 4);
    } else if (idleExpression == "wink") {
      drawMouthCurve(LX, EY, EW * 0.8f, -7.0f, 3.4f, userEyeColor);
      drawEye(RX, EY, EW, eyeH, ER, gaze, 0);
      drawSmile(MX, MY, 30);
    } else if (idleExpression == "cool") {
      drawEye(LX, EY, EW, eyeH, ER, 0, 0);
      drawEye(RX, EY, EW, eyeH, ER, 0, 0);
      uint16_t shade = fxRGB(18, 18, 26);
      fxEllipseAA(LX, EY, EW * 0.62f, EH * 0.52f, shade);
      fxEllipseAA(RX, EY, EW * 0.62f, EH * 0.52f, shade);
      fxThickLine(LX + EW * 0.5f, EY - 2, RX - EW * 0.5f, EY - 2, 3.0f, shade);
      drawMouthCurve(MX - 4, MY, 34, 8.0f, 3.6f, userMouthColor);
    } else if (idleExpression == "excited") {
      drawEye(LX, EY, EW + 6, EH + 6, ER, 0, 0);
      drawEye(RX, EY, EW + 6, EH + 6, ER, 0, 0);
      drawSmile(MX, MY - 2, 48);
    } else if (idleExpression == "peaceful" || idleExpression == "grateful") {
      drawHappyArc(LX, EY, EW);
      drawHappyArc(RX, EY, EW);
      drawSmile(MX, MY - 2, 36);
    } else if (idleExpression == "starstruck") {
      fxStar(LX, EY, EH * 0.5f, COL_GOLD, true);
      fxStar(RX, EY, EH * 0.5f, COL_GOLD, true);
      drawSmile(MX, MY - 2, 44);
    } else if (idleExpression == "surprised") {
      drawEye(LX, EY, EW + 6, EH + 12, ER + 3, gaze, 0);
      drawEye(RX, EY, EW + 6, EH + 12, ER + 3, gaze, 0);
      drawOvalMouth(MX, MY, 11, 11);
    } else if (idleExpression == "playful") {
      drawMouthCurve(LX, EY, EW * 0.8f, -7.0f, 3.4f, userEyeColor);   // wink
      drawEye(RX, EY, EW, eyeH, ER, gaze, 0);
      drawSmile(MX, MY, 30);
      fxDiscAA(MX, MY + 7, 5, COL_ROSE);                              // cheeky tongue tip
    } else if (idleExpression == "mischievous") {
      drawEye(LX, EY + 4, EW, EH - 14, ER, 4, 2);
      drawEye(RX, EY + 4, EW, EH - 14, ER, 4, 2);
      gfx->fillRect(LX - EW / 2, EY + 4 - (EH - 14) / 2 - 2, EW, 4, COL_BG);
      gfx->fillRect(RX - EW / 2, EY + 4 - (EH - 14) / 2 - 2, EW, 4, COL_BG);
      drawMouthCurve(MX - 4, MY, 30, 6.0f, 3.4f, userMouthColor);
    } else if (idleExpression == "shy") {
      drawBlush(LX - 12, EY + 18, 16);
      drawBlush(RX + 12, EY + 18, 16);
      drawEye(LX, EY + 2, EW, EH - 8, ER, -3, 4);
      drawEye(RX, EY + 2, EW, EH - 8, ER, -3, 4);
      drawMouthCurve(MX, MY, 18, 4.0f, 2.6f, userMouthColor);
    } else {
      // Neutral idle: gentle glance + blink + soft smile/neutral mouth.
      drawEye(LX, EY, EW, eyeH, ER, gaze, 0);
      drawEye(RX, EY, EW, eyeH, ER, gaze, 0);
      if (idleOrbit >= 10 && idleOrbit <= 13) drawSmile(MX, MY, 26);
      else drawMouthCurve(MX, MY, 28, 4.0f, 3.2f, userMouthColor);
    }

    int idleHatLift = 0;
    if (idleExpression == "surprised" || idleExpression == "starstruck") idleHatLift = 10;
    else if (idleExpression == "excited") idleHatLift = 6;
    drawCompanionAccessories(LX, RX, EY, MY, idleHatLift);
    gFxOffX = gFxOffY = 0.0f;   // clear before clock/status text
  }

  // ── Clock ── small top-left when the face is shown; a styled centerpiece
  // (big digital / analog / word clock) when the face is hidden.
  if (idleShowClock) {
    struct tm ti;
    if (getLocalTime(&ti, 10)) {
      if (!idleShowFace && idleClockFace == 1) {
        drawAnalogClock(SCREEN_WIDTH / 2, 118, 72, ti.tm_hour, ti.tm_min);
      } else if (!idleShowFace && idleClockFace == 2) {
        drawWordClock(SCREEN_WIDTH / 2, 118, ti.tm_hour, ti.tm_min);
      } else if (!idleShowFace) {
        char big[12];
        if (idleClock12Hour) {
          int hh = ti.tm_hour % 12; if (hh == 0) hh = 12;
          snprintf(big, sizeof(big), "%d:%02d", hh, ti.tm_min);
        } else {
          snprintf(big, sizeof(big), "%02d:%02d", ti.tm_hour, ti.tm_min);
        }
        gfx->setTextSize(6);
        gfx->setTextColor(userAccentColor);
        int tw = (int)strlen(big) * 6 * 6;
        gfx->setCursor((SCREEN_WIDTH - tw) / 2, 96);
        gfx->print(big);
        if (idleClock12Hour) {
          gfx->setTextSize(2);
          gfx->setCursor((SCREEN_WIDTH - 24) / 2, 152);
          gfx->print(ti.tm_hour >= 12 ? "PM" : "AM");
        }
      } else {
        char timeBuf[12];
        if (idleClock12Hour) {
          int hour = ti.tm_hour % 12; if (hour == 0) hour = 12;
          snprintf(timeBuf, sizeof(timeBuf), "%d:%02d %s", hour, ti.tm_min, ti.tm_hour >= 12 ? "PM" : "AM");
        } else {
          snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        }
        gfx->setTextSize(2);
        gfx->setTextColor(userAccentColor);
        gfx->setCursor(6, 5);
        gfx->print(timeBuf);
      }
    }
  }

  // ── Weather badge (top-right) — temperature only, no icon ──
  if (idleShowWeather && weatherCode >= 0 && weatherTempTenths != 0) {
    const int8_t ws = weatherTextSize < 1 ? 1 : (weatherTextSize > 3 ? 3 : weatherTextSize);
    char tmpBuf[10];
    snprintf(tmpBuf, sizeof(tmpBuf), "%dF", weatherTempTenths / 10);
    gfx->setTextSize(ws);
    gfx->setTextColor(userAccentColor);
    int tw = (int)strlen(tmpBuf) * 6 * ws;
    gfx->setCursor(SCREEN_WIDTH - tw - 2, 5);
    gfx->print(tmpBuf);
  }

  // ── Just below the top row: status placard takes priority, else the days counter ──
  if (idleStatusSign.length() > 0) {
    // A little desk placard, e.g. "FOCUSED" / "IN A MEETING".
    int tw = (int)idleStatusSign.length() * 12;     // size 2
    int w = tw + 22;
    if (w > SCREEN_WIDTH - 8) w = SCREEN_WIDTH - 8;
    int x = (SCREEN_WIDTH - w) / 2;
    gfx->fillRoundRect(x, 7, w, 24, 8, COL_ROSE);
    gfx->drawRoundRect(x, 7, w, 24, 8, fxLighten(COL_ROSE, 60));
    gfx->setTextSize(2);
    gfx->setTextColor(COL_FG);
    gfx->setCursor(x + (w - tw) / 2, 13);
    gfx->print(idleStatusSign);
  } else if (anniversaryEpoch > 0) {
    time_t nowt = time(nullptr);
    if (nowt > 1600000000) {                       // only once the clock has synced
      long days = (long)((nowt - (time_t)anniversaryEpoch) / 86400L);
      if (days >= 0) {
        char dbuf[28];
        snprintf(dbuf, sizeof(dbuf), "%ld days together", days);
        gfx->setTextSize(1);
        gfx->setTextColor(COL_ROSE);
        int tw = (int)strlen(dbuf) * 6 + 16;       // text + heart icon
        int sx = (SCREEN_WIDTH - tw) / 2;
        drawIconHeart(sx + 5, 24, 3);
        gfx->setCursor(sx + 16, 20);
        gfx->print(dbuf);
      }
    }
  }

  // ── Hourly weather strip (the cells). Independent of the bottom info line. ──
  const bool cellsShown = idleShowForecast && forecastCount > 0;
  if (cellsShown) {
    gfx->setTextSize(1);
    const int n = forecastCount;
    const int cellW = 56;
    const int startCx = (SCREEN_WIDTH - n * cellW) / 2 + cellW / 2;
    for (int i = 0; i < n; i++) {
      int cx = startCx + i * cellW;
      int h = forecastHourLbl[i];
      int h12 = h % 12; if (h12 == 0) h12 = 12;
      char hl[5]; snprintf(hl, sizeof(hl), "%d%c", h12, h < 12 ? 'a' : 'p');
      gfx->setTextColor(userFaceColor);
      gfx->setCursor(cx - (int)strlen(hl) * 3, 184);
      gfx->print(hl);
      drawMiniWeatherIcon(cx, 200, forecastCode[i]);
      char tl[6]; snprintf(tl, sizeof(tl), "%dF", forecastTempF[i]);
      gfx->setCursor(cx - (int)strlen(tl) * 3, 216);
      gfx->print(tl);
    }
  }

  // ── Bottom info line: sunrise/sunset OR a rotating affirmation (your choice). ──
  // Sits above the weather strip when it's shown, otherwise near the bottom.
  if (idleBottomLine == 1) {
    if (sunriseStr[0] || sunsetStr[0]) {
      char sline[24];
      snprintf(sline, sizeof(sline), "up %s   dn %s", sunriseStr[0] ? sunriseStr : "--:--", sunsetStr[0] ? sunsetStr : "--:--");
      gfx->setTextSize(1);
      gfx->setTextColor(userAccentColor);
      int y = cellsShown ? 166 : (SCREEN_HEIGHT - 24);
      gfx->setCursor((SCREEN_WIDTH - (int)strlen(sline) * 6) / 2, y);
      gfx->print(sline);
    }
  } else if (idleBottomLine == 2) {
    struct tm ta;
    if (getLocalTime(&ta, 5)) {
      long idx = (long)ta.tm_yday * 24 + ta.tm_hour;           // advances every hour
      const char* aff = AFFIRMATIONS[((idx % AFFIRMATION_COUNT) + AFFIRMATION_COUNT) % AFFIRMATION_COUNT];
      int sz = cellsShown ? 1 : idleAffirmationSize;           // shrink to one line above the strip
      int maxLines = cellsShown ? 1 : 3;
      int cy = cellsShown ? 170 : (SCREEN_HEIGHT - 30);
      drawCenteredWrapped(String(aff), sz, cy, maxLines, userAccentColor);
    }
  }

  // ── Bottom bar: IP address or status text ──
  gfx->setTextSize(1);
  gfx->setTextColor(userFaceColor);
  if (idleShowWifi && !ipAddress.isEmpty()) {
    gfx->setCursor(4, SCREEN_HEIGHT - 12);
    gfx->print(ipAddress);
    if (!currentSsid.isEmpty()) {
      int ssidW = currentSsid.length() * 6;
      gfx->setCursor(SCREEN_WIDTH - ssidW - 4, SCREEN_HEIGHT - 12);
      gfx->setTextColor(userAccentColor);
      gfx->print(currentSsid);
    }
  } else if (!statusText.isEmpty()) {
    int sw = (int)statusText.length() * 6;
    gfx->setCursor((SCREEN_WIDTH - sw) / 2, SCREEN_HEIGHT - 12);
    gfx->print(statusText);
  }

  pushCanvas();
}


// ─── Touch menu renderer ───

static const char* MENU_TITLES[] = { "Reactions", "Quick Actions", "Display", "Home Screen" };

struct MenuItem { const char* label; };

static const MenuItem MENU_P0[] = {
  {"Heart"}, {"Thumbs up"}, {"Star"}, {"Hug"}
};
static const MenuItem MENU_P1[] = {
  {"Clear screen"}, {"Show weather"}, {"Show clock"}, {"Goodnight"}
};
static const MenuItem MENU_P2[] = {
  {"Bright +"}, {"Bright -"}, {"Rotate"}, {"Color cycle"}
};
static const MenuItem MENU_P3[] = {
  {"Clock"}, {"Weather"}, {"Face"}, {"WiFi"}
};

static const uint8_t MENU_ITEM_COUNT = 4;

void renderMenuFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);

  // Title bar with close "X"
  gfx->setTextSize(2);
  gfx->setTextColor(userAccentColor);
  const char* title = MENU_TITLES[menuPage % MENU_PAGES];
  gfx->setCursor((SCREEN_WIDTH - (int)strlen(title) * 12) / 2, 8);
  gfx->print(title);
  // Close X in top-right corner
  gfx->setTextColor(userFaceColor);
  gfx->setCursor(SCREEN_WIDTH - 18, 6);
  gfx->print("X");

  // Page indicator dots
  for (uint8_t p = 0; p < MENU_PAGES; p++) {
    int dx = SCREEN_WIDTH / 2 - (MENU_PAGES * 8) / 2 + p * 8 + 2;
    if (p == menuPage)
      gfx->fillCircle(dx, 30, 3, userAccentColor);
    else
      gfx->drawCircle(dx, 30, 3, userFaceColor);
  }

  // Menu items as touch zones (4 items, 40px tall each, starting at y=40)
  const MenuItem* items;
  switch (menuPage) {
    case 0: items = MENU_P0; break;
    case 1: items = MENU_P1; break;
    case 2: items = MENU_P2; break;
    default: items = MENU_P3; break;
  }

  for (uint8_t i = 0; i < MENU_ITEM_COUNT; i++) {
    int y = 42 + i * 42;
    gfx->drawRoundRect(10, y, SCREEN_WIDTH - 20, 36, 8, userFaceColor);
    gfx->setTextSize(2);
    gfx->setTextColor(userFaceColor);
    gfx->setCursor(20, y + 10);
    gfx->print(items[i].label);
    // Show toggle state for Page 3 (Home Screen)
    if (menuPage == 3) {
      bool on = false;
      switch (i) {
        case 0: on = idleShowClock; break;
        case 1: on = idleShowWeather; break;
        case 2: on = idleShowFace; break;
        case 3: on = idleShowWifi; break;
      }
      gfx->setCursor(SCREEN_WIDTH - 50, y + 10);
      gfx->setTextColor(on ? 0x07E0 : COL_ROSE);
      gfx->print(on ? "ON" : "OFF");
    }
  }

  // Navigation arrows
  gfx->setTextSize(3);
  gfx->setTextColor(userFaceColor);
  gfx->setCursor(10, 215);
  gfx->print("<");
  gfx->setCursor(SCREEN_WIDTH - 28, 215);
  gfx->print(">");

  // Debug: show last tap coordinates
  if (lastMenuTapX >= 0) {
    char dbg[40];
    snprintf(dbg, sizeof(dbg), "X:%d Y:%d r(%d,%d)", lastMenuTapX, lastMenuTapY, touchRawX, touchRawY);
    gfx->setTextSize(1);
    gfx->setTextColor(0xFFE0); // yellow
    gfx->setCursor(60, 225);
    gfx->print(dbg);
  }

  pushCanvas();
}

void renderConfirmClear() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);

  gfx->setTextSize(3);
  gfx->setTextColor(userAccentColor);
  gfx->setCursor(90, 40);
  gfx->print("Clear?");

  // Yes button
  gfx->fillRoundRect(30, 100, 110, 60, 12, 0x07E0);
  gfx->setTextSize(3);
  gfx->setTextColor(COL_BG);
  gfx->setCursor(55, 118);
  gfx->print("Yes");

  // No button
  gfx->fillRoundRect(180, 100, 110, 60, 12, COL_ROSE);
  gfx->setTextColor(COL_BG);
  gfx->setCursor(212, 118);
  gfx->print("No");

  pushCanvas();
}

// ─── Menu action handlers ───

void executeMenuAction(uint8_t page, uint8_t item) {
  switch (page) {
    case 0: // Reactions
      switch (item) {
        case 0: startTransientExpression("love", 2500, "Heart sent!"); break;
        case 1: startTransientExpression("happy", 2500, "Thumbs up!"); break;
        case 2: startTransientExpression("sparkle", 2500, "Star!"); break;
        case 3: startTransientExpression("blush", 2500, "Hug!"); break;
      }
      break;
    case 1: // Quick Actions
      switch (item) {
        case 0:
          noteQueueCount = 0; noteQueueIndex = 0;
          currentMode = MODE_IDLE; currentNote = "";
          preferences.begin("desk-cfg", false);
          preferences.remove("note_text");
          preferences.end();
          setIdleStatus("Cleared");
          break;
        case 1:
          currentMode = MODE_WEATHER;
          renderCurrentMode();
          break;
        case 2:
          currentMode = MODE_IDLE;
          renderCurrentMode();
          break;
        case 3:
          setSleepMode();
          break;
      }
      break;
    case 2: // Display
      switch (item) {
        case 0:
          displayBrightness = (displayBrightness <= 230) ? displayBrightness + 25 : 255;
          analogWrite(TFT_BL, displayBrightness);
          preferences.begin("desk-cfg", false);
          preferences.putUChar("brightness", displayBrightness);
          preferences.end();
          break;
        case 1:
          displayBrightness = (displayBrightness >= 25) ? displayBrightness - 25 : 0;
          analogWrite(TFT_BL, displayBrightness);
          preferences.begin("desk-cfg", false);
          preferences.putUChar("brightness", displayBrightness);
          preferences.end();
          break;
        case 2: {
          static const uint8_t LANDSCAPE_MADCTL[] = { 0x28, 0x68, 0xA8, 0xE8 };
          preferences.begin("desk-cfg", false);
          int rot = (preferences.getInt("display_rot", 0) + 1) & 3;
          preferences.putInt("display_rot", rot);
          preferences.end();
          tft.setRotation(1);
          { uint8_t m = LANDSCAPE_MADCTL[rot]; tft.sendCommand(0x36, &m, 1); }
          break;
        }
        case 3:
          // Cycle accent color through a preset list
          {
            static const uint16_t ACCENT_CYCLE[] = { COL_ACCENT, COL_ROSE, COL_GOLD, COL_MINT, COL_LAVENDER, COL_SKYBLUE, COL_PEACH };
            static uint8_t accentIdx = 0;
            accentIdx = (accentIdx + 1) % 7;
            userAccentColor = ACCENT_CYCLE[accentIdx];
            preferences.begin("desk-cfg", false);
            preferences.putUShort("col_accent", userAccentColor);
            preferences.end();
          }
          break;
      }
      break;
    case 3: // Home Screen toggles
      switch (item) {
        case 0: idleShowClock   = !idleShowClock; break;
        case 1: idleShowWeather = !idleShowWeather; break;
        case 2: idleShowFace    = !idleShowFace; break;
        case 3: idleShowWifi    = !idleShowWifi; break;
      }
      preferences.begin("desk-cfg", false);
      preferences.putBool("idle_clock",   idleShowClock);
      preferences.putBool("idle_weather", idleShowWeather);
      preferences.putBool("idle_face",    idleShowFace);
      preferences.putBool("idle_wifi",    idleShowWifi);
      preferences.end();
      break;
  }
  publishStatus();
}

void renderFirecrackerFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);

  unsigned long now = millis();
  unsigned long elapsed = now - firecrackerStartMs;
  float progress = (float)elapsed / (float)firecrackerDurationMs;
  if (progress > 1.0f) progress = 1.0f;

  int totalCrackers = firecrackerCount;

  if (!firecrackerExploded && progress < 1.0f) {
    // ── Draw all firecrackers (they all share ONE fuse) ──
    if (totalCrackers == 1) {
      // Single firecracker: centered
      int bodyX = 140, bodyY = 80, bodyW = 40, bodyH = 80;
      gfx->fillRoundRect(bodyX, bodyY, bodyW, bodyH, 6, COL_ROSE);
      for (int s = 0; s < 4; s++) {
        int sy = bodyY + 10 + s * 18;
        gfx->fillRect(bodyX, sy, bodyW, 4, userAccentColor);
      }
      gfx->fillRoundRect(bodyX - 4, bodyY - 8, bodyW + 8, 14, 4, userEyeColor);
      // Fuse from top, burning down
      float fuseTotal = 60.0f;
      float fuseRemaining = fuseTotal * (1.0f - progress);
      int fuseStartX = bodyX + bodyW / 2;
      int fuseStartY = bodyY - 8;
      for (float f = 0; f < fuseRemaining; f += 2.0f) {
        float fNorm = f / fuseTotal;
        int fx = fuseStartX + (int)(sinf(fNorm * 6.0f) * 12.0f);
        int fy = fuseStartY - (int)f;
        gfx->fillCircle(fx, fy, 1, userEyeColor);
      }
      // Spark at tip
      int sparkX = fuseStartX + (int)(sinf((1.0f - progress) * 6.0f) * 12.0f);
      int sparkY = fuseStartY - (int)(fuseRemaining);
      uint16_t sparkCols[] = {0xFFE0, 0xFBE0, 0xFC00, 0xFD20};
      for (int sp = 0; sp < 6; sp++) {
        gfx->fillCircle(sparkX + random(11) - 5, sparkY + random(7) - 3, 1 + random(2), sparkCols[random(4)]);
      }
    } else {
      // Multiple firecrackers in a row with ONE shared fuse connecting them
      int spacing = 300 / totalCrackers;
      int crackerW = (spacing < 30) ? spacing - 4 : 26;
      if (crackerW < 6) crackerW = 6;
      int crackerH = crackerW * 2;
      int baseY = 150 - crackerH;
      int fuseY = baseY - 6;  // fuse runs along the top of all crackers

      // Draw all firecracker bodies
      for (int c = 0; c < totalCrackers; c++) {
        int cx = 10 + c * spacing + spacing / 2 - crackerW / 2;
        gfx->fillRoundRect(cx, baseY, crackerW, crackerH, 3, COL_ROSE);
        // Stripes
        for (int s = 0; s < 2; s++) {
          int sy = baseY + 4 + s * (crackerH / 3);
          gfx->fillRect(cx, sy, crackerW, 2, userAccentColor);
        }
        // Cap
        gfx->fillRoundRect(cx - 2, baseY - 4, crackerW + 4, 7, 2, userEyeColor);
      }

      // Draw the shared horizontal fuse connecting all crackers
      int firstCX = 10 + spacing / 2;
      int lastCX = 10 + (totalCrackers - 1) * spacing + spacing / 2;
      // The fuse runs from left of first to right of last at fuseY - 12
      int fuseLineY = fuseY - 12;
      float fuseLen = (float)(lastCX - firstCX + 20);
      float burnedLen = fuseLen * progress;  // fuse burns left to right
      // Draw remaining fuse (unburned portion)
      int fuseStartPx = firstCX - 10 + (int)burnedLen;
      int fuseEndPx = lastCX + 10;
      if (fuseStartPx < fuseEndPx) {
        for (int px = fuseStartPx; px <= fuseEndPx; px += 2) {
          int fy = fuseLineY + (int)(sinf((float)(px - firstCX) / 20.0f) * 3.0f);
          gfx->fillCircle(px, fy, 1, userEyeColor);
        }
      }
      // Draw vertical fuse drops from horizontal fuse to each cracker cap
      for (int c = 0; c < totalCrackers; c++) {
        int cx = 10 + c * spacing + spacing / 2;
        for (int dy = fuseLineY; dy <= fuseY; dy += 2) {
          gfx->fillCircle(cx, dy, 1, userEyeColor);
        }
      }
      // Spark traveling along the fuse
      int sparkPx = firstCX - 10 + (int)burnedLen;
      int sparkFY = fuseLineY + (int)(sinf((float)(sparkPx - firstCX) / 20.0f) * 3.0f);
      uint16_t sparkCols[] = {0xFFE0, 0xFBE0, 0xFC00, 0xFD20};
      for (int sp = 0; sp < 6; sp++) {
        gfx->fillCircle(sparkPx + random(7) - 3, sparkFY + random(5) - 2, 1 + random(2), sparkCols[random(4)]);
      }
    }

    // Countdown timer
    if (firecrackerShowCountdown) {
      int secsLeft = (int)((firecrackerDurationMs - elapsed) / 1000) + 1;
      if (secsLeft < 1) secsLeft = 1;
      gfx->setTextSize(totalCrackers == 1 ? 4 : 3);
      gfx->setTextColor(userEyeColor);
      int numW = secsLeft >= 10 ? 36 : 18;
      gfx->setCursor(160 - numW / 2, totalCrackers == 1 ? 180 : 190);
      gfx->print(secsLeft);
    }
  } else {
    // ── Explosion! All firecrackers go off at once ──
    if (!firecrackerExploded) {
      firecrackerExploded = true;
      firecrackerExplodeMs = now;
      // Spread particles across all cracker positions
      int spacing = totalCrackers > 1 ? 300 / totalCrackers : 0;
      uint8_t perCracker = 48 / totalCrackers;
      if (perCracker < 2) perCracker = 2;
      for (uint8_t i = 0; i < 48; i++) {
        int whichCracker = (int)(i / perCracker);
        if (whichCracker >= totalCrackers) whichCracker = totalCrackers - 1;
        int centerX = totalCrackers == 1 ? 160 : (10 + whichCracker * spacing + spacing / 2);
        int centerY = totalCrackers == 1 ? 100 : 110;
        float a = (float)i * 3.14159f * 2.f / (float)perCracker + (random(100) / 200.f);
        int spd = 4 + random(8);
        gPtcl[i].x = centerX;
        gPtcl[i].y = centerY;
        gPtcl[i].vx = (int8_t)((float)spd * cosf(a));
        gPtcl[i].vy = (int8_t)((float)spd * sinf(a));
        gPtcl[i].life = 20 + random(20);
        gPtcl[i].color = pickBurstColor();
      }
    }

    unsigned long explodeElapsed = now - firecrackerExplodeMs;

    // Render explosion particles
    bool anyAlive = false;
    for (uint8_t i = 0; i < 48; i++) {
      if (gPtcl[i].life == 0) continue;
      anyAlive = true;
      gPtcl[i].x += gPtcl[i].vx;
      gPtcl[i].y += gPtcl[i].vy;
      gPtcl[i].vy += 1;
      gPtcl[i].life--;
      int r = gPtcl[i].life > 10 ? 3 : (gPtcl[i].life > 5 ? 2 : 1);
      drawFireworkParticle(gPtcl[i].x, gPtcl[i].y, r, gPtcl[i].color);
    }

    // Show explosion word for the configured duration
    if (firecrackerWord.length() > 0 && explodeElapsed < firecrackerWordDurationMs) {
      int wordLen = firecrackerWord.length();
      int textSize = wordLen <= 4 ? 4 : (wordLen <= 8 ? 3 : 2);
      int charW = textSize * 6;
      gfx->setTextSize(textSize);
      gfx->setTextColor(0xFFE0);
      gfx->setCursor(160 - (wordLen * charW) / 2, 108);
      gfx->print(firecrackerWord.c_str());
    }

    if (!anyAlive && explodeElapsed >= firecrackerWordDurationMs) {
      setIdleStatus("That was fun!");
    }
  }
  pushCanvas();
}

void renderCurrentMode() {
  if (!displayAvailable) return;
  switch (currentMode) {
    case MODE_NOTE:
      drawCurrentNoteBackground();
      break;
    case MODE_BANNER:
      renderBannerFrame();
      break;
    case MODE_IMAGE:
      renderImage();
      break;
    case MODE_COLOR_IMAGE:
      renderColorImage();
      break;
    case MODE_EXPRESSION:
      renderExpressionFrame();
      break;
    case MODE_FLOWER:
      renderFlowerFrame();
      break;
    case MODE_FIREWORKS:
      renderFireworksFrame();
      break;
    case MODE_HEART_RAIN:
      renderHeartRainFrame();
      break;
    case MODE_SNOWFALL:
      renderSnowfallFrame();
      break;
    case MODE_STARFIELD:
      renderStarfieldFrame();
      break;
    case MODE_AURORA:
      renderAuroraFrame();
      break;
    case MODE_FIREFLIES:
      renderFirefliesFrame();
      break;
    case MODE_FALLING_LEAVES:
      renderFallingLeavesFrame();
      break;
    case MODE_STORM:
      renderStormFrame();
      break;
    case MODE_TORNADO:
      renderTornadoFrame();
      break;
    case MODE_COUNTDOWN:
      renderCountdownFrame();
      break;
    case MODE_POMODORO:
      renderPomodoroFrame();
      break;
    case MODE_WEATHER:
      renderWeatherFrame();
      break;
    case MODE_SLEEP:
      renderSleepFrame();
      break;
    case MODE_ANIMATED_NOTE:
      renderAnimatedNoteFrame();
      break;
    case MODE_MENU:
      renderMenuFrame();
      break;
    case MODE_CONFIRM_CLEAR:
      renderConfirmClear();
      break;
    case MODE_FIRECRACKER:
      renderFirecrackerFrame();
      break;
    case MODE_IDLE:
    default:
      renderIdle();
      break;
  }
}

// ─── Particle mode helpers ───

static const uint16_t BURST_COLORS_RAINBOW[] = {
  COL_ROSE, COL_GOLD, COL_SKYBLUE, COL_MINT, COL_PINK, COL_LAVENDER, COL_PEACH, COL_ACCENT
};
static const uint16_t BURST_COLORS_WARM[] = {
  COL_ROSE, COL_GOLD, COL_PEACH, COL_PINK, COL_ROSE, COL_GOLD, COL_PEACH, COL_PINK
};
static const uint16_t BURST_COLORS_COOL[] = {
  COL_SKYBLUE, COL_MINT, COL_LAVENDER, COL_ACCENT, COL_SKYBLUE, COL_MINT, COL_LAVENDER, COL_ACCENT
};

uint16_t pickBurstColor() {
  switch (fireworkPalette) {
    case 1: return BURST_COLORS_WARM[(uint8_t)random(8)];
    case 2: return BURST_COLORS_COOL[(uint8_t)random(8)];
    case 3: return userAccentColor; // mono
    default: return BURST_COLORS_RAINBOW[(uint8_t)random(8)];
  }
}

// Firework states: rocket uses vx=0, vy<0 (ascending), life>100 = rocket phase
// When life drops to 100, it "explodes" into burst particles

uint8_t fwRocketCount = 0; // how many rockets are currently active

void initFireworkRocket() {
  // Launch 1-2 rockets from bottom
  uint8_t count = 1 + (uint8_t)random(2);
  fwRocketCount = count;
  gPtclCount = 48;
  for (uint8_t r = 0; r < count; r++) {
    gPtcl[r].x     = (int16_t)(60 + random(200));
    gPtcl[r].y     = (int16_t)(SCREEN_HEIGHT - 10);
    gPtcl[r].vx    = (int8_t)(random(3) - 1); // slight drift
    gPtcl[r].vy    = (int8_t)(-6 - random(3)); // upward
    gPtcl[r].life  = 200; // >100 means rocket phase
    gPtcl[r].color = COL_GOLD;
  }
  // Clear remaining
  for (uint8_t i = count; i < 48; i++) {
    gPtcl[i].life = 0;
  }
}

FwSizeParams getFireworkSizeParams() {
  uint8_t sz = fireworkSize;
  if (sz == 5) sz = (uint8_t)random(5); // random
  if (sz > 4) sz = 1;
  return FW_SIZES[sz];
}

void explodeRocket(uint8_t rocketIdx) {
  explodeRocketStage(rocketIdx, fireworkStages);
}

void explodeRocketStage(uint8_t rocketIdx, uint8_t stagesLeft) {
  int16_t cx = gPtcl[rocketIdx].x;
  int16_t cy = gPtcl[rocketIdx].y;
  uint16_t col = pickBurstColor();
  gPtcl[rocketIdx].life = 0;
  FwSizeParams sp = getFireworkSizeParams();
  uint8_t placed = 0;
  bool heartBurst = (fireworkShape == 1);
  for (uint8_t i = 0; i < 48 && placed < sp.count; i++) {
    if (gPtcl[i].life > 0) continue;
    if (heartBurst) {
      float t = (float)placed * 3.14159f * 2.f / (float)sp.count;
      float hx = 16.f * sinf(t) * sinf(t) * sinf(t);
      float hy = -(13.f * cosf(t) - 5.f * cosf(2*t) - 2.f * cosf(3*t) - cosf(4*t));
      gPtcl[i].x  = cx;
      gPtcl[i].y  = cy;
      gPtcl[i].vx = (int8_t)(hx * sp.heartScale);
      gPtcl[i].vy = (int8_t)(hy * sp.heartScale);
    } else {
      float a = (float)placed * 3.14159f * 2.f / (float)sp.count + (random(100) / 100.f) * 0.3f;
      int spd = sp.spdMin + (int)random(sp.spdMax - sp.spdMin + 1);
      gPtcl[i].x  = cx;
      gPtcl[i].y  = cy;
      gPtcl[i].vx = (int8_t)((float)spd * cosf(a));
      gPtcl[i].vy = (int8_t)((float)spd * sinf(a));
    }
    gPtcl[i].life  = (uint8_t)(sp.lifeBase + random(20));
    gPtcl[i].color = (placed % 3 == 0) ? COL_GOLD : col;
    placed++;
  }
  // Bright flash bloom at the instant of the burst.
  fwFlashX = cx; fwFlashY = cy; fwFlashLife = 6; fwFlashColor = col;

  // Crackle: several delayed secondary mini-bursts at staggered times/offsets,
  // so the burst keeps popping and sparkling instead of one quiet pop.
  static const uint8_t CRACKLE_DELAYS[] = { 62, 72, 82, 90, 96 };
  uint8_t crackleSeeds = fireworkCrackle ? (3 + (uint8_t)random(3)) : 0;   // 0, or 3-5 pops
  uint8_t seeded = 0;
  for (uint8_t i = 0; i < 48 && seeded < crackleSeeds; i++) {
    if (gPtcl[i].life > 0) continue;
    gPtcl[i].x    = cx + (int16_t)(random(36) - 18);
    gPtcl[i].y    = cy + (int16_t)(random(28) - 14);
    gPtcl[i].vx   = 0;
    gPtcl[i].vy   = 0;
    gPtcl[i].life = CRACKLE_DELAYS[seeded % 5]; // 51-100 → crackle seed
    gPtcl[i].color = (seeded & 1) ? COL_GOLD : col;
    seeded++;
  }
  // Multi-stage: spawn child rocket(s) that will burst again
  if (stagesLeft >= 2) {
    for (uint8_t c = 0; c < (stagesLeft >= 3 ? 2 : 1); c++) {
      for (uint8_t i = 0; i < 48; i++) {
        if (gPtcl[i].life > 0) continue;
        gPtcl[i].x     = cx + (int16_t)(random(40) - 20);
        gPtcl[i].y     = cy;
        gPtcl[i].vx    = (int8_t)(random(5) - 2);
        gPtcl[i].vy    = (int8_t)(-4 - random(2));
        gPtcl[i].life  = 120; // child rocket (>100 = rocket phase)
        gPtcl[i].color = COL_GOLD;
        break;
      }
    }
  }
}

void initFireworks() {
  initFireworkRocket();
}

void renderFireworksFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);

  // Burst flash: a quick bright bloom that expands and fades over a few frames.
  if (fwFlashLife > 0) {
    float f = fwFlashLife / 6.0f;                 // 1 → 0
    int fr = (int)((1.1f - f) * 26.0f) + 6;
    fxGlow(fwFlashX, fwFlashY, fr, fxLighten(fwFlashColor, 120), (uint8_t)(f * 180));
    fxDiscAA(fwFlashX, fwFlashY, 3.0f + f * 4.0f, 0xFFFF, (uint8_t)(f * 230));
    fwFlashLife--;
  }

  bool anyAlive = false;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    if (gPtcl[i].life == 0) continue;
    anyAlive = true;

    // Crackle seed: vx==0 && vy==0 && life 51-99 → waits then pops mini sparks
    if (gPtcl[i].vx == 0 && gPtcl[i].vy == 0 && gPtcl[i].life > 50 && gPtcl[i].life <= 100) {
      gPtcl[i].life--;
      if (gPtcl[i].life == 51) {
        // Crackle pop: spawn 4-6 tiny fast sparks
        int16_t cx = gPtcl[i].x + (int16_t)(random(20) - 10);
        int16_t cy = gPtcl[i].y + (int16_t)(random(20) - 10);
        uint16_t cc = gPtcl[i].color;
        gPtcl[i].life = 0;
        uint8_t sparks = 4 + (uint8_t)random(3);
        for (uint8_t s = 0; s < sparks; s++) {
          for (uint8_t j = 0; j < 48; j++) {
            if (gPtcl[j].life > 0) continue;
            float a = (float)s * 3.14159f * 2.f / (float)sparks + (random(100) / 100.f) * 0.5f;
            gPtcl[j].x    = cx;
            gPtcl[j].y    = cy;
            gPtcl[j].vx   = (int8_t)(3.f * cosf(a));
            gPtcl[j].vy   = (int8_t)(3.f * sinf(a));
            gPtcl[j].life = (uint8_t)(8 + random(8));
            gPtcl[j].color = cc;
            break;
          }
        }
        continue;
      }
      continue; // crackle seed still waiting
    }

    if (gPtcl[i].life > 100) {
      // Rocket phase: ascending
      gPtcl[i].x += gPtcl[i].vx;
      gPtcl[i].y += gPtcl[i].vy;
      gPtcl[i].life--;
      // Explode when reaches target height or life hits 100
      bool targetReached = gPtcl[i].y <= (int16_t)(30 + random(80));
      if (gPtcl[i].life <= 101 || targetReached) {
        gPtcl[i].life = 101; // will be set to 0 in explodeRocket
        explodeRocket(i);
        continue;
      }
      // Draw rocket trail
      int16_t rx = gPtcl[i].x, ry = gPtcl[i].y;
      if (rx >= 0 && rx < SCREEN_WIDTH && ry >= 0 && ry < SCREEN_HEIGHT) {
        gfx->fillCircle(rx, ry, 2, COL_GOLD);
        // Tail sparks
        for (int t = 1; t <= 3; t++) {
          int16_t ty = ry + t * 4;
          if (ty < SCREEN_HEIGHT)
            gfx->drawPixel(rx + (int)random(3) - 1, ty, COL_PEACH);
        }
      }
    } else {
      // Burst phase: expanding particles
      gPtcl[i].x += gPtcl[i].vx;
      gPtcl[i].y += gPtcl[i].vy;
      if (gPtcl[i].life % 2 == 0) gPtcl[i].vy = (int8_t)(gPtcl[i].vy + 1);
      if (gPtcl[i].life % 4 == 0 && gPtcl[i].vx != 0)
        gPtcl[i].vx = (int8_t)(gPtcl[i].vx > 0 ? gPtcl[i].vx - 1 : gPtcl[i].vx + 1);
      gPtcl[i].life--;
      if (gPtcl[i].y >= SCREEN_HEIGHT || gPtcl[i].x < -5 || gPtcl[i].x >= SCREEN_WIDTH + 5) {
        gPtcl[i].life = 0;
        continue;
      }
      int16_t nx = gPtcl[i].x, ny = gPtcl[i].y;
      if (nx >= 0 && nx < SCREEN_WIDTH && ny >= 0 && ny < SCREEN_HEIGHT) {
        int r = (gPtcl[i].life > 30) ? 3 : (gPtcl[i].life > 15) ? 2 : 1;
        // Dying embers twinkle white (the "crackle" shimmer).
        uint16_t pc = (gPtcl[i].life < 14 && (random(3) == 0)) ? 0xFFFF : gPtcl[i].color;
        drawFireworkParticle(nx, ny, r, pc);
        if (gPtcl[i].life > 10) {
          int tx = nx - gPtcl[i].vx;
          int ty = ny - gPtcl[i].vy;
          if (tx >= 0 && tx < SCREEN_WIDTH && ty >= 0 && ty < SCREEN_HEIGHT)
            gfx->drawPixel(tx, ty, gPtcl[i].color);
        }
      }
    }
  }
  if (!anyAlive && !fwManualOnly) initFireworkRocket();
  pushCanvas();
}

void initHeartRain() {
  gPtclCount = 18;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    gPtcl[i].y     = (int16_t)random(SCREEN_HEIGHT - 18);
    gPtcl[i].vx    = 0;
    gPtcl[i].vy    = (int8_t)(1 + random(3));
    gPtcl[i].life  = (uint8_t)(1 + random(3));  // heart size
    gPtcl[i].color = (i % 2 == 0) ? COL_ROSE : COL_PINK;
  }
}

void renderHeartRainFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].y += gPtcl[i].vy;
    if (gPtcl[i].y > SCREEN_HEIGHT - 18) {
      gPtcl[i].y = -10;
      gPtcl[i].x = (int16_t)random(SCREEN_WIDTH);
    }
    int16_t x = gPtcl[i].x, y = gPtcl[i].y;
    int s = gPtcl[i].life;
    uint16_t c = gPtcl[i].color;
    gfx->fillCircle(x - s, y - s / 2, s, c);
    gfx->fillCircle(x + s, y - s / 2, s, c);
    gfx->fillTriangle(x - s * 2, y - s / 2 + 1, x + s * 2, y - s / 2 + 1, x, y + s * 2, c);
  }
  pushCanvas();
}

void initSnowfall() {
  gPtclCount = 28;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    gPtcl[i].y     = (int16_t)random(SCREEN_HEIGHT - 18);
    gPtcl[i].vx    = (int8_t)(random(3) - 1);
    gPtcl[i].vy    = (int8_t)(1 + random(2));
    gPtcl[i].life  = (uint8_t)(1 + random(3));  // snowflake radius
    gPtcl[i].color = userFaceColor;
  }
}

void renderSnowfallFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x += gPtcl[i].vx;
    gPtcl[i].y += gPtcl[i].vy;
    if (gPtcl[i].y >= SCREEN_HEIGHT - 18) { gPtcl[i].y = 0; gPtcl[i].x = (int16_t)random(SCREEN_WIDTH); }
    if (gPtcl[i].x < 0)             gPtcl[i].x = SCREEN_WIDTH - 1;
    if (gPtcl[i].x >= SCREEN_WIDTH) gPtcl[i].x = 0;
    gfx->fillCircle(gPtcl[i].x, gPtcl[i].y, gPtcl[i].life, gPtcl[i].color);
  }
  pushCanvas();
}

void initStarfield() {
  gPtclCount = 36;
  const int CX = SCREEN_WIDTH / 2, CY = (SCREEN_HEIGHT - 18) / 2;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x     = (int16_t)(CX + random(41) - 20);
    gPtcl[i].y     = (int16_t)(CY + random(41) - 20);
    gPtcl[i].vx    = 0;
    gPtcl[i].vy    = 0;
    gPtcl[i].life  = (uint8_t)(4 + random(60));  // Z depth: large=far, small=near
    gPtcl[i].color = userFaceColor;
  }
}

void renderStarfieldFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  const int CX = SCREEN_WIDTH / 2, CY = (SCREEN_HEIGHT - 18) / 2;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    float z = gPtcl[i].life / 64.f + 0.01f;
    int sx = CX + (int)((gPtcl[i].x - CX) / z);
    int sy = CY + (int)((gPtcl[i].y - CY) / z);
    gPtcl[i].life--;
    if (gPtcl[i].life == 0 || sx < 0 || sx >= SCREEN_WIDTH || sy < 0 || sy >= SCREEN_HEIGHT - 18) {
      gPtcl[i].x    = (int16_t)(CX + random(41) - 20);
      gPtcl[i].y    = (int16_t)(CY + random(41) - 20);
      gPtcl[i].life = (uint8_t)(50 + random(14));
    } else {
      int r = (gPtcl[i].life < 20) ? 2 : 1;
      uint16_t c = (gPtcl[i].life < 20) ? userAccentColor : userFaceColor;
      gfx->fillCircle(sx, sy, r, c);
    }
  }
  pushCanvas();
}

// ─── Aurora: flowing translucent ribbons of light over a starry night ───
void renderAuroraFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  const float clk = millis() * 0.0011f;
  // sparse stars in the upper sky
  for (int i = 0; i < 28; i++) {
    int sx = (i * 53 + 7) % SCREEN_WIDTH;
    int sy = (i * 31) % (SCREEN_HEIGHT / 2);
    fxDiscAA(sx, sy, (i % 5 == 0) ? 1.4f : 0.9f, COL_FG, 160);
  }
  uint16_t band[3] = { fxRGB(40, 200, 140), fxRGB(60, 140, 220), fxRGB(150, 90, 220) };
  for (int b = 0; b < 3; b++) {
    const uint16_t col = band[b];
    const float baseY = SCREEN_HEIGHT * (0.34f + b * 0.16f);
    const int hh = 30 + b * 6;
    for (int x = 0; x < SCREEN_WIDTH; x += 2) {
      float y = baseY + sinf(x * 0.018f + clk * (1.0f + b * 0.3f) + b) * 22.0f
                      + sinf(x * 0.043f + clk * 1.7f) * 8.0f;
      int top = (int)(y - hh * 0.5f);
      for (int yy = 0; yy < hh; yy++) {
        float f = 1.0f - fabsf(yy - hh * 0.5f) / (hh * 0.5f);
        uint8_t a = (uint8_t)(f * 70.0f);
        fxBlend(x, top + yy, col, a);
        fxBlend(x + 1, top + yy, col, a);
      }
    }
  }
  pushCanvas();
}

// ─── Fireflies: slow-drifting glows that breathe in and out at dusk ───
void initFireflies() {
  gPtclCount = 22;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    gPtcl[i].y     = (int16_t)random(SCREEN_HEIGHT);
    gPtcl[i].vx    = (int8_t)(random(3) - 1);
    gPtcl[i].vy    = (int8_t)(random(3) - 1);
    gPtcl[i].life  = (uint8_t)random(64);
    gPtcl[i].color = (i % 4 == 0) ? COL_MINT : COL_GOLD;
  }
}

void renderFirefliesFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(fxRGB(10, 14, 26));   // solid deep dusk (no gradient banding)
  const float clk = millis() * 0.004f;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x += gPtcl[i].vx;
    gPtcl[i].y += gPtcl[i].vy;
    if (random(40) == 0) { gPtcl[i].vx = (int8_t)(random(3) - 1); gPtcl[i].vy = (int8_t)(random(3) - 1); }
    if (gPtcl[i].x < 0) gPtcl[i].x = SCREEN_WIDTH - 1;
    if (gPtcl[i].x >= SCREEN_WIDTH) gPtcl[i].x = 0;
    if (gPtcl[i].y < 0) gPtcl[i].y = SCREEN_HEIGHT - 1;
    if (gPtcl[i].y >= SCREEN_HEIGHT) gPtcl[i].y = 0;
    float glow = sinf(clk + gPtcl[i].life) * 0.5f + 0.5f;
    uint8_t a = (uint8_t)(glow * 230.0f);
    fxGlow(gPtcl[i].x, gPtcl[i].y, 6.0f + glow * 4.0f, gPtcl[i].color, a);
    fxDiscAA(gPtcl[i].x, gPtcl[i].y, 1.6f, fxLighten(gPtcl[i].color, 80), a);
  }
  pushCanvas();
}

// ─── Falling leaves: warm autumn leaves tumbling and swaying down ───
void initFallingLeaves() {
  gPtclCount = 26;
  uint16_t pal[4] = { fxRGB(220, 120, 30), fxRGB(230, 180, 40), fxRGB(180, 60, 40), fxRGB(210, 100, 60) };
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    gPtcl[i].y     = (int16_t)(random(SCREEN_HEIGHT) - SCREEN_HEIGHT);
    gPtcl[i].vx    = 0;
    gPtcl[i].vy    = (int8_t)(1 + random(2));
    gPtcl[i].life  = (uint8_t)random(64);   // sway phase seed
    gPtcl[i].color = pal[i % 4];
  }
}

void renderFallingLeavesFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  const float clk = millis() * 0.003f;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].y += gPtcl[i].vy;
    float swA = clk * 1.4f + gPtcl[i].life;
    int x = gPtcl[i].x + (int)(sinf(swA) * 16.0f);
    int y = gPtcl[i].y;
    if (y > SCREEN_HEIGHT + 8) { gPtcl[i].y = (int16_t)(-8 - random(40)); gPtcl[i].x = (int16_t)random(SCREEN_WIDTH); }
    // tumbling leaf: axis-aligned ellipse that narrows as it turns edge-on
    fxEllipseAA(x, y, 3.5f + 3.0f * fabsf(cosf(swA)), 3.5f + 1.5f * fabsf(sinf(swA)), gPtcl[i].color);
    fxThickLine(x, y, x - 3, y + 3, 0.9f, fxDarken(gPtcl[i].color, 40), 160);  // stem
  }
  pushCanvas();
}

// ─── Thunderstorm scene: a deep night-storm — lightning lights the whole sky,
// dark clouds silhouette against it, depth-layered rain drives down. ───
void renderStormFrame() {
  if (!displayAvailable) return;
  const unsigned long m = millis() % 3400UL;
  const bool flash = (m < 70UL);
  // Pure black base (this panel shows any non-black fill as a gray wash). Lightning
  // briefly lights the whole sky via a bright fill; otherwise it's true black.
  gfx->fillScreen(flash ? fxRGB(150, 162, 205) : COL_BG);
  // Layered billowing clouds (back darker, front lifted). They read as dark
  // silhouettes and pop dramatically when the sky flashes behind them.
  for (int layer = 0; layer < 2; layer++) {
    const uint16_t cl = layer == 0 ? fxRGB(20, 22, 32) : fxRGB(36, 40, 54);
    const int yb = layer == 0 ? 18 : 28, step = layer == 0 ? 52 : 66;
    for (int cxp = -20; cxp < SCREEN_WIDTH + 20; cxp += step) {
      fxDiscAA(cxp, yb, 22, cl);
      fxDiscAA(cxp + 22, yb + 6, 27, cl);
      fxDiscAA(cxp + 46, yb, 20, cl);
    }
  }
  // Individual scattered raindrops — each falls at its own speed and respawns at a
  // fresh random x, so they never line up into moving rows. 3 depth layers.
  static bool rainInit = false;
  static float rX[96], rY[96], rSpd[96];
  static uint8_t rLay[96];
  if (!rainInit) {
    for (int i = 0; i < 96; i++) {
      rX[i] = (float)random(SCREEN_WIDTH);
      rY[i] = (float)random(SCREEN_HEIGHT);
      rLay[i] = i % 3;
      rSpd[i] = 7.0f + rLay[i] * 5.0f + random(20) / 10.0f;
    }
    rainInit = true;
  }
  for (int i = 0; i < 96; i++) {
    rY[i] += rSpd[i];
    if (rY[i] >= SCREEN_HEIGHT) { rY[i] -= SCREEN_HEIGHT; rX[i] = (float)random(SCREEN_WIDTH); }
    const int len = 5 + rLay[i] * 5;
    const uint16_t rc = rLay[i] == 0 ? fxRGB(54, 70, 104) : rLay[i] == 1 ? fxRGB(92, 118, 165) : fxRGB(150, 180, 222);
    const int x = (int)rX[i], y = (int)rY[i];
    gfx->drawLine(x, y, x - (2 + rLay[i]), y + len, rc);   // slight wind slant per layer
  }
  // Forked bolt: soft glow underlay + hot white core, branching once (~every 3.4s).
  static int boltSeed = 0;
  if (m < 30UL) boltSeed = (int)(millis() & 0x7fff);
  if (m < 220UL) {
    const uint16_t hot = 0xFFFF, glow = fxRGB(170, 195, 255);
    float zx = 46 + (boltSeed % 5) * 50.0f, zy = 2;
    static const float dxs[] = { -12, 14, -9, 13, -8, 11, -7 };
    static const float dys[] = { 18, 20, 17, 19, 18, 20, 17 };
    for (int s2 = 0; s2 < 7; s2++) {
      float nx = zx + dxs[s2], ny = zy + dys[s2];
      fxThickLine(zx, zy, nx, ny, 3.6f, glow, 200);
      fxThickLine(zx, zy, nx, ny, 1.6f, hot);
      if (s2 == 3) {
        fxThickLine(nx, ny, nx + 15, ny + 20, 2.2f, glow, 170);
        fxThickLine(nx + 15, ny + 20, nx + 9, ny + 40, 1.4f, hot);
      }
      zx = nx; zy = ny;
    }
  }
  pushCanvas();
}

// A small cow silhouette. facing = +1/-1 (flips it horizontally to face travel);
// walkPhase >= 0 animates a trotting leg cycle (walking), < 0 = splayed legs (flung).
void drawFlyingCow(float cx, float cy, float s, float rot, uint8_t alpha, int facing, float walkPhase) {
  const float c = cosf(rot), sn = sinf(rot);
  auto wx = [&](float lx, float ly) { return cx + (lx * facing) * c - ly * sn; };
  auto wy = [&](float lx, float ly) { return cy + (lx * facing) * sn + ly * c; };
  const uint16_t body = fxRGB(245, 245, 245), spot = fxRGB(44, 44, 50), snout = fxRGB(232, 150, 160);
  for (int i = 0; i < 4; i++) {                    // legs
    float lx = -s * 0.6f + i * s * 0.4f;
    float footY = s * 0.95f;
    if (walkPhase >= 0.0f) footY -= fmaxf(0.0f, sinf(walkPhase * 6.2832f + i * 1.5708f)) * s * 0.34f;
    fxThickLine(wx(lx, s * 0.45f), wy(lx, s * 0.45f), wx(lx, footY), wy(lx, footY), s * 0.16f, spot, alpha);
  }
  fxEllipseAA(wx(0, 0), wy(0, 0), s * 1.1f, s * 0.66f, body, alpha);             // body
  fxDiscAA(wx(-s * 0.35f, -s * 0.08f), wy(-s * 0.35f, -s * 0.08f), s * 0.3f, spot, alpha); // spots
  fxDiscAA(wx(s * 0.3f, s * 0.16f), wy(s * 0.3f, s * 0.16f), s * 0.24f, spot, alpha);
  fxDiscAA(wx(s * 1.0f, -s * 0.18f), wy(s * 1.0f, -s * 0.18f), s * 0.42f, body, alpha);     // head
  fxDiscAA(wx(s * 1.22f, -s * 0.02f), wy(s * 1.22f, -s * 0.02f), s * 0.2f, snout, alpha);   // snout
}

// A small tumbling house silhouette for the tornado to fling.
void drawFlyingHouse(float cx, float cy, float s, float rot, uint8_t alpha) {
  const float c = cosf(rot), sn = sinf(rot);
  auto wx = [&](float lx, float ly) { return cx + lx * c - ly * sn; };
  auto wy = [&](float lx, float ly) { return cy + lx * sn + ly * c; };
  const uint16_t wall = fxRGB(205, 178, 140), roof = fxRGB(150, 62, 52), door = fxRGB(92, 62, 42), win = fxRGB(150, 200, 232);
  fxFillTriAA(wx(-s, 0), wy(-s, 0), wx(s, 0), wy(s, 0), wx(-s, s), wy(-s, s), wall, alpha);   // wall
  fxFillTriAA(wx(s, 0), wy(s, 0), wx(s, s), wy(s, s), wx(-s, s), wy(-s, s), wall, alpha);
  fxFillTriAA(wx(-s * 1.25f, 0), wy(-s * 1.25f, 0), wx(s * 1.25f, 0), wy(s * 1.25f, 0), wx(0, -s * 0.95f), wy(0, -s * 0.95f), roof, alpha); // roof
  fxFillTriAA(wx(-s * 0.26f, s), wy(-s * 0.26f, s), wx(s * 0.26f, s), wy(s * 0.26f, s), wx(-s * 0.26f, s * 0.42f), wy(-s * 0.26f, s * 0.42f), door, alpha); // door
  fxDiscAA(wx(s * 0.42f, s * 0.46f), wy(s * 0.42f, s * 0.46f), s * 0.18f, win, alpha);        // window
}

// A planted tree that bends/twists toward the funnel as it nears (never sucked up).
void drawTree(float baseX, float groundY, float lean, float gust) {
  const uint16_t bark = fxRGB(96, 64, 40), barkD = fxRGB(70, 46, 28);
  const uint16_t leafA = fxRGB(48, 120, 56), leafB = fxRGB(74, 152, 74);
  const float midX = baseX + lean * 0.4f, midY = groundY - 20;
  const float topX = baseX + lean,        topY = groundY - 42;
  fxThickLine(baseX, groundY, midX, midY, 5.0f, barkD);     // lower trunk
  fxThickLine(midX, midY, topX, topY, 3.4f, bark);          // upper trunk bends in the wind
  for (int i = 0; i < 5; i++) {                             // canopy, twisting with the gust
    float a = i * 1.2566f + gust;
    float r = 9.0f + (i % 2) * 4.0f;
    fxDiscAA(topX + cosf(a) * 9.0f + gust * 1.5f, topY + sinf(a) * 7.0f, r, (i & 1) ? leafA : leafB);
  }
  if (fabsf(lean) > 24.0f) {                                // leaves torn off in a strong gust
    float lp = fmodf((float)millis() * 0.002f + baseX, 1.0f);
    fxDiscAA(topX + lean * lp * 1.5f, topY - lp * 16.0f, 2.0f, leafB, (uint8_t)(200 * (1.0f - lp)));
  }
}

// ─── Tornado scene: a swirling funnel that wanders left/right with flung debris,
// under an ominous green-tinged storm sky — with an optional cow/house caught in it. ───
void initTornado() {
  gPtclCount = 26;
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].x    = (int16_t)random(SCREEN_WIDTH);
    gPtcl[i].y    = (int16_t)(SCREEN_HEIGHT - 30 + random(28));
    gPtcl[i].vx   = (int8_t)(random(7) - 3);
    gPtcl[i].vy   = (int8_t)(random(64));      // swirl phase seed
    gPtcl[i].life = (uint8_t)(40 + random(120));
    gPtcl[i].color = (i % 3 == 0) ? fxRGB(120, 96, 70) : (i % 3 == 1) ? fxRGB(96, 90, 84) : fxRGB(140, 120, 92);
  }
}

void renderTornadoFrame() {
  if (!displayAvailable) return;
  // Pure-black sky (any non-black fill reads as a gray wash on this panel). The
  // ominous green-storm mood comes from drawn low clouds, not a full-screen tint.
  gfx->fillScreen(COL_BG);
  for (int cxp = -20; cxp < SCREEN_WIDTH + 20; cxp += 58) {
    fxDiscAA(cxp, 16, 20, fxRGB(40, 50, 42));
    fxDiscAA(cxp + 24, 22, 24, fxRGB(34, 44, 36));
    fxDiscAA(cxp + 48, 16, 18, fxRGB(40, 50, 42));
  }
  gfx->fillRect(0, SCREEN_HEIGHT - 10, SCREEN_WIDTH, 10, fxRGB(46, 36, 24));   // ground

  const float clk = millis() * 0.006f;
  // Wander the funnel back and forth with a little randomness; bounce off edges.
  gTornadoX += gTornadoVX;
  if (random(60) == 0) gTornadoVX += (random(3) - 1) * 0.4f;
  if (gTornadoVX > 2.4f) gTornadoVX = 2.4f;
  if (gTornadoVX < -2.4f) gTornadoVX = -2.4f;
  if (gTornadoX < 40) { gTornadoX = 40; gTornadoVX = fabsf(gTornadoVX); }
  if (gTornadoX > SCREEN_WIDTH - 40) { gTornadoX = SCREEN_WIDTH - 40; gTornadoVX = -fabsf(gTornadoVX); }

  const int baseY = SCREEN_HEIGHT - 10;
  const int topY = 22;
  const uint16_t dark = fxRGB(74, 68, 64), mid = fxRGB(122, 112, 102), lite = fxRGB(178, 168, 152);
  // Funnel geometry, reused below for the flung object so it hugs the funnel.
  auto funnelCxAt = [&](float f) {
    return gTornadoX + sinf(clk + f * 7.0f) * (6.0f + (1.0f - f) * 14.0f)
                     + sinf((float)millis() * 0.0013f) * 10.0f * (1.0f - f);
  };
  for (int y = topY; y <= baseY; y += 5) {
    float f = (float)(y - topY) / (float)(baseY - topY);      // 0 top → 1 bottom
    float halfW = (4.0f + (1.0f - f) * 44.0f);
    float cx = funnelCxAt(f);
    uint16_t band = ((y / 5) & 1) ? mid : dark;
    fxEllipseAA(cx, y, halfW, 3.4f, band, 220);
    fxEllipseAA(cx + halfW * 0.4f, y, halfW * 0.3f, 2.6f, lite, 150);   // rotation highlight
  }
  // Debris swirling near the base.
  for (uint8_t i = 0; i < gPtclCount; i++) {
    gPtcl[i].vy = (uint8_t)(gPtcl[i].vy + 4);
    float ph = gPtcl[i].vy * 0.05f + i;
    float orbit = 18.0f + (i % 5) * 8.0f;
    int x = (int)(gTornadoX + cosf(ph) * orbit);
    int y = baseY - (i % 7) * 10 - (int)(fabsf(sinf(ph)) * 8.0f);
    fxDiscAA(x, y, 1.8f, gPtcl[i].color, 220);
  }
  // Dust kicked along the ground.
  for (int i = 0; i < 10; i++) {
    int dx = (int)(gTornadoX) + (i - 5) * 12;
    fxDiscAA(dx, baseY - random(4), 2.2f, fxRGB(110, 100, 84), 120);
  }
  // Trees: planted scenery that leans/twists toward the funnel (never sucked up).
  // Drawn independently so they can share the scene with a flung cow/house.
  if (gTornadoTrees) {
    const int treeX[3] = { SCREEN_WIDTH / 5, SCREEN_WIDTH / 2, SCREEN_WIDTH * 4 / 5 };
    for (int i = 0; i < 3; i++) {
      float dist = fabsf((float)treeX[i] - gTornadoX);
      float prox = fmaxf(0.0f, 1.0f - dist / 130.0f);          // 0 far .. 1 funnel overhead
      float gust = sinf((float)millis() * 0.005f + i * 2.0f) * (0.4f + prox);
      float lean = (gTornadoX - (float)treeX[i]) * (0.08f + prox * 0.22f) + gust * 6.0f; // pulled toward funnel
      drawTree((float)treeX[i], (float)baseY, lean, gust);
    }
  }
  // Flung prop.
  if (gTornadoObject != 0) {
    // Cow/house: enter from the side OPPOSITE the funnel, travel along the ground,
    // then get sucked up the funnel, tumbling, and flung off the top.
    float p = (float)(millis() % 9000UL) / 9000.0f;
    float startX = (gTornadoX < SCREEN_WIDTH / 2) ? (SCREEN_WIDTH + 24.0f) : -24.0f;
    int facing = (startX > gTornadoX) ? -1 : 1;                // face direction of travel
    if (p < 0.45f) {
      // Phase A — approach along the ground (cow walks; house drags).
      float wp = p / 0.45f;
      float ox = startX + (gTornadoX - startX) * wp;
      float oy = baseY - 8.0f;
      if (gTornadoObject == 1) drawFlyingCow(ox, oy - fabsf(sinf(wp * 18.0f)) * 2.0f, 10.0f, 0.0f, 255, facing, wp * 6.0f);
      else drawFlyingHouse(ox, oy, 10.0f, 0.0f, 255);
    } else {
      // Phase B — spiral up the funnel, tumbling, fade out as it's flung away.
      float sp = (p - 0.45f) / 0.55f;
      int objY = baseY - (int)(easeOutCubic(sp) * (baseY - topY - 8));
      float f = (float)(objY - topY) / (float)(baseY - topY);
      float orbitR = 10.0f + (1.0f - f) * 40.0f;
      float ang = sp * 16.0f;
      float ox = funnelCxAt(f) + cosf(ang) * orbitR;
      float oy = objY + sinf(ang) * 4.0f;
      float rot = ang * 1.3f;
      float sc = 10.0f - sp * 4.0f;
      uint8_t a = (sp > 0.85f) ? (uint8_t)(255.0f * (1.0f - (sp - 0.85f) / 0.15f)) : 255;
      if (gTornadoObject == 1) drawFlyingCow(ox, oy, sc, rot, a, 1, -1.0f);
      else drawFlyingHouse(ox, oy, sc, rot, a);
    }
  }
  pushCanvas();
}

// ─── Firework shape helpers ───

void drawSmallHeart(int16_t cx, int16_t cy, int s, uint16_t c) {
  if (s < 1) s = 1;
  fxDiscAA(cx - s * 0.55f, cy - s * 0.45f, s * 0.7f, c);
  fxDiscAA(cx + s * 0.55f, cy - s * 0.45f, s * 0.7f, c);
  gfx->fillTriangle(cx - s * 1.3f, cy - s * 0.3f, cx + s * 1.3f, cy - s * 0.3f, cx, cy + s * 1.7f, c);
  fxDiscAA(cx - s * 0.5f, cy - s * 0.5f, s * 0.22f, fxLighten(c, 140), 180);
}

void drawSmallStar(int16_t cx, int16_t cy, int s, uint16_t c) {
  if (s < 1) s = 1;
  fxStar(cx, cy, s * 1.8f, c, false);
}

void drawFireworkParticle(int16_t x, int16_t y, int r, uint16_t c) {
  uint8_t shape = fireworkShape;
  if (shape == 3) shape = random(3); // random picks 0/1/2
  switch (shape) {
    case 1: drawSmallHeart(x, y, r, c); break;
    case 2: drawSmallStar(x, y, r, c);  break;
    default:
      fxGlow(x, y, r * 2.2f, c, 90);
      fxDiscAA(x, y, r, c);
      break;
  }
}

// ─── Animated note overlay ───

void initNoteOverlayWater() {
  noteOvCount = 20;
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)random(SCREEN_WIDTH + 40);
    noteOvPtcl[i].y     = (int16_t)(SCREEN_HEIGHT - 50 + random(50));
    noteOvPtcl[i].vx    = (int8_t)(-2 - random(3));
    noteOvPtcl[i].vy    = 0;
    noteOvPtcl[i].life  = (uint8_t)(1 + random(3));
    noteOvPtcl[i].color = (i % 3 == 0) ? COL_SKYBLUE : (i % 3 == 1) ? COL_MINT : COL_ACCENT;
  }
}

void initNoteOverlayStars() {
  noteOvCount = 12;
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)(SCREEN_WIDTH + random(80));
    noteOvPtcl[i].y     = (int16_t)random(SCREEN_HEIGHT / 2);
    noteOvPtcl[i].vx    = (int8_t)(-4 - random(4));
    noteOvPtcl[i].vy    = (int8_t)(2 + random(2));
    noteOvPtcl[i].life  = (uint8_t)(30 + random(40));
    noteOvPtcl[i].color = (i % 3 == 0) ? COL_GOLD : (i % 3 == 1) ? COL_PEACH : userFaceColor;
  }
}

void initNoteOverlayFlowers() {
  noteOvCount = 8;
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)(20 + random(SCREEN_WIDTH - 40));
    noteOvPtcl[i].y     = (int16_t)(SCREEN_HEIGHT + random(30));
    noteOvPtcl[i].vx    = 0;
    noteOvPtcl[i].vy    = (int8_t)(-1);
    noteOvPtcl[i].life  = (uint8_t)(40 + random(40));
    noteOvPtcl[i].color = (i % 4 == 0) ? COL_ROSE : (i % 4 == 1) ? COL_PINK : (i % 4 == 2) ? COL_LAVENDER : COL_PEACH;
  }
}

void initNoteOverlayFireworks() {
  noteOvCount = 16;
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)(40 + random(SCREEN_WIDTH - 80));
    noteOvPtcl[i].y     = (int16_t)(20 + random(SCREEN_HEIGHT / 2));
    float a = (float)i * 3.14159f * 2.f / 16.f;
    int spd = 1 + (int)random(3);
    noteOvPtcl[i].vx    = (int8_t)((float)spd * cosf(a));
    noteOvPtcl[i].vy    = (int8_t)((float)spd * sinf(a));
    noteOvPtcl[i].life  = (uint8_t)(20 + random(25));
    noteOvPtcl[i].color = pickBurstColor();
  }
}

void initNoteOverlaySnowfall() {
  noteOvCount = 18;
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    noteOvPtcl[i].y     = (int16_t)random(SCREEN_HEIGHT);
    noteOvPtcl[i].vx    = (int8_t)(random(3) - 1);
    noteOvPtcl[i].vy    = (int8_t)(1 + random(2));
    noteOvPtcl[i].life  = (uint8_t)(1 + random(3));
    noteOvPtcl[i].color = userFaceColor;
  }
}

void initNoteOverlayStarfield() {
  noteOvCount = 20;
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)(SCREEN_WIDTH / 2 + random(41) - 20);
    noteOvPtcl[i].y     = (int16_t)(SCREEN_HEIGHT / 2 + random(41) - 20);
    noteOvPtcl[i].vx    = 0;
    noteOvPtcl[i].vy    = 0;
    noteOvPtcl[i].life  = (uint8_t)(4 + random(60));
    noteOvPtcl[i].color = userFaceColor;
  }
}

void initNoteOverlayBloomingGarden() {
  noteOvCount = 5;
  static const uint16_t pal[] = { COL_ROSE, COL_GOLD, COL_LAVENDER, COL_PINK, COL_SKYBLUE };
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)(30 + i * ((SCREEN_WIDTH - 60) / 4));
    noteOvPtcl[i].y     = 0;
    noteOvPtcl[i].vx    = (int8_t)(i % 5);            // flower type index
    noteOvPtcl[i].vy    = (int8_t)(2 + (i % 3));      // growth speed
    noteOvPtcl[i].life  = (uint8_t)(i * 30);          // staggered bloom phase
    noteOvPtcl[i].color = pal[i % 5];
  }
}

void initNoteOverlayConfetti() {
  noteOvCount = 24;
  static const uint16_t pal[] = { COL_ROSE, COL_GOLD, COL_SKYBLUE, COL_MINT, COL_LAVENDER, COL_PEACH };
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    noteOvPtcl[i].y     = (int16_t)(random(SCREEN_HEIGHT) - SCREEN_HEIGHT);  // start above
    noteOvPtcl[i].vx    = (int8_t)(random(5) - 2);
    noteOvPtcl[i].vy    = (int8_t)(2 + random(3));
    noteOvPtcl[i].life  = (uint8_t)random(64);  // spin phase
    noteOvPtcl[i].color = pal[i % 6];
  }
}

void initNoteOverlayBubbles() {
  noteOvCount = 18;
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    noteOvPtcl[i].y     = (int16_t)(SCREEN_HEIGHT + random(SCREEN_HEIGHT));  // start below
    noteOvPtcl[i].vx    = 0;
    noteOvPtcl[i].vy    = (int8_t)(1 + random(3));   // rise speed
    noteOvPtcl[i].life  = (uint8_t)(3 + random(8));  // radius
    noteOvPtcl[i].color = (i % 3 == 0) ? COL_SKYBLUE : (i % 3 == 1) ? COL_MINT : fxLighten(COL_LAVENDER, 60);
  }
}

void initNoteOverlayFallingHearts() {
  noteOvCount = 16;
  static const uint16_t pal[] = { COL_ROSE, COL_PINK, COL_PEACH };
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    noteOvPtcl[i].y     = (int16_t)(random(SCREEN_HEIGHT) - SCREEN_HEIGHT);
    noteOvPtcl[i].vx    = 0;
    noteOvPtcl[i].vy    = (int8_t)(1 + random(3));
    noteOvPtcl[i].life  = (uint8_t)(5 + random(6));   // size
    noteOvPtcl[i].color = pal[i % 3];
  }
}

void initNoteOverlayBalloons() {
  noteOvCount = 12;
  static const uint16_t pal[] = { COL_ROSE, COL_GOLD, COL_SKYBLUE, COL_MINT, COL_LAVENDER };
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    noteOvPtcl[i].y     = (int16_t)(SCREEN_HEIGHT + random(SCREEN_HEIGHT));
    noteOvPtcl[i].vx    = 0;
    noteOvPtcl[i].vy    = (int8_t)(1 + random(2));
    noteOvPtcl[i].life  = (uint8_t)(8 + random(7));   // radius
    noteOvPtcl[i].color = pal[i % 5];
  }
}

void initNoteOverlayMusicNotes() {
  noteOvCount = 14;
  static const uint16_t pal[] = { COL_GOLD, COL_LAVENDER, COL_PINK, COL_SKYBLUE };
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    noteOvPtcl[i].y     = (int16_t)(SCREEN_HEIGHT + random(SCREEN_HEIGHT));
    noteOvPtcl[i].vx    = 0;
    noteOvPtcl[i].vy    = (int8_t)(1 + random(3));
    noteOvPtcl[i].life  = 0;
    noteOvPtcl[i].color = pal[i % 4];
  }
}

void initNoteOverlaySparkles() {
  noteOvCount = 22;
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    noteOvPtcl[i].y     = (int16_t)random(SCREEN_HEIGHT);
    noteOvPtcl[i].vx    = 0;
    noteOvPtcl[i].vy    = 0;
    noteOvPtcl[i].life  = (uint8_t)random(40);
    noteOvPtcl[i].color = (i % 3 == 0) ? COL_GOLD : (i % 3 == 1) ? 0xFFFF : COL_PEACH;
  }
}

void initNoteOverlayAutumnLeaves() {
  noteOvCount = 16;
  static const uint16_t pal[] = { 0xFC80, COL_GOLD, 0xC2A8, COL_PEACH };  // orange, gold, rust, peach
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    noteOvPtcl[i].y     = (int16_t)(random(SCREEN_HEIGHT) - SCREEN_HEIGHT);
    noteOvPtcl[i].vx    = 0;
    noteOvPtcl[i].vy    = (int8_t)(1 + random(2));
    noteOvPtcl[i].life  = 0;
    noteOvPtcl[i].color = pal[i % 4];
  }
}

void initNoteOverlayGentleRain() {
  noteOvCount = 24;
  for (uint8_t i = 0; i < noteOvCount; i++) {
    noteOvPtcl[i].x     = (int16_t)random(SCREEN_WIDTH);
    noteOvPtcl[i].y     = (int16_t)random(SCREEN_HEIGHT);
    noteOvPtcl[i].vx    = 0;
    noteOvPtcl[i].vy    = (int8_t)(4 + random(3));
    noteOvPtcl[i].life  = 0;
    noteOvPtcl[i].color = fxLighten(COL_SKYBLUE, 30);
  }
}

void initNoteOverlay() {
  switch (noteAnimType) {
    case 1: initNoteOverlayWater();          break;
    case 2: initNoteOverlayStars();          break;
    case 3: initNoteOverlayFlowers();        break;
    case 4: initNoteOverlayFireworks();      break;
    case 5: initNoteOverlaySnowfall();       break;
    case 6: initNoteOverlayStarfield();      break;
    case 7: initNoteOverlayBloomingGarden(); break;
    case 8: initNoteOverlayConfetti();       break;
    case 9: initNoteOverlayBubbles();        break;
    case 10: initNoteOverlayFallingHearts(); break;
    case 11: initNoteOverlayBalloons();      break;
    case 12: initNoteOverlayMusicNotes();    break;
    case 13: initNoteOverlaySparkles();      break;
    case 14: initNoteOverlayAutumnLeaves();  break;
    case 15: initNoteOverlayGentleRain();    break;
    default: noteOvCount = 0;                break;
  }
}

void drawFlowerShape(int16_t cx, int16_t cy, int s, uint16_t c) {
  // Simple 4-petal flower
  gfx->fillCircle(cx, cy - s, s, c);
  gfx->fillCircle(cx, cy + s, s, c);
  gfx->fillCircle(cx - s, cy, s, c);
  gfx->fillCircle(cx + s, cy, s, c);
  gfx->fillCircle(cx, cy, s > 1 ? s - 1 : 1, COL_GOLD);
}

void renderAnimatedNoteFrame() {
  if (!displayAvailable) return;
  if (animatedNoteUsesTextBackground || !colorImageBuffer) {
    if (!spriteReady || !canvas) {
      drawCurrentNoteBackground();
      return;
    }
    drawCurrentNoteBackground(false);
  } else if (spriteReady && canvas) {
    memcpy(canvas->getBuffer(), colorImageBuffer, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
  } else {
    tft.drawRGBBitmap(0, 0, colorImageBuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
    return;
  }
  // Overlay animated particles
  static uint8_t phase = 0;
  phase++;
  for (uint8_t i = 0; i < noteOvCount; i++) {
    if (noteAnimType == 1) {
      // Flowing water: drift left with sine wave
      noteOvPtcl[i].x += noteOvPtcl[i].vx;
      int16_t waveY = noteOvPtcl[i].y + (int16_t)(sinf((phase + i * 8) * 0.15f) * 4);
      if (noteOvPtcl[i].x < -10) {
        noteOvPtcl[i].x = SCREEN_WIDTH + random(20);
        noteOvPtcl[i].y = SCREEN_HEIGHT - 50 + random(50);
      }
      int r = noteOvPtcl[i].life;
      if (noteOvPtcl[i].x >= 0 && noteOvPtcl[i].x < SCREEN_WIDTH && waveY >= 0 && waveY < SCREEN_HEIGHT)
        canvas->fillCircle(noteOvPtcl[i].x, waveY, r, noteOvPtcl[i].color);
    } else if (noteAnimType == 2) {
      // Shooting stars: streak from upper-right to lower-left
      noteOvPtcl[i].x += noteOvPtcl[i].vx;
      noteOvPtcl[i].y += noteOvPtcl[i].vy;
      noteOvPtcl[i].life--;
      if (noteOvPtcl[i].life == 0 || noteOvPtcl[i].x < -10 || noteOvPtcl[i].y > SCREEN_HEIGHT) {
        noteOvPtcl[i].x    = (int16_t)(SCREEN_WIDTH + random(80));
        noteOvPtcl[i].y    = (int16_t)random(SCREEN_HEIGHT / 2);
        noteOvPtcl[i].vx   = (int8_t)(-4 - random(4));
        noteOvPtcl[i].vy   = (int8_t)(2 + random(2));
        noteOvPtcl[i].life = (uint8_t)(30 + random(40));
        noteOvPtcl[i].color = (random(3) == 0) ? COL_GOLD : (random(2) == 0) ? COL_PEACH : userFaceColor;
      }
      int16_t sx = noteOvPtcl[i].x, sy = noteOvPtcl[i].y;
      if (sx >= 0 && sx < SCREEN_WIDTH && sy >= 0 && sy < SCREEN_HEIGHT) {
        canvas->drawPixel(sx, sy, noteOvPtcl[i].color);
        // Trail
        int16_t tx = sx - noteOvPtcl[i].vx;
        int16_t ty = sy - noteOvPtcl[i].vy;
        if (tx >= 0 && tx < SCREEN_WIDTH && ty >= 0 && ty < SCREEN_HEIGHT)
          canvas->drawPixel(tx, ty, noteOvPtcl[i].color);
        int16_t t2x = tx - noteOvPtcl[i].vx;
        int16_t t2y = ty - noteOvPtcl[i].vy;
        if (t2x >= 0 && t2x < SCREEN_WIDTH && t2y >= 0 && t2y < SCREEN_HEIGHT)
          canvas->drawPixel(t2x, t2y, noteOvPtcl[i].color);
      }
    } else if (noteAnimType == 3) {
      // Growing flowers: rise from bottom, stop, bloom
      if (noteOvPtcl[i].life > 20) {
        noteOvPtcl[i].y += noteOvPtcl[i].vy;
        noteOvPtcl[i].life--;
      } else if (noteOvPtcl[i].life > 0) {
        noteOvPtcl[i].life--;
      } else {
        // Reset: new flower
        noteOvPtcl[i].x    = (int16_t)(20 + random(SCREEN_WIDTH - 40));
        noteOvPtcl[i].y    = (int16_t)(SCREEN_HEIGHT + random(30));
        noteOvPtcl[i].vy   = (int8_t)(-1);
        noteOvPtcl[i].life = (uint8_t)(40 + random(40));
      }
      int16_t fy = noteOvPtcl[i].y;
      if (fy >= 0 && fy < SCREEN_HEIGHT) {
        int s = (noteOvPtcl[i].life <= 20) ? 3 : 2;
        // Stem
        canvas->drawLine(noteOvPtcl[i].x, fy + s * 2, noteOvPtcl[i].x, SCREEN_HEIGHT, COL_MINT);
        // Flower head
        drawFlowerShape(noteOvPtcl[i].x, fy, s, noteOvPtcl[i].color);
      }
    } else if (noteAnimType == 4) {
      // Fireworks overlay: burst particles that fade and respawn
      noteOvPtcl[i].x += noteOvPtcl[i].vx;
      noteOvPtcl[i].y += noteOvPtcl[i].vy;
      if (noteOvPtcl[i].life % 2 == 0) noteOvPtcl[i].vy = (int8_t)(noteOvPtcl[i].vy + 1);
      noteOvPtcl[i].life--;
      if (noteOvPtcl[i].life == 0) {
        // Respawn burst from random center
        int16_t cx = (int16_t)(40 + random(SCREEN_WIDTH - 80));
        int16_t cy = (int16_t)(20 + random(SCREEN_HEIGHT / 2));
        float a = (float)i * 3.14159f * 2.f / noteOvCount;
        int spd = 1 + (int)random(3);
        noteOvPtcl[i].x    = cx;
        noteOvPtcl[i].y    = cy;
        noteOvPtcl[i].vx   = (int8_t)((float)spd * cosf(a));
        noteOvPtcl[i].vy   = (int8_t)((float)spd * sinf(a));
        noteOvPtcl[i].life = (uint8_t)(20 + random(25));
        noteOvPtcl[i].color = pickBurstColor();
      }
      int16_t px = noteOvPtcl[i].x, py = noteOvPtcl[i].y;
      if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
        int r = noteOvPtcl[i].life > 15 ? 2 : 1;
        canvas->fillCircle(px, py, r, noteOvPtcl[i].color);
      }
    } else if (noteAnimType == 5) {
      // Snowfall overlay: gentle drift down
      noteOvPtcl[i].x += noteOvPtcl[i].vx;
      noteOvPtcl[i].y += noteOvPtcl[i].vy;
      if (noteOvPtcl[i].y >= SCREEN_HEIGHT) { noteOvPtcl[i].y = 0; noteOvPtcl[i].x = (int16_t)random(SCREEN_WIDTH); }
      if (noteOvPtcl[i].x < 0)             noteOvPtcl[i].x = SCREEN_WIDTH - 1;
      if (noteOvPtcl[i].x >= SCREEN_WIDTH)  noteOvPtcl[i].x = 0;
      canvas->fillCircle(noteOvPtcl[i].x, noteOvPtcl[i].y, noteOvPtcl[i].life, noteOvPtcl[i].color);
    } else if (noteAnimType == 6) {
      // Starfield overlay: zoom from center
      const int CX = SCREEN_WIDTH / 2, CY = SCREEN_HEIGHT / 2;
      float z = noteOvPtcl[i].life / 64.f + 0.01f;
      int sx = CX + (int)((noteOvPtcl[i].x - CX) / z);
      int sy = CY + (int)((noteOvPtcl[i].y - CY) / z);
      noteOvPtcl[i].life--;
      if (noteOvPtcl[i].life == 0 || sx < 0 || sx >= SCREEN_WIDTH || sy < 0 || sy >= SCREEN_HEIGHT) {
        noteOvPtcl[i].x    = (int16_t)(CX + random(41) - 20);
        noteOvPtcl[i].y    = (int16_t)(CY + random(41) - 20);
        noteOvPtcl[i].life = (uint8_t)(50 + random(14));
      } else {
        int r = noteOvPtcl[i].life < 20 ? 2 : 1;
        canvas->fillCircle(sx, sy, r, noteOvPtcl[i].color);
      }
    } else if (noteAnimType == 7) {
      // Blooming garden (new growing-flowers): stems rise, blooms ease open with
      // a gentle sway, then the cycle repeats — staggered so it feels alive.
      static const char* GARDEN_TYPES[] = { "rose", "tulip", "daisy", "lily", "sunflower" };
      noteOvPtcl[i].life = (uint8_t)(noteOvPtcl[i].life + (noteOvPtcl[i].vy < 1 ? 1 : noteOvPtcl[i].vy));
      float cycle = noteOvPtcl[i].life / 255.0f;                 // 0..1 looping
      float grow  = fxClampf01(cycle / 0.42f);                  // stem extends
      float bloom = fxClampf01((cycle - 0.42f) / 0.30f);        // flower opens
      int baseY = SCREEN_HEIGHT - 2;
      int topY  = baseY - (int)(easeOutCubic(grow) * (SCREEN_HEIGHT * 0.52f));
      float sway = sinf((phase + i * 24) * 0.05f) * (2.0f + grow * 6.0f);
      int sx = noteOvPtcl[i].x + (int)(sway * grow);
      // Stem (tapered, anti-aliased)
      fxThickLine(noteOvPtcl[i].x, baseY, sx, topY, 2.4f, COL_MINT);
      if (grow > 0.45f) {  // a pair of leaves partway up
        int ly = (baseY + topY) / 2;
        int lx = (noteOvPtcl[i].x + sx) / 2;
        fxEllipseAA(lx - 7, ly, 7, 3, fxDarken(COL_MINT, 30));
        fxEllipseAA(lx + 7, ly, 7, 3, fxDarken(COL_MINT, 30));
      }
      if (bloom > 0.001f) {
        int scale = 2 + (int)(easeOutBack(bloom) * 8.0f);
        if (scale < 2) scale = 2;
        drawFlowerByType(GARDEN_TYPES[noteOvPtcl[i].vx % 5], sx, topY, scale,
                         phase + i * 7, noteOvPtcl[i].color, COL_GOLD);
      }
    } else if (noteAnimType == 8) {
      // Confetti: colorful tumbling rectangles drifting down, wrapping at the bottom.
      noteOvPtcl[i].x += noteOvPtcl[i].vx;
      noteOvPtcl[i].y += noteOvPtcl[i].vy;
      noteOvPtcl[i].life = (uint8_t)(noteOvPtcl[i].life + 3);  // spin
      if (noteOvPtcl[i].y >= SCREEN_HEIGHT) {
        noteOvPtcl[i].y = (int16_t)(-4 - random(20));
        noteOvPtcl[i].x = (int16_t)random(SCREEN_WIDTH);
      }
      if (noteOvPtcl[i].x < 0) noteOvPtcl[i].x = SCREEN_WIDTH - 1;
      if (noteOvPtcl[i].x >= SCREEN_WIDTH) noteOvPtcl[i].x = 0;
      // Tumble: width oscillates with the spin phase for a flipping-paper look.
      float sp = noteOvPtcl[i].life / 40.7f;
      int hw = 1 + (int)(fabsf(cosf(sp)) * 3.0f);
      int x = noteOvPtcl[i].x, y = noteOvPtcl[i].y;
      if (y >= 0 && y < SCREEN_HEIGHT)
        canvas->fillRect(x - hw, y - 2, hw * 2, 4, noteOvPtcl[i].color);
    } else if (noteAnimType == 9) {
      // Bubbles: rise with a gentle horizontal wobble, pop/respawn at the top.
      noteOvPtcl[i].y -= noteOvPtcl[i].vy;
      int wob = (int)(sinf((phase + i * 30) * 0.06f) * 6.0f);
      int x = noteOvPtcl[i].x + wob, y = noteOvPtcl[i].y;
      int r = noteOvPtcl[i].life;
      if (y < -r) {
        noteOvPtcl[i].y = (int16_t)(SCREEN_HEIGHT + random(20));
        noteOvPtcl[i].x = (int16_t)random(SCREEN_WIDTH);
      } else if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        fxRingAA(x, y, r, 1.6f, noteOvPtcl[i].color, 220);          // bubble outline
        fxDiscAA(x - r * 0.35f, y - r * 0.35f, r * 0.22f, 0xFFFF, 200); // highlight
      }
    } else if (noteAnimType == 10) {
      // Falling hearts: drift down with a gentle sway, wrap at the bottom.
      noteOvPtcl[i].y += noteOvPtcl[i].vy;
      int x = noteOvPtcl[i].x + (int)(sinf((phase + i * 24) * 0.05f) * 8.0f);
      int y = noteOvPtcl[i].y;
      if (y > SCREEN_HEIGHT + 8) { noteOvPtcl[i].y = (int16_t)(-8 - random(30)); noteOvPtcl[i].x = (int16_t)random(SCREEN_WIDTH); }
      float s = noteOvPtcl[i].life * 0.6f;
      if (y >= -8 && y < SCREEN_HEIGHT + 8) {
        fxDiscAA(x - s * 0.5f, y - s * 0.35f, s * 0.55f, noteOvPtcl[i].color, 230);
        fxDiscAA(x + s * 0.5f, y - s * 0.35f, s * 0.55f, noteOvPtcl[i].color, 230);
        fxFillTriAA(x - s, y - s * 0.05f, x + s, y - s * 0.05f, x, y + s, noteOvPtcl[i].color, 230);
      }
    } else if (noteAnimType == 11) {
      // Balloons: rise with a slow sway, each trailing a little string.
      noteOvPtcl[i].y -= noteOvPtcl[i].vy;
      int x = noteOvPtcl[i].x + (int)(sinf((phase + i * 20) * 0.04f) * 6.0f);
      int y = noteOvPtcl[i].y, r = noteOvPtcl[i].life;
      if (y < -r * 3) { noteOvPtcl[i].y = (int16_t)(SCREEN_HEIGHT + r + random(40)); noteOvPtcl[i].x = (int16_t)random(SCREEN_WIDTH); }
      else if (x >= 0 && x < SCREEN_WIDTH) {
        fxThickLine(x, y + r, x + 2, y + r + 14, 1.0f, fxDarken(noteOvPtcl[i].color, 40), 160);
        fxDiscRadial(x, y, r, fxLighten(noteOvPtcl[i].color, 80), noteOvPtcl[i].color);
        fxDiscAA(x - r * 0.3f, y - r * 0.3f, r * 0.2f, 0xFFFF, 180);
      }
    } else if (noteAnimType == 12) {
      // Music notes: eighth-notes float up with a wobble.
      noteOvPtcl[i].y -= noteOvPtcl[i].vy;
      int x = noteOvPtcl[i].x + (int)(sinf((phase + i * 30) * 0.06f) * 10.0f);
      int y = noteOvPtcl[i].y;
      if (y < -16) { noteOvPtcl[i].y = (int16_t)(SCREEN_HEIGHT + random(30)); noteOvPtcl[i].x = (int16_t)random(SCREEN_WIDTH); }
      else if (x >= 0 && x < SCREEN_WIDTH && y >= 0) {
        fxDiscAA(x, y, 4.0f, noteOvPtcl[i].color, 230);
        fxThickLine(x + 4, y, x + 4, y - 14, 2.0f, noteOvPtcl[i].color, 230);
        fxThickLine(x + 4, y - 14, x + 9, y - 11, 2.0f, noteOvPtcl[i].color, 230);
      }
    } else if (noteAnimType == 13) {
      // Sparkles: twinkle in place, fading in and out, then respawn elsewhere.
      noteOvPtcl[i].life++;
      float tw = sinf((noteOvPtcl[i].life + i * 16) * 0.12f) * 0.5f + 0.5f;
      if (tw < 0.05f) { noteOvPtcl[i].x = (int16_t)random(SCREEN_WIDTH); noteOvPtcl[i].y = (int16_t)random(SCREEN_HEIGHT); }
      fxStar(noteOvPtcl[i].x, noteOvPtcl[i].y, 2.0f + tw * 3.0f, noteOvPtcl[i].color, tw > 0.7f);
    } else if (noteAnimType == 14) {
      // Autumn leaves: tumble down, the ellipse going edge-on as they spin.
      noteOvPtcl[i].y += noteOvPtcl[i].vy;
      float swA = (phase + i * 24) * 0.05f;
      int x = noteOvPtcl[i].x + (int)(sinf(swA) * 14.0f), y = noteOvPtcl[i].y;
      if (y > SCREEN_HEIGHT + 8) { noteOvPtcl[i].y = (int16_t)(-8 - random(30)); noteOvPtcl[i].x = (int16_t)random(SCREEN_WIDTH); }
      if (y >= -8 && y < SCREEN_HEIGHT + 8)
        fxEllipseAA(x, y, 3.0f + 3.0f * fabsf(cosf(swA)), 3.0f + 1.5f * fabsf(sinf(swA)), noteOvPtcl[i].color);
    } else if (noteAnimType == 15) {
      // Gentle rain: soft thin streaks drifting down.
      noteOvPtcl[i].y += noteOvPtcl[i].vy;
      if (noteOvPtcl[i].y > SCREEN_HEIGHT) { noteOvPtcl[i].y = (int16_t)(random(30) - 30); noteOvPtcl[i].x = (int16_t)random(SCREEN_WIDTH); }
      fxThickLine(noteOvPtcl[i].x, noteOvPtcl[i].y, noteOvPtcl[i].x - 1, noteOvPtcl[i].y + 8, 1.0f, noteOvPtcl[i].color, 150);
    }
  }
  // Drifting petals for the blooming garden (procedural, over the flowers).
  if (noteAnimType == 7) {
    for (int k = 0; k < 6; k++) {
      float pk = fmodf((phase + k * 43) * 0.012f, 1.0f);
      int px = (k * 53 + (int)(sinf((phase + k * 30) * 0.04f) * 18)) % SCREEN_WIDTH;
      int py = (int)(pk * SCREEN_HEIGHT);
      uint16_t pc = (k % 2) ? COL_PINK : COL_ROSE;
      fxDiscAA(px, py, 2.2f, pc, 200);
    }
  }
  pushCanvas();
}

void setParticleMode(const String& name) {
  transientActive = false;
  lastParticleTickMs = millis();
  expressionPhase    = 0;
  gPtclCount = 0;
  noteOvCount = 0;
  memset(gPtcl, 0, sizeof(gPtcl));
  memset(noteOvPtcl, 0, sizeof(noteOvPtcl));
  gfx->fillScreen(COL_BG);
  fwManualOnly = false; // "Send fireworks" enables auto-relaunch
  if (name == "fireworks") {
    currentMode = MODE_FIREWORKS;
    statusText = "Fireworks active";
    initFireworks();
  } else if (name == "heart_rain") {
    currentMode = MODE_HEART_RAIN;
    statusText = "Heart rain active";
    initHeartRain();
  } else if (name == "snowfall") {
    currentMode = MODE_SNOWFALL;
    statusText = "Snowfall active";
    initSnowfall();
  } else if (name == "aurora") {
    currentMode = MODE_AURORA;
    statusText = "Aurora active";
  } else if (name == "fireflies") {
    currentMode = MODE_FIREFLIES;
    statusText = "Fireflies active";
    initFireflies();
  } else if (name == "falling_leaves") {
    currentMode = MODE_FALLING_LEAVES;
    statusText = "Falling leaves active";
    initFallingLeaves();
  } else if (name == "thunderstorm") {
    currentMode = MODE_STORM;
    statusText = "Thunderstorm active";
  } else if (name == "tornado") {
    currentMode = MODE_TORNADO;
    statusText = "Tornado active";
    initTornado();
  } else {
    currentMode = MODE_STARFIELD;
    statusText = "Starfield active";
    initStarfield();
  }
  renderCurrentMode();
  publishStatus();
}

// ─── Countdown mode ───

// ─── Pomodoro focus timer: companion focuses during work, naps on breaks ───
void renderPomodoroFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  long remaining = (long)pomoPhaseDurMs - (long)(millis() - pomoStartMs);
  if (remaining < 0) remaining = 0;
  int secs = (int)(remaining / 1000), mm = secs / 60, ss = secs % 60;
  const bool work = (pomoPhase == 0);

  gfx->setTextSize(2);
  gfx->setTextColor(work ? COL_GOLD : COL_SKYBLUE);
  const char* title = work ? "FOCUS" : "BREAK";
  gfx->setCursor((SCREEN_WIDTH - (int)strlen(title) * 12) / 2, 14);
  gfx->print(title);

  const int fcx = SCREEN_WIDTH / 2, fcy = 92;
  if (work) {                                   // determined / focused
    drawEye(fcx - 40, fcy, 44, 28, 12, 0, 2);
    drawEye(fcx + 40, fcy, 44, 28, 12, 0, 2);
    drawBrow(fcx - 62, fcy - 24, fcx - 18, fcy - 18);
    drawBrow(fcx + 62, fcy - 24, fcx + 18, fcy - 18);
    drawMouthCurve(fcx, fcy + 32, 30, 2.5f, 3.4f, userMouthColor);
  } else {                                      // resting / napping
    drawEye(fcx - 40, fcy + 2, 44, 6, 12, 0, 0);
    drawEye(fcx + 40, fcy + 2, 44, 6, 12, 0, 0);
    drawMouthCurve(fcx, fcy + 30, 22, 2.0f, 3.0f, userMouthColor);
    drawZzz(fcx + 56, fcy - 20, (int)(millis() / 120));
  }

  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
  gfx->setTextSize(4);
  gfx->setTextColor(userFaceColor);
  gfx->setCursor((SCREEN_WIDTH - (int)strlen(buf) * 24) / 2, 150);
  gfx->print(buf);

  const int barW = SCREEN_WIDTH - 40;
  float prog = pomoPhaseDurMs > 0 ? 1.0f - (float)remaining / (float)pomoPhaseDurMs : 0.0f;
  gfx->drawRect(20, 196, barW, 8, userFaceColor);
  gfx->fillRect(20, 196, (int)(barW * prog), 8, work ? COL_GOLD : COL_SKYBLUE);
  for (int i = 0; i < pomoRounds && i < 8; i++) {
    uint16_t c = (i < pomoRoundDone) ? COL_GOLD : fxDarken(userFaceColor, 120);
    fxDiscAA(SCREEN_WIDTH / 2 - (pomoRounds - 1) * 7 + i * 14, 220, 3, c);
  }
  pushCanvas();
}

// Live OTA download progress (drawn over the "Updating" screen without clearing it).
void drawOtaProgress(size_t done, int total) {
  if (!displayAvailable) return;
  const int barX = 30, barW = SCREEN_WIDTH - 60, barY = 150, barH = 16;
  gfx->fillRect(barX, barY, barW, barH, COL_BG);                 // clear bar area
  gfx->drawRect(barX, barY, barW, barH, userFaceColor);
  if (total > 0) {
    float frac = (float)done / (float)total;
    if (frac > 1.0f) frac = 1.0f;
    gfx->fillRect(barX + 2, barY + 2, (int)((barW - 4) * frac), barH - 4, COL_GOLD);
  } else {
    // Unknown size → indeterminate sliding block.
    int bw = barW / 4;
    int pos = (int)((millis() / 6) % (barW - bw));
    gfx->fillRect(barX + 2 + pos, barY + 2, bw, barH - 4, COL_GOLD);
  }
  char b[28];
  if (total > 0) snprintf(b, sizeof(b), "%d%%  -  %lu KB", (int)((float)done / (float)total * 100.0f), (unsigned long)(done / 1024));
  else snprintf(b, sizeof(b), "%lu KB", (unsigned long)(done / 1024));
  gfx->fillRect(0, barY + barH + 4, SCREEN_WIDTH, 12, COL_BG);    // clear text line
  gfx->setTextSize(1);
  gfx->setTextColor(userFaceColor);
  gfx->setCursor((SCREEN_WIDTH - (int)strlen(b) * 6) / 2, barY + barH + 6);
  gfx->print(b);
  pushCanvas();
}

void renderCountdownFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  long elapsed   = (long)((millis() - countdownStartMs) / 1000UL);
  long remaining = countdownSeconds - elapsed;
  if (remaining < 0) remaining = 0;
  long h = remaining / 3600;
  long m = (remaining % 3600) / 60;
  long s = remaining % 60;
  char buf[12];
  if (h > 0) snprintf(buf, sizeof(buf), "%02ld:%02ld:%02ld", h, m, s);
  else       snprintf(buf, sizeof(buf), "%02ld:%02ld", m, s);
  // Label
  gfx->setTextSize(1);
  gfx->setTextColor(userFaceColor);
  gfx->setCursor((SCREEN_WIDTH - 9 * 6) / 2, 55);
  gfx->print("COUNTDOWN");
  // Large digits
  gfx->setTextSize(4);
  gfx->setTextColor(COL_GOLD);
  gfx->setCursor((SCREEN_WIDTH - (int)strlen(buf) * 24) / 2, 75);
  gfx->print(buf);
  // Progress bar
  int barTotal = SCREEN_WIDTH - 40;
  int safeSecs = (countdownSeconds > 0) ? (int)countdownSeconds : 1;
  int barFill  = (int)((long)barTotal * remaining / safeSecs);
  gfx->fillRect(20, 148, barTotal, 8, COL_BG);
  gfx->fillRect(20, 148, barFill,  8, COL_ACCENT);
  gfx->drawRect(20, 148, barTotal, 8, userFaceColor);
  // At zero: trigger chosen celebration
  if (remaining == 0 && !countdownExpired) {
    countdownExpired = true;
    switch (countdownEndAction) {
      case 1:  setParticleMode("heart_rain"); break;
      case 2:  setParticleMode("snowfall");   break;
      case 3:  setParticleMode("starfield");  break;
      default: setParticleMode("fireworks");  break;
    }
  }
  pushCanvas();
}

void setCountdown(long seconds) {
  countdownSeconds   = seconds;
  countdownStartMs   = millis();
  countdownExpired   = false;
  currentMode        = MODE_COUNTDOWN;
  lastParticleTickMs = 0;
}

// ─── NTP clock ───

void syncNtp() {
  configTime(ntpUtcOffsetSeconds, 0, "pool.ntp.org", "time.google.com");
}

// ─── Weather widget (OpenMeteo) ───

static int weatherCodeCategory(int code) {
  if (code == 0)  return 0; // clear sky
  if (code <= 3)  return 1; // partly / overcast
  if (code <= 48) return 1; // fog  → show as cloud
  if (code <= 67) return 2; // drizzle / rain
  if (code <= 77) return 3; // snow
  if (code <= 82) return 2; // rain showers
  if (code <= 86) return 3; // snow showers
  return 4;                 // thunderstorm
}

static uint16_t weatherCategoryColor(int cat) {
  switch (cat) {
    case 0: return COL_GOLD;
    case 1: return userFaceColor;
    case 2: return COL_SKYBLUE;
    case 3: return userFaceColor;
    case 4: return COL_LAVENDER;
    default: return userFaceColor;
  }
}

void drawWeatherIcon(int cx, int cy, int r, int cat) {
  if (cat == 0) {  // Sun
    gfx->fillCircle(cx, cy, r, COL_GOLD);
    for (int i = 0; i < 8; i++) {
      float a = i * 3.14159f / 4.f;
      gfx->drawLine(cx + (int)((r+2)*cosf(a)), cy + (int)((r+2)*sinf(a)),
                   cx + (int)((r+r/2)*cosf(a)), cy + (int)((r+r/2)*sinf(a)), COL_GOLD);
    }
  } else if (cat == 1) {  // Cloud
    gfx->fillCircle(cx - r/3, cy, r*2/3, userFaceColor);
    gfx->fillCircle(cx + r/3, cy, r*2/3, userFaceColor);
    gfx->fillCircle(cx, cy - r/3, r/2, userFaceColor);
    gfx->fillRect(cx - r*2/3, cy, r*4/3, r/2+1, userFaceColor);
  } else if (cat == 2) {  // Rain
    gfx->fillCircle(cx - r/4, cy - r/4, r*2/3, COL_SKYBLUE);
    gfx->fillCircle(cx + r/4, cy - r/4, r*2/3, COL_SKYBLUE);
    gfx->fillRect(cx - r*2/3, cy - r/4, r*4/3, r/3+1, COL_SKYBLUE);
    for (int d = 0; d < 3; d++)
      gfx->drawLine(cx - r/3 + d*r/3, cy + r/4, cx - r/3 + d*r/3 - 2, cy + r, COL_SKYBLUE);
  } else if (cat == 3) {  // Snow
    gfx->fillCircle(cx - r/4, cy - r/4, r*2/3, userFaceColor);
    gfx->fillCircle(cx + r/4, cy - r/4, r*2/3, userFaceColor);
    gfx->fillRect(cx - r*2/3, cy - r/4, r*4/3, r/3+1, userFaceColor);
    for (int d = 0; d < 3; d++)
      gfx->fillCircle(cx - r/3 + d*r/3, cy + r/2, 2, userFaceColor);
  } else {  // Storm
    gfx->fillCircle(cx - r/4, cy - r/4, r*2/3, COL_LAVENDER);
    gfx->fillCircle(cx + r/4, cy - r/4, r*2/3, COL_LAVENDER);
    gfx->fillRect(cx - r*2/3, cy - r/4, r*4/3, r/3+1, COL_LAVENDER);
    gfx->fillTriangle(cx, cy + r/4, cx - r/4, cy + 3*r/4, cx + r/8, cy + r/2, COL_GOLD);
    gfx->fillTriangle(cx + r/8, cy + r/2, cx - r/8, cy + 3*r/4, cx + r/4, cy + 3*r/4, COL_GOLD);
  }
}

void drawWeatherBadge(int x, int y) {
  if (weatherCode < 0) return;
  drawWeatherIcon(x, y, 8, weatherCodeCategory(weatherCode));
}

void renderWeatherFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  if (weatherCode < 0) {
    String detail = (weatherLat == 0.f && weatherLon == 0.f)
        ? "Use the app weather controls"
        : (weatherStatusText.isEmpty() ? "Check Wi-Fi or try again" : weatherStatusText);
    if (detail.length() > 34) {
      detail = detail.substring(0, 34);
    }
    gfx->setTextSize(2);
    gfx->setTextColor(userFaceColor);
    gfx->setCursor(34, 95);
    gfx->print((weatherLat == 0.f && weatherLon == 0.f) ? "Set location first" : "Waiting for weather");
    gfx->setTextSize(1);
    gfx->setTextColor(COL_ACCENT);
    gfx->setCursor(24, 122);
    gfx->print(detail);
    pushCanvas();
    return;
  }
  int cat = weatherCodeCategory(weatherCode);
  drawWeatherIcon(SCREEN_WIDTH / 2, 68, 28, cat);
  char tempBuf[12];
  int tWhole = weatherTempTenths / 10;
  int tFrac  = abs(weatherTempTenths) % 10;
  snprintf(tempBuf, sizeof(tempBuf), "%d.%d F", tWhole, tFrac);
  gfx->setTextSize(3);
  gfx->setTextColor(weatherCategoryColor(cat));
  gfx->setCursor((SCREEN_WIDTH - (int)strlen(tempBuf) * 18) / 2, 115);
  gfx->print(tempBuf);
  const char* labels[] = { "Clear", "Cloudy", "Rain", "Snow", "Storm" };
  gfx->setTextSize(2);
  gfx->setTextColor(userFaceColor);
  gfx->setCursor((SCREEN_WIDTH - (int)strlen(labels[cat]) * 12) / 2, 158);
  gfx->print(labels[cat]);
  pushCanvas();
}

// Read the index-th element of a JSON numeric array ("key":[a,b,c,...]).
static float jsonArrayNumAt(const String& body, const char* key, int index, float fallback) {
  String tag = String("\"") + key + "\":[";
  int k = body.indexOf(tag);
  if (k < 0) return fallback;
  int p = k + tag.length();
  for (int count = 0; count < index; count++) {
    int c = body.indexOf(',', p);
    int e = body.indexOf(']', p);
    if (c < 0 || (e >= 0 && e < c)) return fallback;   // ran past the end
    p = c + 1;
  }
  int c = body.indexOf(',', p);
  int e = body.indexOf(']', p);
  int end = (c < 0) ? e : ((e >= 0 && e < c) ? e : c);
  if (end < 0) return fallback;
  return body.substring(p, end).toFloat();
}

// Read "HH:MM" out of the first element of a daily time array ("key":["...T05:23",...]).
static void jsonDailyTimeFirst(const String& body, const char* key, char* out5) {
  out5[0] = '\0';
  String tag = String("\"") + key + "\":[\"";
  int k = body.indexOf(tag);
  if (k < 0) return;
  int t = body.indexOf('T', k);
  if (t < 0) return;
  // "....T05:23" → take the 5 chars after T
  if ((int)body.length() >= t + 6) {
    for (int i = 0; i < 5; i++) out5[i] = body[t + 1 + i];
    out5[5] = '\0';
  }
}

void fetchWeather() {
  if (weatherLat == 0.f && weatherLon == 0.f) {
    weatherStatusText = "Set location first";
    refreshWeatherDisplayIfVisible();
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    weatherStatusText = "Weather needs Wi-Fi";
    statusText = weatherStatusText;
    publishStatus();
    refreshWeatherDisplayIfVisible();
    return;
  }
  WiFiClientSecure wcl;
  wcl.setInsecure();
  String weatherUrl = "https://api.open-meteo.com/v1/forecast?latitude=";
  weatherUrl += String(weatherLat, 4);
  weatherUrl += "&longitude=";
  weatherUrl += String(weatherLon, 4);
  weatherUrl += "&current=temperature_2m,weather_code&hourly=temperature_2m,weather_code";
  weatherUrl += "&daily=sunrise,sunset&timezone=auto&forecast_days=2";

  HTTPClient whttp;
  whttp.begin(wcl, weatherUrl);
  whttp.setTimeout(10000);
  whttp.setConnectTimeout(6000);
  const int httpCode = whttp.GET();

  if (httpCode < 200 || httpCode >= 300) {
    whttp.end();
    weatherStatusText = httpCode > 0
        ? (String("Weather HTTP ") + String(httpCode))
        : "Weather no response";
    statusText = weatherStatusText;
    lastWeatherFetchMs = millis();
    publishStatus();
    refreshWeatherDisplayIfVisible();
    return;
  }

  const String body = whttp.getString();
  whttp.end();

  if (body.length() > 10) {
    // Open-Meteo includes "current_units":{"temperature_2m":"°C",...} before the actual
    // "current":{"temperature_2m":28.5,...} block. Search from "current": to skip units.
    String searchBody = body;
    int curStart = body.indexOf("\"current\":{");
    if (curStart < 0) curStart = body.indexOf("\"current\": {");
    if (curStart >= 0) searchBody = body.substring(curStart);
    float tempC = extractJsonFloatField(searchBody, "temperature_2m", -999.f);
    int nextWeatherCode = extractJsonIntField(searchBody, "weather_code",
                          extractJsonIntField(searchBody, "weathercode", -1));
    if (tempC > -998.f && nextWeatherCode >= 0) {
      // Convert to Fahrenheit for display
      float tempF = tempC * 9.0f / 5.0f + 32.0f;
      weatherTempTenths = (int)(tempF * 10.f + (tempF >= 0.f ? 0.5f : -0.5f));
      weatherCode = nextWeatherCode;

      // ── Forecast: next few hours + today's sunrise/sunset ──
      forecastCount = 0;
      struct tm ti;
      if (getLocalTime(&ti, 50)) {
        int hourlyBody = body.indexOf("\"hourly\":");
        String hb = (hourlyBody >= 0) ? body.substring(hourlyBody) : body;
        for (int k = 0; k < 4; k++) {
          int idx = ti.tm_hour + 1 + k;            // start at the next hour (local, forecast_days=2 → 0..47)
          if (idx > 47) break;
          float ftc = jsonArrayNumAt(hb, "temperature_2m", idx, -999.f);
          int fcode = (int)jsonArrayNumAt(hb, "weather_code", idx, -1.f);
          if (ftc < -998.f) break;
          forecastTempF[forecastCount] = (int)(ftc * 9.0f / 5.0f + 32.0f + 0.5f);
          forecastCode[forecastCount]  = fcode;
          forecastHourLbl[forecastCount] = idx % 24;
          forecastCount++;
        }
      }
      jsonDailyTimeFirst(body, "sunrise", sunriseStr);
      jsonDailyTimeFirst(body, "sunset", sunsetStr);

      weatherStatusText = "Weather updated";
      statusText = "Weather updated";
      lastWeatherFetchMs = millis();
      publishStatus();
      refreshWeatherDisplayIfVisible();
      return;
    }
  }

  weatherStatusText = body.length() > 0 ? "Weather parse failed" : "Weather empty response";
  statusText = weatherStatusText;
  lastWeatherFetchMs = millis();
  publishStatus();
  refreshWeatherDisplayIfVisible();
}

// ─── Daily greetings + spontaneous reactions ───

void drawStatusBar() {
  if (!displayAvailable) return;
  bool wifiOk  = (WiFi.status() == WL_CONNECTED);
  bool relayOk = (lastRelaySuccessMs > 0 && millis() - lastRelaySuccessMs < 120000UL);
  gfx->setTextSize(1);
  // WiFi dot
  gfx->fillCircle(SCREEN_WIDTH - 28, STATUS_BAR_Y + 3, 5,
                 wifiOk ? 0x07E0 : COL_ROSE);
  // Cloud (Firestore) dot
  gfx->fillCircle(SCREEN_WIDTH - 18, STATUS_BAR_Y + 3, 5,
                 relayOk ? 0x07E0 : (fbConfigured() ? COL_ROSE : COL_FG));
}

void checkTimeGreetings() {
  if (activePetMode == "off") return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastTimeCheckMs < 60000UL) return;
  lastTimeCheckMs = millis();
  struct tm ti;
  if (!getLocalTime(&ti, 10)) return;
  const int hour = ti.tm_hour, yday = ti.tm_yday;
  // Morning greeting (6–9 am)
  if (hour >= 6 && hour < 10 && lastGreetingDay != yday) {
    if (currentMode == MODE_IDLE && !transientActive) {
      lastGreetingDay = yday;
      bondLevel    = clampLevel(bondLevel + 5);
      boredomLevel = clampLevel(boredomLevel - 10);
      persistPetState();
      startTransientExpression("love", 4500, "Good morning!");
    }
  }
  // Evening goodnight (21–23)
  if (hour >= 21 && lastEveningDay != yday) {
    if (currentMode == MODE_IDLE && !transientActive) {
      lastEveningDay = yday;
      setSleepMode();
    }
  }
  // Miss you: idle >25 min with no interaction
  if (currentMode == MODE_IDLE && !transientActive &&
      lastIdleInteractionMs > 0 &&
      millis() - lastIdleInteractionMs > 25UL * 60UL * 1000UL) {
    lastIdleInteractionMs = millis();
    startTransientExpression("sad", 2200, "Miss you!");
  }
}

// ─── Emoji-reactive note messages (Phase 10) ───

void detectEmojiReaction(const String& text) {
  // Scan the note text for known emoji-like tokens and react
  struct { const char* token; const char* expr; unsigned long ms; } rules[] = {
    { "<3",  "love",     3000 },
    { ":)",  "happy",    2200 },
    { ":D",  "laugh",    2200 },
    { ":(",  "sad",      2200 },
    { ":*",  "kiss",     2200 },
    { ";)",  "wink",     2200 },
    { ":o",  "surprised",2200 },
    { ":O",  "surprised",2200 },
    { "xD",  "laugh",    2200 },
    { "<*>", "star_eyes",2500 },
    { "zzz", "sleepy",   3000 },
    { "ZZZ", "sleepy",   3000 },
    { "!",   "excited",  1800 },
  };
  // Real UTF-8 emoji take priority — react to the first recognised one.
  int i = 0;
  while (i < (int)text.length()) {
    if ((uint8_t)text[i] >= 0x80) {
      int len = 1;
      uint32_t cp = utf8DecodeAt(text, i, len);
      String expr = emojiToExpression(cp);
      if (expr.length() > 0) {
        lastNoteEmoji = expr;
        emojiReactionEndsMs = millis() + 2600;
        startTransientExpression(expr, 2600, "Reacting!");
        return;
      }
      i += len;
      continue;
    }
    i++;
  }
  for (size_t r = 0; r < sizeof(rules)/sizeof(rules[0]); r++) {
    if (text.indexOf(rules[r].token) >= 0) {
      lastNoteEmoji = rules[r].expr;
      emojiReactionEndsMs = millis() + rules[r].ms;
      startTransientExpression(rules[r].expr, rules[r].ms, "Reacting!");
      return;
    }
  }
}

// ─── Sleep / goodnight ambient scene (Phase 11) ───

void renderSleepFrame() {
  if (!displayAvailable) return;
  const float t = (float)(expressionPhase % 64) / 63.f;
  // Deep dusk-to-night gradient sky (a lighter band near the horizon fakes the
  // glow cheaply, without a big per-frame radial blend).
  fxScreenGradient(fxRGB(30, 24, 70), fxRGB(8, 6, 26));

  // Twinkling stars (soft AA, gentle brightness pulse).
  static const uint16_t STAR_X[] = { 18, 52, 230, 290, 16, 270, 195, 60, 110, 300, 38, 250 };
  static const uint8_t  STAR_Y[] = { 14, 36,  18,  42, 62,  72,  50, 80,  26,  96, 100, 120 };
  for (int i = 0; i < 12; i++) {
    float tw = 0.5f + 0.5f * sinf(t * 3.14159f * 2.f + i * 0.9f);
    uint16_t c = fxMix(fxRGB(120, 120, 180), 0xFFFF, (uint8_t)(tw * 255));
    fxDiscAA(STAR_X[i], STAR_Y[i], 0.8f + tw * 1.4f, c);
    if (tw > 0.85f) fxStar(STAR_X[i], STAR_Y[i], 3.0f, c);
  }

  // Glowing crescent moon.
  const int MCX = 120, MCY = 88;
  float pulse = 0.9f + sinf(t * 3.14159f * 2.f) * 0.1f;
  float mr = 30.f * pulse;
  fxGlow(MCX, MCY, mr * 1.8f, COL_GOLD, 110);
  fxDiscRadial(MCX, MCY, mr, fxRGB(255, 248, 200), fxRGB(245, 205, 90));
  fxDiscAA(MCX + mr / 2.6f, MCY - mr / 4.f, mr - 3.f, fxRGB(10, 8, 26));   // crescent shadow
  // a couple of craters
  fxDiscAA(MCX - mr * 0.3f, MCY + mr * 0.1f, mr * 0.14f, fxRGB(230, 190, 90), 120);
  fxDiscAA(MCX - mr * 0.1f, MCY + mr * 0.45f, mr * 0.1f, fxRGB(230, 190, 90), 120);

  // Zzz drifting up.
  int zDrift = (int)(t * 40.f);
  gfx->setTextSize(2); gfx->setTextColor(COL_LAVENDER);
  gfx->setCursor(196, 118 - zDrift); gfx->print("z");
  gfx->setTextSize(3); gfx->setCursor(214, 92 - zDrift); gfx->print("Z");

  gfx->setTextSize(1); gfx->setTextColor(fxLighten(COL_LAVENDER, 80));
  gfx->setCursor((SCREEN_WIDTH - 12 * 6) / 2, 168);
  gfx->print("Sweet dreams");
  drawStatusBar();
  pushCanvas();
}

void setSleepMode() {
  gfx->fillScreen(COL_BG);
  currentMode     = MODE_SLEEP;
  expressionPhase = 0;
  lastExpressionTickMs = 0;
  statusText = "Good night";
  renderCurrentMode();
  publishStatus();
}

// ─── Flower drawing helpers (scaled 2×) ───

static String flowerTypeForIndex(int index) {
  static const char* FLOWERS[] = { "rose", "sunflower", "king_protea", "tulip", "daisy", "lily" };
  return String(FLOWERS[index % 6]);
}

void drawFlowerRose(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float spiralT = (float)phase / 63.f;
  float spiralOff = spiralT * 0.4f;
  int outerR   = 15 * scale / 8;
  int outerDist = 30 * scale / 8;
  int midR     = 11 * scale / 8;
  int midDist  = 17 * scale / 8;
  int centerR  = 7  * scale / 8;
  if (outerR    < 3) outerR    = 3;
  if (outerDist < 7) outerDist = 7;
  if (midR      < 3) midR      = 3;
  if (midDist   < 5) midDist   = 5;
  if (centerR   < 2) centerR   = 2;

  uint16_t petalDark = fxDarken(petalColor, 70);
  uint16_t petalLite = fxLighten(petalColor, 90);
  for (int i = 0; i < 5; i++) {
    float a = i * 3.14159f * 2.f / 5.f + spiralOff;
    float px = cx + outerDist * cosf(a);
    float py = cy + outerDist * sinf(a);
    fxDiscRadial(px, py, outerR, petalLite, petalDark);
  }
  for (int i = 0; i < 5; i++) {
    float a = i * 3.14159f * 2.f / 5.f + spiralOff + 0.314f;
    float px = cx + midDist * cosf(a);
    float py = cy + midDist * sinf(a);
    fxDiscRadial(px, py, midR, petalLite, fxDarken(petalColor, 40));
  }
  fxDiscRadial(cx, cy, centerR + 2, fxLighten(centerColor, 70), fxDarken(centerColor, 80));
  fxDiscAA(cx, cy, fmaxf(1.0f, centerR - 2.0f), fxDarken(petalColor, 30));
}

void drawFlowerSunflower(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float t = (float)phase / 63.f;
  int petalLen = 34 * scale / 8;
  int petalW   = 7  * scale / 8;
  int centerR  = 22 * scale / 8;
  if (petalLen < 9)  petalLen = 9;
  if (petalW   < 2)  petalW   = 2;
  if (centerR  < 5)  centerR  = 5;

  uint16_t petalLite = fxLighten(petalColor, 80);
  for (int i = 0; i < 16; i++) {
    float a = i * 3.14159f * 2.f / 16.f + t * 0.2f;
    float tip_x = cx + (centerR + petalLen) * cosf(a);
    float tip_y = cy + (centerR + petalLen) * sinf(a);
    float base_x = cx + centerR * cosf(a);
    float base_y = cy + centerR * sinf(a);
    fxThickLine(base_x, base_y, tip_x, tip_y, petalW, fxDarken(petalColor, 30));
    fxThickLine((base_x + tip_x) * 0.5f, (base_y + tip_y) * 0.5f, tip_x, tip_y, petalW * 0.6f, petalLite);
  }
  fxDiscRadial(cx, cy, centerR, fxLighten(centerColor, 30), fxDarken(centerColor, 110));
  int numSeeds = 21 + (int)(t * 11.f);
  for (int i = 0; i < numSeeds; i++) {
    float angle = i * 2.399f + t * 0.5f;
    float r     = (centerR - 4) * sqrtf((float)i / 34.f);
    fxDiscAA(cx + r * cosf(angle), cy + r * sinf(angle), 1.4f, fxDarken(centerColor, 150));
  }
}

void drawFlowerKingProtea(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float t = (float)phase / 63.f;
  float growT = t < 0.1f ? t * 10.f : 1.0f;
  int bractLen = (int)(38 * scale / 8 * growT);
  int bractW   = 5  * scale / 8;
  int centerR  = 26 * scale / 8;
  if (bractLen < 5)  bractLen = 5;
  if (bractW   < 2)  bractW   = 2;
  if (centerR  < 7)  centerR  = 7;

  uint16_t bractLite = fxLighten(petalColor, 70);
  for (int i = 0; i < 20; i++) {
    float a = i * 3.14159f * 2.f / 20.f + t * 0.1f;
    int base_x = cx + (int)(centerR * cosf(a));
    int base_y = cy + (int)(centerR * sinf(a));
    int tip_x  = cx + (int)((centerR + bractLen) * cosf(a));
    int tip_y  = cy + (int)((centerR + bractLen) * sinf(a));
    float side_a = a + 3.14159f / 2.f;
    int sx = (int)(bractW * cosf(side_a));
    int sy = (int)(bractW * sinf(side_a));
    fxFillTriAA(base_x + sx, base_y + sy, base_x - sx, base_y - sy, tip_x, tip_y, (i & 1) ? petalColor : bractLite);
    fxDiscAA(tip_x, tip_y, bractW * 0.6f, bractLite, 200);
  }
  fxDiscRadial(cx, cy, centerR, fxLighten(centerColor, 60), fxDarken(centerColor, 70));
  fxDiscAA(cx, cy, fmaxf(2.0f, centerR - 5.0f), fxDarken(petalColor, 40));
  fxDiscRadial(cx, cy, fmaxf(1.0f, centerR - 11.0f), fxLighten(centerColor, 40), centerColor);
}

void drawFlowerTulip(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  int bloomW = 18 * scale / 8;
  int bloomH = 24 * scale / 8;
  int sway = (int)(sinf((float)phase / 63.f * 3.14159f * 2.f) * (scale / 2.f));
  uint16_t lite = fxLighten(petalColor, 70), dark = fxDarken(petalColor, 60);
  fxFillTriAA(cx - bloomW, cy + bloomH / 4, cx, cy - bloomH, cx + sway, cy + bloomH / 4, dark);
  fxFillTriAA(cx + bloomW, cy + bloomH / 4, cx, cy - bloomH, cx + sway, cy + bloomH / 4, petalColor);
  fxFillTriAA(cx - bloomW / 2, cy + bloomH / 4, cx, cy - bloomH, cx + sway, cy + bloomH / 4, lite);
  fxThickLine(cx, cy + bloomH / 4, cx, cy - bloomH, 1.6f, fxDarken(petalColor, 90), 150);
  fxDiscAA(cx, cy, fmaxf(2.0f, 5.0f * scale / 8.0f), centerColor);
}

void drawFlowerDaisy(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float petalDist = 16.f * scale / 8.f;
  float petalR = 6.f * scale / 8.f;
  uint16_t lite = fxLighten(petalColor, 60);
  for (int i = 0; i < 10; i++) {
    float a = i * 3.14159f * 2.f / 10.f + (float)phase / 63.f * 0.1f;
    float px = cx + petalDist * cosf(a), py = cy + petalDist * sinf(a);
    fxEllipseAA(px, py, petalR * 1.3f, petalR * 0.8f, petalColor);
    fxDiscAA(px + (cx - px) * 0.2f, py + (cy - py) * 0.2f, petalR * 0.4f, lite, 150);
  }
  fxDiscRadial(cx, cy, fmaxf(2.0f, (float)scale), fxLighten(centerColor, 50), fxDarken(centerColor, 60));
}

void drawFlowerLily(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  int petalLen = 22 * scale / 8;
  int petalW = 6 * scale / 8;
  float spin = (float)phase / 63.f * 0.2f;
  uint16_t lite = fxLighten(petalColor, 70);
  for (int i = 0; i < 6; i++) {
    float a = i * 3.14159f * 2.f / 6.f + spin;
    int tipX = cx + (int)(petalLen * cosf(a));
    int tipY = cy + (int)(petalLen * sinf(a));
    int sideX = (int)(petalW * cosf(a + 3.14159f / 2.f));
    int sideY = (int)(petalW * sinf(a + 3.14159f / 2.f));
    fxFillTriAA(cx - sideX, cy - sideY, cx + sideX, cy + sideY, tipX, tipY, petalColor);
    fxThickLine(cx, cy, tipX, tipY, 1.4f, lite, 160);
    fxDiscAA(tipX, tipY, petalW * 0.5f, petalColor, 220);
  }
  fxDiscRadial(cx, cy, fmaxf(2.0f, 7.f * scale / 8.f), fxLighten(centerColor, 60), fxDarken(centerColor, 50));
}

// Cherry blossom — five soft notched pale-pink petals with golden stamens.
void drawFlowerCherryBlossom(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float spin = (float)phase / 63.f * 0.12f;
  float pr = 9.f * scale / 8.f; if (pr < 3) pr = 3;
  float dist = 10.f * scale / 8.f; if (dist < 4) dist = 4;
  for (int i = 0; i < 5; i++) {
    float a = i * 6.2832f / 5.f + spin - 1.5708f;
    float px = cx + cosf(a) * dist, py = cy + sinf(a) * dist;
    fxDiscRadial(px, py, pr, fxLighten(petalColor, 110), petalColor);
    // little notch at the petal tip
    fxDiscAA(cx + cosf(a) * (dist + pr * 0.7f), cy + sinf(a) * (dist + pr * 0.7f), pr * 0.36f, COL_BG);
  }
  fxDiscRadial(cx, cy, pr * 0.55f, fxLighten(centerColor, 60), centerColor);
  for (int i = 0; i < 6; i++) {
    float a = i * 1.047f + spin;
    fxDiscAA(cx + cosf(a) * pr * 0.5f, cy + sinf(a) * pr * 0.5f, 1.2f, COL_GOLD);
  }
}

// Orchid — broad rounded petals + a contrasting lip, gently rotating.
void drawFlowerOrchid(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float spin = (float)phase / 63.f * 0.1f;
  float pr = 10.f * scale / 8.f; if (pr < 3) pr = 3;
  float dist = 11.f * scale / 8.f; if (dist < 4) dist = 4;
  uint16_t lip = centerColor;
  for (int i = 0; i < 5; i++) {
    float a = i * 6.2832f / 5.f + spin - 1.5708f;
    fxEllipseAA(cx + cosf(a) * dist, cy + sinf(a) * dist, pr, pr * 0.78f, (i == 2) ? lip : petalColor);
  }
  // central lip + column
  fxDiscRadial(cx, cy + pr * 0.2f, pr * 0.5f, fxLighten(lip, 70), fxDarken(lip, 40));
  fxDiscAA(cx, cy, pr * 0.22f, fxLighten(petalColor, 120));
  // freckles on the lip
  fxDiscAA(cx - 2, cy + pr * 0.3f, 1.0f, fxDarken(lip, 120));
  fxDiscAA(cx + 2, cy + pr * 0.4f, 1.0f, fxDarken(lip, 120));
}

// Lotus — pointed petals fanning upward in two layers, gently breathing open.
void drawFlowerLotus(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float open = 0.85f + 0.15f * fxPulse((float)phase / 63.f);
  int petalLen = (int)(20 * scale / 8 * open); if (petalLen < 6) petalLen = 6;
  int petalW = 7 * scale / 8; if (petalW < 2) petalW = 2;
  uint16_t lite = fxLighten(petalColor, 90), dark = fxDarken(petalColor, 50);
  for (int i = 0; i < 7; i++) {
    float a = -1.5708f + (i - 3) * 0.42f;
    float tx = cx + cosf(a) * petalLen, ty = cy + sinf(a) * petalLen;
    float sx = petalW * cosf(a + 1.5708f), sy = petalW * sinf(a + 1.5708f);
    fxFillTriAA(cx - sx, cy - sy, cx + sx, cy + sy, tx, ty, (i & 1) ? petalColor : lite);
  }
  for (int i = 0; i < 5; i++) {
    float a = -1.5708f + (i - 2) * 0.5f;
    float tx = cx + cosf(a) * petalLen * 0.6f, ty = cy + sinf(a) * petalLen * 0.6f;
    float sx = petalW * 0.7f * cosf(a + 1.5708f), sy = petalW * 0.7f * sinf(a + 1.5708f);
    fxFillTriAA(cx - sx, cy - sy, cx + sx, cy + sy, tx, ty, dark);
  }
  fxDiscRadial(cx, cy, fmaxf(2.0f, 4.f * scale / 8.f), fxLighten(centerColor, 60), centerColor);
}

// Lavender — a slender spike of tiny florets that sways most at its tip.
void drawFlowerLavender(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  (void)centerColor;
  float sway = sinf((float)phase / 63.f * 6.2832f) * (scale * 0.15f);
  int n = 7 + scale / 3;
  int spacing = (int)fmaxf(3.0f, 4.0f * scale / 8.f);
  uint16_t lite = fxLighten(petalColor, 80);
  for (int i = 0; i < n; i++) {
    float fy = cy - i * spacing;
    float fx = cx + sway * (i / (float)n);
    float r = fmaxf(1.5f, (3.0f * scale / 8.f) * (1.0f - i / (float)(n + 2)));
    fxDiscRadial(fx - r * 0.8f, fy, r, lite, petalColor);
    fxDiscRadial(fx + r * 0.8f, fy, r, lite, fxDarken(petalColor, 40));
  }
}

// Poppy — four broad round petals and a dark, dotted seed center.
void drawFlowerPoppy(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float spin = (float)phase / 63.f * 0.08f;
  float pr = 11.f * scale / 8.f; if (pr < 3) pr = 3;
  float dist = 9.f * scale / 8.f; if (dist < 3) dist = 3;
  uint16_t dark = fxDarken(petalColor, 50);
  for (int i = 0; i < 4; i++) {
    float a = i * 1.5708f + spin;
    fxDiscRadial(cx + cosf(a) * dist, cy + sinf(a) * dist, pr, fxLighten(petalColor, 70), dark);
  }
  fxDiscRadial(cx, cy, fmaxf(2.0f, 5.f * scale / 8.f), fxDarken(centerColor, 40), fxDarken(centerColor, 120));
  for (int i = 0; i < 8; i++) {
    float a = i * 0.7854f + spin;
    fxDiscAA(cx + cosf(a) * pr * 0.45f, cy + sinf(a) * pr * 0.45f, 1.0f, COL_BG);
  }
}

// Hibiscus — five wide petals with a long protruding pollen-tipped stamen.
void drawFlowerHibiscus(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float spin = (float)phase / 63.f * 0.1f;
  float pr = 12.f * scale / 8.f; if (pr < 4) pr = 4;
  float dist = 10.f * scale / 8.f; if (dist < 4) dist = 4;
  uint16_t lite = fxLighten(petalColor, 70);
  for (int i = 0; i < 5; i++) {
    float a = i * 6.2832f / 5.f + spin - 1.5708f;
    fxEllipseAA(cx + cosf(a) * dist, cy + sinf(a) * dist, pr, pr * 0.85f, (i & 1) ? petalColor : lite);
  }
  fxDiscRadial(cx, cy, pr * 0.5f, fxLighten(centerColor, 40), fxDarken(petalColor, 110));
  float sa = -0.5f + spin;
  float ex = cx + cosf(sa) * pr * 1.7f, ey = cy + sinf(sa) * pr * 1.7f;
  fxThickLine(cx, cy, ex, ey, 1.6f, fxLighten(centerColor, 30));
  fxDiscAA(ex, ey, fmaxf(1.5f, 2.5f * scale / 8.f), COL_GOLD);
}

// Peony — a lush, many-layered ruffled bloom of soft overlapping petals.
void drawFlowerPeony(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float t = (float)phase / 63.f;
  uint16_t lite = fxLighten(petalColor, 90), dark = fxDarken(petalColor, 50);
  for (int ring = 0; ring < 3; ring++) {
    int n = 8 - ring * 2;
    float dist = (14 - ring * 4) * scale / 8.f;
    float pr = (8 - ring) * scale / 8.f; if (pr < 2) pr = 2;
    float off = ring * 0.4f + t * 0.2f;
    for (int i = 0; i < n; i++) {
      float a = i * 6.2832f / n + off;
      fxDiscRadial(cx + cosf(a) * dist, cy + sinf(a) * dist, pr, lite, (ring == 0) ? dark : petalColor);
    }
  }
  fxDiscRadial(cx, cy, fmaxf(2.0f, 5.f * scale / 8.f), fxLighten(centerColor, 70), centerColor);
}

// Daffodil — six bright petals around a raised contrasting trumpet corona.
void drawFlowerDaffodil(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float spin = (float)phase / 63.f * 0.1f;
  int petalLen = 16 * scale / 8; if (petalLen < 5) petalLen = 5;
  int petalW = 6 * scale / 8; if (petalW < 2) petalW = 2;
  uint16_t lite = fxLighten(petalColor, 60);
  for (int i = 0; i < 6; i++) {
    float a = i * 6.2832f / 6.f + spin;
    int tx = cx + (int)(petalLen * cosf(a)), ty = cy + (int)(petalLen * sinf(a));
    int sx = (int)(petalW * cosf(a + 1.5708f)), sy = (int)(petalW * sinf(a + 1.5708f));
    fxFillTriAA(cx - sx, cy - sy, cx + sx, cy + sy, tx, ty, (i & 1) ? petalColor : lite);
  }
  float cr = fmaxf(3.0f, 7.f * scale / 8.f);
  fxDiscRadial(cx, cy, cr, fxLighten(centerColor, 50), fxDarken(centerColor, 70));
  fxRingAA(cx, cy, cr, 1.6f, fxDarken(centerColor, 110));
}

// Dandelion — a fluffy seed-head that lets a few tufts blow off and drift each cycle.
void drawFlowerDandelion(int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  float t = (float)phase / 63.f;
  float pr = 14.f * scale / 8.f; if (pr < 5) pr = 5;
  uint16_t seed = fxLighten(petalColor, 120);
  const int total = 40;
  int gone = (int)(t * 14.0f);
  for (int i = gone; i < total; i++) {
    float a = i * 2.399f;
    float rr = pr * sqrtf((float)i / total);
    float sx = cx + cosf(a) * rr, sy = cy + sinf(a) * rr;
    fxThickLine(cx, cy, sx, sy, 0.8f, fxDarken(seed, 30), 90);
    fxDiscAA(sx, sy, 1.4f, seed, 220);
  }
  fxDiscAA(cx, cy, fmaxf(1.5f, 2.5f * scale / 8.f), fxDarken(centerColor, 40));
  for (int i = 0; i < gone && i < 14; i++) {
    float prog = t + i * 0.04f;
    float sx = cx + (40 + i * 6) * prog;
    float sy = cy - (20 + i * 4) * prog + sinf(prog * 6.28f) * 6.0f;
    if (sx < SCREEN_WIDTH && sy > 0) fxDiscAA(sx, sy, 1.3f, seed, (uint8_t)(200 * (1.0f - t)));
  }
}

void drawFlowerByType(const String& flowerType, int cx, int cy, int scale, int phase, uint16_t petalColor, uint16_t centerColor) {
  if (flowerType == "lotus") {
    drawFlowerLotus(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "lavender") {
    drawFlowerLavender(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "poppy") {
    drawFlowerPoppy(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "hibiscus") {
    drawFlowerHibiscus(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "peony") {
    drawFlowerPeony(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "daffodil") {
    drawFlowerDaffodil(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "dandelion") {
    drawFlowerDandelion(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "sunflower") {
    drawFlowerSunflower(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "king_protea") {
    drawFlowerKingProtea(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "tulip") {
    drawFlowerTulip(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "daisy") {
    drawFlowerDaisy(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "lily") {
    drawFlowerLily(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "cherry_blossom") {
    drawFlowerCherryBlossom(cx, cy, scale, phase, petalColor, centerColor);
  } else if (flowerType == "orchid") {
    drawFlowerOrchid(cx, cy, scale, phase, petalColor, centerColor);
  } else {
    drawFlowerRose(cx, cy, scale, phase, petalColor, centerColor);
  }
}

void renderFlowerFrame() {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);
  int count = flowerCount;
  if (count < 1) count = 1;
  if (count > 7) count = 7;
  int baseScale = 8 * flowerSize / 100;
  if (baseScale < 4) baseScale = 4;
  if (baseScale > 14) baseScale = 14;

  // Continuous "wind" clock → smooth, fluid sway/spin (not tied to the 64-step phase).
  const float wind = (float)millis() * 0.0016f;
  const uint16_t stemDark = fxDarken(flowerStemColor, 35);

  for (int i = 0; i < count; i++) {
    int cx = SCREEN_WIDTH / 2;
    int cy = 100;
    int stemEndX = cx;
    // Per-flower sway as a smooth float (a gentle gust + a faster flutter).
    float sway = sinf(wind + i * 0.7f) * (4.0f + i) + sinf(wind * 2.3f + i) * 1.5f;
    if (flowerArrangement == "bouquet") {
      cx = SCREEN_WIDTH / 2 - (count - 1) * 12 + i * 24;
      cy = 116 - abs(i - count / 2) * 8;
      stemEndX = SCREEN_WIDTH / 2 + (i - count / 2) * 5;
    } else if (flowerArrangement == "row") {
      cx = 42 + i * ((SCREEN_WIDTH - 84) / (count > 1 ? count - 1 : 1));
      cy = 100 + ((i & 1) ? 10 : 0);
    }

    const int stemStartY = cy + scaleByPercent(42, flowerSize);
    const float topX = cx + sway;            // the swaying top of the stem / flower base
    // Anti-aliased tapered stem from the planted base up to the (swaying) flower.
    fxThickLine(stemEndX, SCREEN_HEIGHT - 8, topX, stemStartY, 3.0f, flowerStemColor);
    // A couple of soft leaves partway up.
    float lmx = (stemEndX + topX) * 0.5f, lmy = (SCREEN_HEIGHT - 8 + stemStartY) * 0.5f;
    fxEllipseAA(lmx - 8, lmy, 9, 3.5f, stemDark);
    fxEllipseAA(lmx + 8, lmy + 6, 9, 3.5f, stemDark);

    // Sub-pixel sway of the whole flower head (all petals are AA + offset-aware now).
    gFxOffX = sway; gFxOffY = 0.0f;
    String flowerType = flowerMixed ? flowerTypeForIndex(i) : currentFlower;
    drawFlowerByType(flowerType, cx, cy, baseScale - (count > 1 ? (i % 2) : 0),
                     expressionPhase + i * 5, flowerPetalColor, flowerCenterColor);
    gFxOffX = gFxOffY = 0.0f;
  }
  pushCanvas();
}

void drawNoteWithFlowerAccent(const String& text, int fontSize, int border, const String& icons, const String& flowerType, bool pushToScreen) {
  if (!displayAvailable) return;
  gfx->fillScreen(COL_BG);

  const int flowerCX = 60;
  const int flowerCY = 100;
  const int flowerScale = 6;
  drawFlowerByType(flowerType, flowerCX, flowerCY, flowerScale, expressionPhase, flowerPetalColor, flowerCenterColor);

  // Vertical divider
  gfx->drawLine(125, 6, 125, SCREEN_HEIGHT - 6, userFaceColor);

  // Text on the right side
  const int safeFontSize = 2;
  const int textLeft = 132;
  const int textAreaW = SCREEN_WIDTH - textLeft - 6;
  const int maxChars = textAreaW / (6 * safeFontSize);
  const int lineHeight = (8 * safeFontSize) + 4;
  const int maxLines = (SCREEN_HEIGHT - 16) / lineHeight;

  String lines[maxLines > 20 ? 20 : maxLines];
  int lineCount = 0;
  String remaining = text;
  remaining.trim();
  int usableMaxLines = maxLines > 20 ? 20 : maxLines;
  while (remaining.length() > 0 && lineCount < usableMaxLines) {
    int lineLength = (int)remaining.length() > maxChars ? maxChars : (int)remaining.length();
    if (lineLength < (int)remaining.length()) {
      int split = remaining.lastIndexOf(' ', lineLength);
      if (split > 0) lineLength = split;
    }
    String line = remaining.substring(0, lineLength);
    line.trim();
    if (line.isEmpty()) { line = remaining.substring(0, maxChars); lineLength = line.length(); }
    lines[lineCount++] = line;
    remaining = remaining.substring(lineLength);
    remaining.trim();
  }

  gfx->setTextSize(safeFontSize);
  gfx->setTextColor(currentNoteTextColor);
  gfx->setTextWrap(false);
  int totalH = lineCount * lineHeight;
  int startY = 8 + (SCREEN_HEIGHT - 16 - totalH) / 2;
  if (startY < 8) startY = 8;
  for (int i = 0; i < lineCount; i++) {
    drawLineWithSymbols(lines[i], textLeft, startY + i * lineHeight, safeFontSize);  // emoji/icon aware
  }
  if (pushToScreen) pushCanvas();
}

void drawCurrentNoteBackground(bool pushToScreen) {
  if (currentNoteFlowerAccent.length() > 0) {
    drawNoteWithFlowerAccent(currentNote, currentNoteFontSize, currentNoteBorder, currentNoteIcons, currentNoteFlowerAccent, pushToScreen);
  } else {
    drawWrappedText(currentNote, currentNoteFontSize, currentNoteBorder, currentNoteIcons, pushToScreen);
  }
}

// ─── Preferences and state (identical to mini) ───

void tryStoredPrefs() {
  preferences.begin("desk-cfg", true);
  currentSsid = preferences.getString("ssid", "");
  storedWifiPass = preferences.getString("pass", "");
  deviceToken      = preferences.getString("device_token", "");   // Firestore document id
  fbProjectId      = preferences.getString("fb_project", "");
  fbApiKey         = preferences.getString("fb_apikey", "");
  fbDeviceEmail    = preferences.getString("fb_email", "");
  fbDevicePassword = preferences.getString("fb_pass", "");
  petPersonality = normalizePetPersonality(preferences.getString("pet_personality", petPersonality));
  activePetMode = "off"; // Always start with pet mode off; prevents stale NVS from triggering random expressions
  companionHair = normalizeCompanionHair(preferences.getString("companion_hair", companionHair));
  companionEars = normalizeCompanionEars(preferences.getString("companion_ears", companionEars));
  companionMustache = normalizeCompanionMustache(preferences.getString("companion_mustache", companionMustache));
  companionGlasses = normalizeCompanionGlasses(preferences.getString("companion_glasses", companionGlasses));
  companionHeadwear = normalizeCompanionHeadwear(preferences.getString("companion_headwear", companionHeadwear));
  companionPiercing = normalizeCompanionPiercing(preferences.getString("companion_piercing", companionPiercing));
  companionHeadwearSize = clampAppearancePercent(preferences.getInt("companion_headwear_size", companionHeadwearSize));
  companionHeadwearWidth = clampAppearancePercent(preferences.getInt("companion_headwear_width", companionHeadwearWidth));
  companionHeadwearHeight = clampAppearancePercent(preferences.getInt("companion_headwear_height", companionHeadwearHeight));
  companionHeadwearOffsetX = clampAppearanceOffset(preferences.getInt("companion_headwear_offset_x", companionHeadwearOffsetX));
  companionHeadwearOffsetY = clampAppearanceOffset(preferences.getInt("companion_headwear_offset_y", companionHeadwearOffsetY));
  companionHairSize = clampAppearancePercent(preferences.getInt("companion_hair_size", companionHairSize));
  companionMustacheSize = clampAppearancePercent(preferences.getInt("companion_mustache_size", companionMustacheSize));
  companionHairWidth = clampAppearancePercent(preferences.getInt("companion_hair_width", companionHairWidth));
  companionHairHeight = clampAppearancePercent(preferences.getInt("companion_hair_height", companionHairHeight));
  companionHairThickness = clampAppearancePercent(preferences.getInt("companion_hair_thickness", companionHairThickness));
  companionHairOffsetX = clampAppearanceOffset(preferences.getInt("companion_hair_offset_x", companionHairOffsetX));
  companionHairOffsetY = clampAppearanceOffset(preferences.getInt("companion_hair_offset_y", companionHairOffsetY));
  companionEyeOffsetX = clampAppearanceOffset(preferences.getInt("companion_eye_offset_x", companionEyeOffsetX));
  companionEyeOffsetY = clampAppearanceOffset(preferences.getInt("companion_eye_offset_y", companionEyeOffsetY));
  companionMouthOffsetX = clampAppearanceOffset(preferences.getInt("companion_mouth_offset_x", companionMouthOffsetX));
  companionMouthOffsetY = clampAppearanceOffset(preferences.getInt("companion_mouth_offset_y", companionMouthOffsetY));
  companionScale = (uint16_t)clampCompanionScale(preferences.getInt("companion_scale", companionScale));
  companionOffsetX = clampAppearanceOffset(preferences.getInt("companion_offset_x", companionOffsetX));
  companionOffsetY = clampAppearanceOffset(preferences.getInt("companion_offset_y", companionOffsetY));
  companionMustacheWidth = clampAppearancePercent(preferences.getInt("companion_mustache_width", companionMustacheWidth));
  companionMustacheHeight = clampAppearancePercent(preferences.getInt("companion_mustache_height", companionMustacheHeight));
  companionMustacheThickness = clampAppearancePercent(preferences.getInt("companion_mustache_thickness", companionMustacheThickness));
  companionMustacheOffsetX = clampAppearanceOffset(preferences.getInt("companion_mustache_offset_x", companionMustacheOffsetX));
  companionMustacheOffsetY = clampAppearanceOffset(preferences.getInt("companion_mustache_offset_y", companionMustacheOffsetY));
  bondLevel = clampLevel(preferences.getInt("bond_level", bondLevel));
  energyLevel = clampLevel(preferences.getInt("energy_level", energyLevel));
  boredomLevel = clampLevel(preferences.getInt("boredom_level", boredomLevel));
  const String savedNote = preferences.getString("note_text", "");
  if (!savedNote.isEmpty()) {
    currentNote = savedNote;
    currentNoteFontSize = preferences.getInt("note_fs", 1);
    currentNoteBorder = preferences.getInt("note_border", 0);
    currentNoteIcons = preferences.getString("note_icons", "");
    currentNoteFlowerAccent = preferences.getString("note_flower", "");
    noteQueue[0] = currentNote;
    noteFontSizeQueue[0] = currentNoteFontSize;
    noteQueueCount = 1;
    noteQueueIndex = 0;
    currentMode = MODE_NOTE;
    noteDisplayEndsMs = millis() + 30000;  // auto-return to idle after 30s on boot
  } else {
    statusText = petAmbientStatus();
  }
  weatherLat = preferences.getFloat("wLat", 0.f);
  weatherLon = preferences.getFloat("wLon", 0.f);
  ntpUtcOffsetSeconds = preferences.getInt("ntp_offset", 0);
  preferences.end();
  Serial.printf("[BOOT] Loaded relay url='%s' token='%s' ssid='%s'\n",
                relayUrl.c_str(), deviceToken.c_str(), currentSsid.c_str());
}

// ─── Command handler (identical to mini) ───

uint8_t noteAnimTypeFromName(const String& anim) {
  if (anim == "flowing_water")    return 1;
  if (anim == "shooting_stars")   return 2;
  if (anim == "growing_flowers")  return 3;
  if (anim == "fireworks")        return 4;
  if (anim == "snowfall")         return 5;
  if (anim == "starfield")        return 6;
  if (anim == "blooming_garden" || anim == "growing_flowers_v2") return 7;
  if (anim == "confetti")         return 8;
  if (anim == "bubbles")          return 9;
  if (anim == "falling_hearts")   return 10;
  if (anim == "balloons")         return 11;
  if (anim == "music_notes")      return 12;
  if (anim == "sparkles")         return 13;
  if (anim == "autumn_leaves")    return 14;
  if (anim == "gentle_rain")      return 15;
  return 0;
}

void handleCommandJson(const String& body) {
  const String type = extractJsonStringField(body, "type");
  if (type.isEmpty()) {
    statusText = "Bad command JSON";
    publishStatus();
    return;
  }
  lastIdleInteractionMs = millis(); // track last command for spontaneous greetings

  if (type == "connect_wifi") {
    pendingWifiSsid = extractJsonStringField(body, "ssid");
    pendingWifiPass = extractJsonStringField(body, "password");
    wifiConnectPending = true;
    statusText = "Wi-Fi queued";
    publishStatus();
    return;
  }

  if (type == "scan_wifi") {
    wifiScanPending = true;
    statusText = "Scan queued";
    publishStatus();
    return;
  }

  if (type == "forget_wifi") {
    WiFi.disconnect(true, true);
    WiFi.setAutoReconnect(false);
    currentSsid = "";
    ipAddress = "";
    wifiWasConnected = false;
    availableWifiNetworkCount = 0;
    preferences.begin("desk-cfg", false);
    preferences.remove("ssid");
    preferences.remove("pass");
    preferences.end();
    statusText = "Wi-Fi forgotten";
    publishStatus();
    return;
  }

  if (type == "set_note") {
    // Optional per-note text color (RGB565). -1 / absent → default white.
    int tc = extractJsonIntField(body, "textColor", -1);
    currentNoteTextColor = (tc >= 0) ? (uint16_t)tc : COL_FG;
    // Optional font style (0 regular, 1 bold, 2 outline, 3 shadow).
    currentNoteFontStyle = (uint8_t)extractJsonIntField(body, "fontStyle", 0);
    // Optional text position nudge (so it can sit clear of overlay flowers, etc.).
    currentNoteTextOffX = extractJsonIntField(body, "textOffX", 0);
    currentNoteTextOffY = extractJsonIntField(body, "textOffY", 0);
    // Optional overlay animation folded into the SAME command, so the cloud's
    // single-command slot can't drop the note text (it used to be a 2nd command).
    String anim = extractJsonStringField(body, "animation", "__keep__");
    if (anim != "__keep__") noteAnimType = noteAnimTypeFromName(anim);
    setNote(
      extractJsonStringField(body, "text"),
      extractJsonIntField(body, "fontSize", 1),
      extractJsonIntField(body, "border", 0),
      extractJsonStringField(body, "icons", ""),
      extractJsonStringField(body, "flowerAccent", "")
    );
    return;
  }

  if (type == "set_banner") {
    setBanner(extractJsonStringField(body, "text"), extractJsonIntField(body, "speed", 35));
    return;
  }

  if (type == "set_expression") {
    setExpression(extractJsonStringField(body, "expression", "happy"));
    return;
  }

  if (type == "set_companion_style") {
    companionHair = normalizeCompanionHair(extractJsonStringField(body, "hair", companionHair));
    companionEars = normalizeCompanionEars(extractJsonStringField(body, "ears", companionEars));
    companionMustache = normalizeCompanionMustache(extractJsonStringField(body, "mustache", companionMustache));
    companionGlasses = normalizeCompanionGlasses(extractJsonStringField(body, "glasses", companionGlasses));
    companionHeadwear = normalizeCompanionHeadwear(extractJsonStringField(body, "headwear", companionHeadwear));
    companionPiercing = normalizeCompanionPiercing(extractJsonStringField(body, "piercing", companionPiercing));
    companionHeadwearSize = clampAppearancePercent(extractJsonIntField(body, "headwearSize", companionHeadwearSize));
    companionHeadwearWidth = clampAppearancePercent(extractJsonIntField(body, "headwearWidth", companionHeadwearWidth));
    companionHeadwearHeight = clampAppearancePercent(extractJsonIntField(body, "headwearHeight", companionHeadwearHeight));
    companionHeadwearOffsetX = clampAppearanceOffset(extractJsonIntField(body, "headwearOffsetX", companionHeadwearOffsetX));
    companionHeadwearOffsetY = clampAppearanceOffset(extractJsonIntField(body, "headwearOffsetY", companionHeadwearOffsetY));
    companionHairSize = clampAppearancePercent(extractJsonIntField(body, "hairSize", companionHairSize));
    companionMustacheSize = clampAppearancePercent(extractJsonIntField(body, "mustacheSize", companionMustacheSize));
    companionHairWidth = clampAppearancePercent(extractJsonIntField(body, "hairWidth", companionHairWidth));
    companionHairHeight = clampAppearancePercent(extractJsonIntField(body, "hairHeight", companionHairHeight));
    companionHairThickness = clampAppearancePercent(extractJsonIntField(body, "hairThickness", companionHairThickness));
    companionHairOffsetX = clampAppearanceOffset(extractJsonIntField(body, "hairOffsetX", companionHairOffsetX));
    companionHairOffsetY = clampAppearanceOffset(extractJsonIntField(body, "hairOffsetY", companionHairOffsetY));
    companionEyeOffsetX = clampAppearanceOffset(extractJsonIntField(body, "eyeOffsetX", companionEyeOffsetX));
    companionEyeOffsetY = clampAppearanceOffset(extractJsonIntField(body, "eyeOffsetY", companionEyeOffsetY));
    companionMouthOffsetX = clampAppearanceOffset(extractJsonIntField(body, "mouthOffsetX", companionMouthOffsetX));
    companionMouthOffsetY = clampAppearanceOffset(extractJsonIntField(body, "mouthOffsetY", companionMouthOffsetY));
    companionOffsetX = clampAppearanceOffset(extractJsonIntField(body, "companionOffsetX", companionOffsetX));
    companionOffsetY = clampAppearanceOffset(extractJsonIntField(body, "companionOffsetY", companionOffsetY));
    companionMustacheWidth = clampAppearancePercent(extractJsonIntField(body, "mustacheWidth", companionMustacheWidth));
    companionMustacheHeight = clampAppearancePercent(extractJsonIntField(body, "mustacheHeight", companionMustacheHeight));
    companionMustacheThickness = clampAppearancePercent(extractJsonIntField(body, "mustacheThickness", companionMustacheThickness));
    companionMustacheOffsetX = clampAppearanceOffset(extractJsonIntField(body, "mustacheOffsetX", companionMustacheOffsetX));
    companionMustacheOffsetY = clampAppearanceOffset(extractJsonIntField(body, "mustacheOffsetY", companionMustacheOffsetY));
    persistPetState();
    renderCurrentMode();
    publishStatus();
    return;
  }

  // One combined appearance apply: style + colors + scale + expression in a SINGLE
  // command. The cloud relay keeps only the latest command before the device polls,
  // so applying a saved look used to drop all-but-one of the 4 separate writes; this
  // folds them together. Any omitted field keeps its current value.
  if (type == "set_look") {
    // ── Style / accessories ──
    companionHair = normalizeCompanionHair(extractJsonStringField(body, "hair", companionHair));
    companionEars = normalizeCompanionEars(extractJsonStringField(body, "ears", companionEars));
    companionMustache = normalizeCompanionMustache(extractJsonStringField(body, "mustache", companionMustache));
    companionGlasses = normalizeCompanionGlasses(extractJsonStringField(body, "glasses", companionGlasses));
    companionHeadwear = normalizeCompanionHeadwear(extractJsonStringField(body, "headwear", companionHeadwear));
    companionPiercing = normalizeCompanionPiercing(extractJsonStringField(body, "piercing", companionPiercing));
    companionHeadwearSize = clampAppearancePercent(extractJsonIntField(body, "headwearSize", companionHeadwearSize));
    companionHeadwearWidth = clampAppearancePercent(extractJsonIntField(body, "headwearWidth", companionHeadwearWidth));
    companionHeadwearHeight = clampAppearancePercent(extractJsonIntField(body, "headwearHeight", companionHeadwearHeight));
    companionHeadwearOffsetX = clampAppearanceOffset(extractJsonIntField(body, "headwearOffsetX", companionHeadwearOffsetX));
    companionHeadwearOffsetY = clampAppearanceOffset(extractJsonIntField(body, "headwearOffsetY", companionHeadwearOffsetY));
    companionHairSize = clampAppearancePercent(extractJsonIntField(body, "hairSize", companionHairSize));
    companionMustacheSize = clampAppearancePercent(extractJsonIntField(body, "mustacheSize", companionMustacheSize));
    companionHairWidth = clampAppearancePercent(extractJsonIntField(body, "hairWidth", companionHairWidth));
    companionHairHeight = clampAppearancePercent(extractJsonIntField(body, "hairHeight", companionHairHeight));
    companionHairThickness = clampAppearancePercent(extractJsonIntField(body, "hairThickness", companionHairThickness));
    companionHairOffsetX = clampAppearanceOffset(extractJsonIntField(body, "hairOffsetX", companionHairOffsetX));
    companionHairOffsetY = clampAppearanceOffset(extractJsonIntField(body, "hairOffsetY", companionHairOffsetY));
    companionEyeOffsetX = clampAppearanceOffset(extractJsonIntField(body, "eyeOffsetX", companionEyeOffsetX));
    companionEyeOffsetY = clampAppearanceOffset(extractJsonIntField(body, "eyeOffsetY", companionEyeOffsetY));
    companionMouthOffsetX = clampAppearanceOffset(extractJsonIntField(body, "mouthOffsetX", companionMouthOffsetX));
    companionMouthOffsetY = clampAppearanceOffset(extractJsonIntField(body, "mouthOffsetY", companionMouthOffsetY));
    companionOffsetX = clampAppearanceOffset(extractJsonIntField(body, "companionOffsetX", companionOffsetX));
    companionOffsetY = clampAppearanceOffset(extractJsonIntField(body, "companionOffsetY", companionOffsetY));
    companionMustacheWidth = clampAppearancePercent(extractJsonIntField(body, "mustacheWidth", companionMustacheWidth));
    companionMustacheHeight = clampAppearancePercent(extractJsonIntField(body, "mustacheHeight", companionMustacheHeight));
    companionMustacheThickness = clampAppearancePercent(extractJsonIntField(body, "mustacheThickness", companionMustacheThickness));
    companionMustacheOffsetX = clampAppearanceOffset(extractJsonIntField(body, "mustacheOffsetX", companionMustacheOffsetX));
    companionMustacheOffsetY = clampAppearanceOffset(extractJsonIntField(body, "mustacheOffsetY", companionMustacheOffsetY));
    // ── Scale ──
    companionScale = (uint16_t)clampCompanionScale(extractJsonIntField(body, "scale", companionScale));
    // ── Colors (omit → keep) ──
    int eye    = extractJsonIntField(body, "eyeColor",    -1);
    int face   = extractJsonIntField(body, "faceColor",   -1);
    int accent = extractJsonIntField(body, "accentColor", -1);
    int bodycol= extractJsonIntField(body, "bodyColor",   -1);
    int hair   = extractJsonIntField(body, "hairColor",   -1);
    int hat    = extractJsonIntField(body, "hatColor",    -1);
    int mustache = extractJsonIntField(body, "mustacheColor", -1);
    int mouth  = extractJsonIntField(body, "mouthColor",  -1);
    if (eye    >= 0) userEyeColor    = (uint16_t)eye;
    if (face   >= 0) userFaceColor   = (uint16_t)face;
    if (accent >= 0) userAccentColor = (uint16_t)accent;
    if (bodycol>= 0) userBodyColor   = (uint16_t)bodycol;
    if (hair   >= 0) userHairColor   = (uint16_t)hair;
    if (hat    >= 0) userHatColor    = (uint16_t)hat;
    if (mustache >= 0) userMustacheColor = (uint16_t)mustache;
    if (mouth  >= 0) userMouthColor  = (uint16_t)mouth;
    persistPetState();
    preferences.begin("desk-cfg", false);
    preferences.putUShort("col_eye",    userEyeColor);
    preferences.putUShort("col_face",   userFaceColor);
    preferences.putUShort("col_accent", userAccentColor);
    preferences.putUShort("col_body",   userBodyColor);
    preferences.putUShort("col_hair",   userHairColor);
    preferences.putUShort("col_hat",    userHatColor);
    preferences.putUShort("col_stache", userMustacheColor);
    preferences.putUShort("col_mouth",  userMouthColor);
    preferences.putInt("companion_scale", companionScale);
    preferences.end();
    // ── Expression (optional) — switches to the face and renders ──
    String expr = extractJsonStringField(body, "expression", "");
    if (expr.length() > 0) {
      setExpression(expr);
    } else {
      gfx->fillScreen(COL_BG);
      renderCurrentMode();
    }
    statusText = "Look applied";
    publishStatus();
    return;
  }

  // Offset fields are sent as a separate BLE packet to keep each write under 512 bytes.
  if (type == "set_companion_offsets") {
    companionHeadwearOffsetX = clampAppearanceOffset(extractJsonIntField(body, "headwearOffsetX", companionHeadwearOffsetX));
    companionHeadwearOffsetY = clampAppearanceOffset(extractJsonIntField(body, "headwearOffsetY", companionHeadwearOffsetY));
    companionHairOffsetX     = clampAppearanceOffset(extractJsonIntField(body, "hairOffsetX",     companionHairOffsetX));
    companionHairOffsetY     = clampAppearanceOffset(extractJsonIntField(body, "hairOffsetY",     companionHairOffsetY));
    companionEyeOffsetX      = clampAppearanceOffset(extractJsonIntField(body, "eyeOffsetX",      companionEyeOffsetX));
    companionEyeOffsetY      = clampAppearanceOffset(extractJsonIntField(body, "eyeOffsetY",      companionEyeOffsetY));
    companionMouthOffsetX    = clampAppearanceOffset(extractJsonIntField(body, "mouthOffsetX",    companionMouthOffsetX));
    companionMouthOffsetY    = clampAppearanceOffset(extractJsonIntField(body, "mouthOffsetY",    companionMouthOffsetY));
    companionOffsetX         = clampAppearanceOffset(extractJsonIntField(body, "companionOffsetX", companionOffsetX));
    companionOffsetY         = clampAppearanceOffset(extractJsonIntField(body, "companionOffsetY", companionOffsetY));
    companionMustacheOffsetX = clampAppearanceOffset(extractJsonIntField(body, "mustacheOffsetX", companionMustacheOffsetX));
    companionMustacheOffsetY = clampAppearanceOffset(extractJsonIntField(body, "mustacheOffsetY", companionMustacheOffsetY));
    persistPetState();
    renderCurrentMode();
    publishStatus();
    return;
  }

  if (type == "set_firebase") {
    saveFirebaseSettings(
        extractJsonStringField(body, "projectId"),
        extractJsonStringField(body, "apiKey"),
        extractJsonStringField(body, "deviceEmail"),
        extractJsonStringField(body, "devicePassword"),
        extractJsonStringField(body, "deviceId"));
    return;
  }

  if (type == "set_particle") {
    // Optional tornado prop: a cow or house flung around the funnel.
    String obj = extractJsonStringField(body, "object", "");
    if (obj.length() > 0) {
      if      (obj == "cow")   gTornadoObject = 1;
      else if (obj == "house") gTornadoObject = 2;
      else                     gTornadoObject = 0;
    }
    int trees = extractJsonIntField(body, "trees", -1);
    if (trees >= 0) gTornadoTrees = (trees != 0);
    // Optional firework config folded in so the cloud (latest-wins) applies the
    // whole launch in ONE command instead of 5 (shape/palette/size/stages + mode).
    String shape = extractJsonStringField(body, "shape", "");
    if (shape.length() > 0) {
      if      (shape == "heart")  fireworkShape = 1;
      else if (shape == "star")   fireworkShape = 2;
      else if (shape == "random") fireworkShape = 3;
      else                        fireworkShape = 0;
    }
    String pal = extractJsonStringField(body, "palette", "");
    if (pal.length() > 0) {
      if      (pal == "warm") fireworkPalette = 1;
      else if (pal == "cool") fireworkPalette = 2;
      else if (pal == "mono") fireworkPalette = 3;
      else                    fireworkPalette = 0;
    }
    String sz = extractJsonStringField(body, "size", "");
    if (sz.length() > 0) {
      if      (sz == "small")  fireworkSize = 0;
      else if (sz == "medium") fireworkSize = 1;
      else if (sz == "large")  fireworkSize = 2;
      else if (sz == "xl")     fireworkSize = 3;
      else if (sz == "xxl")    fireworkSize = 4;
      else if (sz == "random") fireworkSize = 5;
      else                     fireworkSize = 1;
    }
    int stages = extractJsonIntField(body, "stages", -1);
    if (stages >= 1) fireworkStages = (uint8_t)(stages > 3 ? 3 : stages);
    setParticleMode(extractJsonStringField(body, "particle", "fireworks"));
    return;
  }

  if (type == "set_firework_shape") {
    String shape = extractJsonStringField(body, "shape", "circle");
    if      (shape == "heart")  fireworkShape = 1;
    else if (shape == "star")   fireworkShape = 2;
    else if (shape == "random") fireworkShape = 3;
    else                        fireworkShape = 0;
    return;
  }

  if (type == "set_firework_size") {
    String sz = extractJsonStringField(body, "size", "medium");
    if      (sz == "small")  fireworkSize = 0;
    else if (sz == "medium") fireworkSize = 1;
    else if (sz == "large")  fireworkSize = 2;
    else if (sz == "xl")     fireworkSize = 3;
    else if (sz == "xxl")    fireworkSize = 4;
    else if (sz == "random") fireworkSize = 5;
    else                     fireworkSize = 1;
    return;
  }

  if (type == "set_note_animation") {
    String anim = extractJsonStringField(body, "animation", "none");
    noteAnimType = noteAnimTypeFromName(anim);   // single source of truth
    const bool hasTextNote = currentNote.length() > 0;
    const bool hasImageNote = colorImageBuffer != nullptr;
    if (noteAnimType > 0 && (hasTextNote || hasImageNote)) {
      transientActive = false;
      animatedNoteUsesTextBackground = hasTextNote;
      currentMode = MODE_ANIMATED_NOTE;
      lastParticleTickMs = millis();
      initNoteOverlay();
      renderCurrentMode();
    } else if (noteAnimType == 0 && currentMode == MODE_ANIMATED_NOTE) {
      currentMode = animatedNoteUsesTextBackground ? MODE_NOTE : MODE_COLOR_IMAGE;
      renderCurrentMode();
    }
    publishStatus();
    return;
  }

  if (type == "set_countdown") {
    int endAct = extractJsonIntField(body, "endAction", 0);
    countdownEndAction = (uint8_t)endAct;
    setCountdown((long)extractJsonIntField(body, "seconds", 60));
    return;
  }

  if (type == "ota_update") {
    String url = extractJsonStringField(body, "url");
    url.trim();
    if (url.length() < 8 || !(url.startsWith("http://") || url.startsWith("https://"))) {
      statusText = "OTA: bad URL";
      publishStatus();
      return;
    }
    if (WiFi.status() != WL_CONNECTED) {
      statusText = "OTA needs Wi-Fi";
      publishStatus();
      return;
    }
    // Show an on-screen notice (the update blocks until it finishes or reboots).
    if (displayAvailable) {
      gfx->fillScreen(COL_BG);
      gfx->setTextSize(2);
      gfx->setTextColor(COL_GOLD);
      gfx->setCursor((SCREEN_WIDTH - 9 * 12) / 2, 96);
      gfx->print("Updating");
      gfx->setTextSize(1);
      gfx->setTextColor(userFaceColor);
      gfx->setCursor((SCREEN_WIDTH - 22 * 6) / 2, 128);
      gfx->print("downloading new firmware");
      pushCanvas();
    }
    statusText = "OTA: downloading";
    publishStatus();

    // Manual download + flash so it works even when the server uses chunked
    // transfer (no Content-Length) or redirects — both make HTTPUpdate bail with
    // "server did not report size". We accept an unknown size and stream it in.
    WiFiClientSecure client;
    client.setInsecure();                                   // hobby-grade: skip cert check
    HTTPClient http;
    http.begin(client, url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // follow GitHub/Storage redirects
    http.setTimeout(20000);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
      statusText = String("OTA HTTP ") + String(code);
      http.end();
      publishStatus();
      renderCurrentMode();
      return;
    }
    int len = http.getSize();                               // -1 when chunked / unknown
    if (!Update.begin(len > 0 ? (size_t)len : UPDATE_SIZE_UNKNOWN)) {
      statusText = String("OTA begin fail: ") + Update.errorString();
      http.end();
      publishStatus();
      renderCurrentMode();
      return;
    }
    // Stream the body in chunks ourselves so we can draw a live progress bar
    // (and so chunked/no-length downloads still work).
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    size_t written = 0;
    unsigned long lastByteMs = millis(), lastDrawMs = 0;
    drawOtaProgress(0, len);
    while (len < 0 || written < (size_t)len) {
      size_t avail = stream->available();
      if (avail) {
        int toRead = avail > sizeof(buf) ? (int)sizeof(buf) : (int)avail;
        int n = stream->readBytes(buf, toRead);
        if (n > 0) {
          if (Update.write(buf, n) != (size_t)n) break;     // flash write error → bail
          written += n;
          lastByteMs = millis();
          if (millis() - lastDrawMs > 120) { lastDrawMs = millis(); drawOtaProgress(written, len); }
        }
      } else {
        if (!http.connected()) break;                       // server closed → finished
        if (millis() - lastByteMs > 15000) break;           // stalled → abort
        delay(2);
      }
    }
    http.end();
    drawOtaProgress(written, len);
    if (!Update.end(true)) {                                // true → finalize with bytes written
      statusText = String("OTA write fail: ") + Update.errorString();
      publishStatus();
      renderCurrentMode();
      return;
    }
    if (!Update.isFinished()) {
      statusText = "OTA incomplete";
      publishStatus();
      renderCurrentMode();
      return;
    }
    statusText = String("OTA done (") + String((unsigned long)written) + " B), rebooting";
    publishStatus();
    delay(600);
    ESP.restart();
    return;
  }

  if (type == "set_pomodoro") {
    int w = extractJsonIntField(body, "work", 25);   if (w < 1) w = 1; if (w > 90) w = 90;
    int b = extractJsonIntField(body, "break", 5);   if (b < 1) b = 1; if (b > 30) b = 30;
    int r = extractJsonIntField(body, "rounds", 4);  if (r < 1) r = 1; if (r > 8) r = 8;
    pomoWorkMin = (uint8_t)w; pomoBreakMin = (uint8_t)b; pomoRounds = (uint8_t)r;
    pomoPhase = 0; pomoRoundDone = 0;
    pomoStartMs = millis();
    pomoPhaseDurMs = (unsigned long)pomoWorkMin * 60000UL;
    lastPomoTickMs = millis();
    transientActive = false;
    currentMode = MODE_POMODORO;
    renderCurrentMode();
    statusText = "Focus session started";
    publishStatus();
    return;
  }

  if (type == "set_expression_speed") {
    int spd = extractJsonIntField(body, "speed", 1);
    if (spd < 1) spd = 1;
    if (spd > 8) spd = 8;
    expressionSpeedMul = (uint8_t)spd;
    return;
  }

  if (type == "set_firework_palette") {
    String pal = extractJsonStringField(body, "palette", "rainbow");
    if      (pal == "warm") fireworkPalette = 1;
    else if (pal == "cool") fireworkPalette = 2;
    else if (pal == "mono") fireworkPalette = 3;
    else                    fireworkPalette = 0;
    return;
  }

  if (type == "set_firework_stages") {
    int s = extractJsonIntField(body, "stages", 1);
    if (s < 1) s = 1;
    if (s > 3) s = 3;
    fireworkStages = (uint8_t)s;
    return;
  }

  if (type == "set_firework_crackle") {
    fireworkCrackle = extractJsonIntField(body, "enabled", 1) != 0;
    return;
  }

  if (type == "fire_firecracker") {
    int dur = extractJsonIntField(body, "duration", 5);
    if (dur < 1) dur = 1;
    if (dur > 30) dur = 30;
    firecrackerDurationMs = (unsigned long)dur * 1000UL;
    firecrackerStartMs = millis();
    firecrackerExploded = false;
    firecrackerWord = extractJsonStringField(body, "word", "BOOM!");
    firecrackerShowCountdown = extractJsonIntField(body, "countdown", 1) != 0;
    int cnt = extractJsonIntField(body, "count", 1);
    if (cnt < 1) cnt = 1;
    if (cnt > 20) cnt = 20;
    firecrackerCount = (uint8_t)cnt;
    int wd = extractJsonIntField(body, "wordDuration", 3);
    if (wd < 1) wd = 1;
    if (wd > 30) wd = 30;
    firecrackerWordDurationMs = (unsigned long)wd * 1000UL;
    firecrackerExplodeMs = 0;
    currentMode = MODE_FIRECRACKER;
    statusText = "Firecracker lit!";
    publishStatus();
    return;
  }

  if (type == "go_home") {
    currentMode = MODE_IDLE;
    renderCurrentMode();
    statusText = "Home screen";
    publishStatus();
    return;
  }

  if (type == "rotate_background") {
    if (idleBackgroundBuffer) {
      rotateBackgroundCW90();
      statusText = "Background rotated";
      renderCurrentMode();  // always re-render so rotation is immediately visible
    } else {
      statusText = "No background loaded";
    }
    publishStatus();
    return;
  }

  if (type == "set_idle_config") {
    idleShowClock   = extractJsonIntField(body, "showClock",   idleShowClock ? 1 : 0) != 0;
    idleShowWeather = extractJsonIntField(body, "showWeather", idleShowWeather ? 1 : 0) != 0;
    idleShowFace    = extractJsonIntField(body, "showFace",    idleShowFace ? 1 : 0) != 0;
    idleShowWifi    = extractJsonIntField(body, "showWifi",    idleShowWifi ? 1 : 0) != 0;
    idleClock12Hour = extractJsonIntField(body, "clock12h", idleClock12Hour ? 1 : 0) != 0;
    idleUseBackgroundImage =
        extractJsonIntField(body, "showBackgroundImage", idleUseBackgroundImage ? 1 : 0) != 0 &&
        idleBackgroundBuffer != nullptr;
    { int ws = extractJsonIntField(body, "weatherSize", weatherTextSize);
      weatherTextSize = (int8_t)(ws < 1 ? 1 : (ws > 3 ? 3 : ws)); }
    { String ie = extractJsonStringField(body, "expression", idleExpression);
      if (ie.length() > 0) idleExpression = ie; }
    { long ann = (long)extractJsonIntField(body, "anniversaryEpoch", (int)anniversaryEpoch);
      if (ann >= 0) anniversaryEpoch = (uint32_t)ann; }
    { int cf = extractJsonIntField(body, "clockFace", idleClockFace);
      idleClockFace = (uint8_t)(cf < 0 ? 0 : (cf > 2 ? 2 : cf)); }
    { int bl = extractJsonIntField(body, "bottomLine", idleBottomLine);
      idleBottomLine = (uint8_t)(bl < 0 ? 0 : (bl > 2 ? 2 : bl)); }
    { int as = extractJsonIntField(body, "affirmationSize", idleAffirmationSize);
      idleAffirmationSize = (uint8_t)(as < 1 ? 1 : (as > 3 ? 3 : as)); }
    idleShowForecast = extractJsonIntField(body, "showForecast", idleShowForecast ? 1 : 0) != 0;
    idleStatusSign = extractJsonStringField(body, "statusSign", idleStatusSign);
    preferences.begin("desk-cfg", false);
    preferences.putBool("idle_clock",   idleShowClock);
    preferences.putBool("idle_weather", idleShowWeather);
    preferences.putBool("idle_face",    idleShowFace);
    preferences.putBool("idle_wifi",    idleShowWifi);
    preferences.putBool("idle_clock_12h", idleClock12Hour);
    preferences.putInt("weather_size",  weatherTextSize);
    preferences.putString("idle_expr",  idleExpression);
    preferences.putUInt("anniversary",  anniversaryEpoch);
    preferences.putUChar("clock_face",  idleClockFace);
    preferences.putUChar("idle_bline",  idleBottomLine);
    preferences.putUChar("idle_asize",  idleAffirmationSize);
    preferences.putBool("idle_fcast",   idleShowForecast);
    preferences.putString("status_sign", idleStatusSign);
    preferences.end();
    if (currentMode == MODE_IDLE) renderCurrentMode();
    statusText = (extractJsonIntField(body, "showBackgroundImage", 0) != 0 && idleBackgroundBuffer == nullptr)
        ? "Pick a home background"
        : "Home screen updated";
    publishStatus();
    return;
  }

  if (type == "set_brightness") {
    int b = extractJsonIntField(body, "brightness", 128);
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    displayBrightness = (uint8_t)b;
    analogWrite(TFT_BL, displayBrightness);
    preferences.begin("desk-cfg", false);
    preferences.putUChar("brightness", displayBrightness);
    preferences.end();
    statusText = "Brightness set";
    publishStatus();
    return;
  }

  if (type == "set_companion_scale") {
    companionScale = (uint16_t)clampCompanionScale(
      extractJsonIntField(body, "scale", companionScale)
    );
    persistPetState();
    if (currentMode == MODE_EXPRESSION || currentMode == MODE_IDLE) renderCurrentMode();
    publishStatus();
    return;
  }

  if (type == "set_draw_color") {
    int c = extractJsonIntField(body, "color", 0xFFFF);
    userFaceColor = (uint16_t)c;
    if (currentMode == MODE_IMAGE) renderCurrentMode();
    return;
  }

  if (type == "fire_rocket") {
    int col = extractJsonIntField(body, "column", -1);
    // Switch to fireworks mode if not already
    if (currentMode != MODE_FIREWORKS) {
      currentMode = MODE_FIREWORKS;
      gPtclCount = 48;
      for (uint8_t i = 0; i < 48; i++) gPtcl[i].life = 0;
    }
    fwManualOnly = true; // Manual rockets: no auto-relaunch
    // Find a free particle slot for a new rocket
    for (uint8_t i = 0; i < 48; i++) {
      if (gPtcl[i].life > 0) continue;
      int16_t xPos;
      if (col >= 0 && col <= 7) {
        xPos = (int16_t)(20 + col * 40);
      } else {
        xPos = (int16_t)(60 + random(200));
      }
      gPtcl[i].x     = xPos;
      gPtcl[i].y     = (int16_t)(SCREEN_HEIGHT - 5);
      gPtcl[i].vx    = (int8_t)(random(3) - 1);
      gPtcl[i].vy    = (int8_t)(-6 - random(3));
      gPtcl[i].life  = 200;
      gPtcl[i].color = COL_GOLD;
      fwRocketCount++;
      break;
    }
    return;
  }

  if (type == "set_timezone") {
    ntpUtcOffsetSeconds = extractJsonIntField(body, "offsetSeconds", 0);
    preferences.begin("desk-cfg", false);
    preferences.putInt("ntp_offset", ntpUtcOffsetSeconds);
    preferences.end();
    if (WiFi.status() == WL_CONNECTED) syncNtp();
    return;
  }

  if (type == "set_location") {
    weatherLat = extractJsonFloatField(body, "lat", weatherLat);
    weatherLon = extractJsonFloatField(body, "lon", weatherLon);
    preferences.begin("desk-cfg", false);
    preferences.putFloat("wLat", weatherLat);
    preferences.putFloat("wLon", weatherLon);
    preferences.end();
    weatherStatusText = "Fetching weather";
    lastWeatherFetchMs = 0;  // force immediate refetch
    if (WiFi.status() == WL_CONNECTED) fetchWeather();
    statusText = "Location saved";
    publishStatus();
    return;
  }

  if (type == "show_weather") {
    if (WiFi.status() == WL_CONNECTED) {
      weatherStatusText = "Fetching weather";
      fetchWeather();
    } else {
      weatherStatusText = "Weather needs Wi-Fi";
    }
    currentMode = MODE_WEATHER;
    renderCurrentMode();
    publishStatus();
    return;
  }

  if (type == "goodnight") {
    setSleepMode();
    return;
  }

  if (type == "set_rotation") {
    static const uint8_t LANDSCAPE_MADCTL[] = { 0x28, 0x68, 0xA8, 0xE8 };
    int rot = extractJsonIntField(body, "rotation", 0) & 3;
    tft.setRotation(1);  // reset geometry to 320×240
    { uint8_t m = LANDSCAPE_MADCTL[rot]; tft.sendCommand(0x36, &m, 1); }
    preferences.begin("desk-cfg", false);
    preferences.putInt("display_rot", rot);
    preferences.end();
    gfx->fillScreen(COL_BG);
    renderCurrentMode();
    statusText = String("MADCTL slot ") + String(rot);
    publishStatus();
    return;
  }

  if (type == "set_colors") {
    int eye    = extractJsonIntField(body, "eyeColor",    -1);
    int face   = extractJsonIntField(body, "faceColor",   -1);
    int accent = extractJsonIntField(body, "accentColor", -1);
    int bodycol= extractJsonIntField(body, "bodyColor",   -1);
    int hair   = extractJsonIntField(body, "hairColor",   -1);
    int hat    = extractJsonIntField(body, "hatColor",    -1);
    int mustache = extractJsonIntField(body, "mustacheColor", -1);
    int mouth  = extractJsonIntField(body, "mouthColor",  -1);
    if (eye    >= 0) userEyeColor    = (uint16_t)eye;
    if (face   >= 0) userFaceColor   = (uint16_t)face;
    if (accent >= 0) userAccentColor = (uint16_t)accent;
    if (bodycol>= 0) userBodyColor   = (uint16_t)bodycol;
    if (hair   >= 0) userHairColor   = (uint16_t)hair;
    if (hat    >= 0) userHatColor    = (uint16_t)hat;
    if (mustache >= 0) userMustacheColor = (uint16_t)mustache;
    if (mouth  >= 0) userMouthColor  = (uint16_t)mouth;
    preferences.begin("desk-cfg", false);
    preferences.putUShort("col_eye",    userEyeColor);
    preferences.putUShort("col_face",   userFaceColor);
    preferences.putUShort("col_accent", userAccentColor);
    preferences.putUShort("col_body",   userBodyColor);
    preferences.putUShort("col_hair",   userHairColor);
    preferences.putUShort("col_hat",    userHatColor);
    preferences.putUShort("col_stache", userMustacheColor);
    preferences.putUShort("col_mouth",  userMouthColor);
    preferences.end();
    gfx->fillScreen(COL_BG);
    renderCurrentMode();
    statusText = "Colors updated";
    publishStatus();
    return;
  }

  if (type == "set_flower") {
    setFlower(
      extractJsonStringField(body, "flower", "rose"),
      extractJsonIntField(body, "count", flowerCount),
      extractJsonIntField(body, "size", flowerSize),
      extractJsonStringField(body, "arrangement", flowerArrangement),
      extractJsonIntField(body, "mixed", flowerMixed ? 1 : 0) != 0,
      extractJsonIntField(body, "petalColor", flowerPetalColor),
      extractJsonIntField(body, "centerColor", flowerCenterColor),
      extractJsonIntField(body, "stemColor", flowerStemColor)
    );
    return;
  }

  if (type == "clear") {
    clearImageBuffer();
    setIdleStatus("Display cleared");
    return;
  }

  if (type == "status") {
    publishStatus();
    return;
  }

  if (type == "begin_image") {
    clearImageBuffer();
    expectedImageBytes = extractJsonIntField(body, "total", 0);
    receivedImageBytes = 0;
    imageTransferActive = expectedImageBytes == sizeof(imageBuffer);
    statusText = imageTransferActive ? "Receiving image" : "Bad image size";
    publishStatus();
    return;
  }

  if (type == "commit_image") {
    if (imageTransferActive && receivedImageBytes == expectedImageBytes) {
      setImageReady();
    } else {
      statusText = "Image incomplete";
      publishStatus();
    }
    imageTransferActive = false;
    return;
  }

  if (type == "begin_color_image") {
    if (!colorImageBuffer) {
      colorImageBuffer = (uint16_t*)ps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 2);
    }
    if (!colorImageBuffer) {
      statusText = "PSRAM alloc failed";
      publishStatus();
      return;
    }
    memset(colorImageBuffer, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
    expectedColorBytes = extractJsonIntField(body, "total", 0);
    receivedColorBytes = 0;
    colorImageTransferForIdleBackground =
      extractJsonIntField(body, "idleBackground", 0) != 0;
    colorImageTransferActive = (expectedColorBytes == (size_t)(SCREEN_WIDTH * SCREEN_HEIGHT * 2));
    statusText = colorImageTransferActive ? "Receiving color image" : "Bad color image size";
    publishStatus();
    return;
  }

  if (type == "color_image_chunk") {
    const String encoded = extractJsonStringField(body, "data");
    if (!appendBase64ColorChunk(encoded)) {
      colorImageTransferActive = false;
      statusText = "Bad color image chunk";
      publishStatus();
    }
    return;
  }

  if (type == "commit_color_image") {
    if (colorImageTransferActive && receivedColorBytes == expectedColorBytes) {
      colorImageTransferActive = false;
      // Byte-swap from big-endian (Flutter) to little-endian (ESP32 native)
      const size_t pixelCount = SCREEN_WIDTH * SCREEN_HEIGHT;
      for (size_t i = 0; i < pixelCount; i++) {
        uint8_t hi = ((uint8_t*)colorImageBuffer)[i * 2];
        uint8_t lo = ((uint8_t*)colorImageBuffer)[i * 2 + 1];
        colorImageBuffer[i] = (uint16_t)(hi << 8) | lo;
      }
      if (colorImageTransferForIdleBackground) {
        idleUseBackgroundImage = saveIdleBackgroundFromColorBuffer();
        statusText = idleUseBackgroundImage ? "Home background ready" : "Home bg alloc failed";
        if (currentMode == MODE_IDLE) {
          renderCurrentMode();
        }
      } else {
        currentMode = MODE_COLOR_IMAGE;
        clearSavedNote();
        statusText = "Color image ready";
        renderCurrentMode();
      }
      publishStatus();
    } else {
      statusText = "Color image incomplete";
      publishStatus();
    }
    colorImageTransferActive = false;
    colorImageTransferForIdleBackground = false;
    return;
  }

  if (type == "set_image") {
    const String encoded = extractJsonStringField(body, "data");
    if (decodeBase64IntoImage(encoded)) {
      setImageReady();
    } else {
      statusText = "Bad image payload";
      publishStatus();
    }
    return;
  }

  if (type == "set_color_image") {
    const String encoded = extractJsonStringField(body, "data");
    if (decodeBase64IntoColorImage(encoded)) {
      if (extractJsonIntField(body, "idleBackground", 0) != 0) {
        idleUseBackgroundImage = saveIdleBackgroundFromColorBuffer();
        statusText = idleUseBackgroundImage ? "Home background ready" : "Home bg alloc failed";
        if (currentMode == MODE_IDLE) {
          renderCurrentMode();
        }
      } else {
        currentMode = MODE_COLOR_IMAGE;
        clearSavedNote();
        statusText = "Color image ready";
        renderCurrentMode();
      }
      publishStatus();
    } else {
      statusText = "Bad color image payload";
      publishStatus();
    }
    return;
  }
}

// ─── State setters (identical to mini) ───

void setIdleStatus(const String& value) {
  statusText = value;
  gfx->fillScreen(COL_BG);
  currentMode = MODE_IDLE;
  transientActive = false;
  activeCareAction = "";
  renderCurrentMode();
  publishStatus();
}

void clearSavedNote() {
  preferences.begin("desk-cfg", false);
  preferences.remove("note_text");
  preferences.end();
}

void setNote(const String& text, int fontSize, int border, const String& icons, const String& flowerAccent) {
  const String boundedText = text.length() > NOTE_TEXT_MAX ? text.substring(0, NOTE_TEXT_MAX) : text;
  // Ignore relay re-delivery of the same note so the idle screen isn't constantly interrupted
  if (boundedText == currentNote && currentMode == MODE_NOTE) return;
  const int boundedFontSize = fontSize < 1 ? 1 : (fontSize > 4 ? 4 : fontSize);
  currentNoteBorder = border < 0 ? 0 : (border > 4 ? 4 : border);
  currentNoteIcons = icons;
  preferences.begin("desk-cfg", false);
  preferences.putString("note_text", boundedText);
  preferences.putInt("note_fs", boundedFontSize);
  preferences.putInt("note_border", currentNoteBorder);
  preferences.putString("note_icons", currentNoteIcons);
  preferences.putString("note_flower", flowerAccent);
  preferences.end();
  bondLevel = clampLevel(bondLevel + 4);
  boredomLevel = clampLevel(boredomLevel - 10);
  persistPetState();
  if (noteQueueCount < NOTE_QUEUE_MAX) {
    noteQueue[noteQueueCount++] = boundedText;
    noteFontSizeQueue[noteQueueCount - 1] = boundedFontSize;
  } else {
    for (int i = 0; i < NOTE_QUEUE_MAX - 1; i++) noteQueue[i] = noteQueue[i + 1];
    for (int i = 0; i < NOTE_QUEUE_MAX - 1; i++) noteFontSizeQueue[i] = noteFontSizeQueue[i + 1];
    noteQueue[NOTE_QUEUE_MAX - 1] = boundedText;
    noteFontSizeQueue[NOTE_QUEUE_MAX - 1] = boundedFontSize;
    if (noteQueueIndex > 0) noteQueueIndex--;
  }
  noteQueueIndex = noteQueueCount - 1;
  currentNote = noteQueue[noteQueueIndex];
  currentNoteFontSize = noteFontSizeQueue[noteQueueIndex];
  currentNoteFlowerAccent = flowerAccent;
  animatedNoteUsesTextBackground = true;
  if (noteAnimType > 0) {
    currentMode = MODE_ANIMATED_NOTE;
    lastParticleTickMs = millis();
    initNoteOverlay();
  } else {
    currentMode = MODE_NOTE;
  }
  statusText = "Showing note";
  renderCurrentMode();
  publishStatus();
  noteDisplayEndsMs = millis() + 45000;  // auto-return to idle after 45s
  // Emoji-reactive expressions only when pet mode is active
  if (activePetMode != "off") {
    startTransientExpression(pickReactionExpression("note"), 2200, "Loved your note");
    detectEmojiReaction(boundedText);
  }
}

void setBanner(const String& text, int speed) {
  transientActive = false;
  currentBanner = text;
  bannerSpeed = speed;
  bannerOffset = SCREEN_WIDTH;
  lastBannerTickMs = millis();
  boredomLevel = clampLevel(boredomLevel - 6);
  energyLevel = clampLevel(energyLevel - 2);
  persistPetState();
  clearSavedNote();
  currentMode = MODE_BANNER;
  statusText = "Banner running";
  renderCurrentMode();
  publishStatus();
}

void setExpression(const String& expression) {
  transientActive = false;
  currentExpression = expression;
  expressionPhase = 0;
  lastExpressionTickMs = 0;
  gfx->fillScreen(COL_BG);
  clearSavedNote();
  currentMode = MODE_EXPRESSION;
  statusText = "Expression active";
  renderCurrentMode();
  publishStatus();
}

void setImageReady() {
  clearSavedNote();
  currentMode = MODE_IMAGE;
  statusText = "Image ready";
  renderCurrentMode();
  publishStatus();
}

void setFlower(const String& flowerType, int count, int size, const String& arrangement, bool mixed, int petalColor, int centerColor, int stemColor) {
  currentFlower = flowerType;
  flowerCount = count < 1 ? 1 : (count > 7 ? 7 : count);
  flowerSize = size < 40 ? 40 : (size > 180 ? 180 : size);
  flowerArrangement = (arrangement == "bouquet" || arrangement == "row") ? arrangement : "single";
  flowerMixed = mixed;
  if (petalColor >= 0) flowerPetalColor = (uint16_t)petalColor;
  if (centerColor >= 0) flowerCenterColor = (uint16_t)centerColor;
  if (stemColor >= 0) flowerStemColor = (uint16_t)stemColor;
  expressionPhase = 0;
  lastExpressionTickMs = 0;
  gfx->fillScreen(COL_BG);
  currentMode = MODE_FLOWER;
  statusText = "Flower animation";
  renderCurrentMode();
  publishStatus();
}

void saveFirebaseSettings(const String& projectId, const String& apiKey,
                          const String& deviceEmail, const String& devicePassword,
                          const String& deviceId) {
  fbProjectId      = projectId;
  fbApiKey         = apiKey;
  fbDeviceEmail    = deviceEmail;
  fbDevicePassword = devicePassword;
  deviceToken      = deviceId;          // Firestore document id
  // Force a fresh sign-in with the new credentials.
  fbIdToken = ""; fbRefreshToken = ""; fbLastCmdId = -1;
  preferences.begin("desk-cfg", false);
  preferences.putString("fb_project", fbProjectId);
  preferences.putString("fb_apikey", fbApiKey);
  preferences.putString("fb_email", fbDeviceEmail);
  preferences.putString("fb_pass", fbDevicePassword);
  preferences.putString("device_token", deviceToken);
  preferences.end();
  statusText = fbConfigured() ? "Cloud configured" : "Cloud cleared";
  publishStatus();
  relayStatusDirty = true;
}

// ─── WiFi (identical to mini) ───

bool connectToWifi(const String& ssid, const String& password) {
  WiFi.disconnect(true, false);
  delay(500);
  storedWifiPass = password;
  preferences.begin("desk-cfg", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", password);
  if (!deviceToken.isEmpty()) preferences.putString("device_token", deviceToken);
  preferences.end();
  configureWifiStaMode();
  delay(100);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), password.c_str());
  markWifiJoinStarted();
  statusText = "Joining Wi-Fi";
  currentSsid = ssid;
  ipAddress = "";
  publishStatus();
  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
    delay(500);
  }
  const int finalStatus = WiFi.status();
  markWifiJoinFinished();
  if (finalStatus == WL_CONNECTED) {
    ipAddress = WiFi.localIP().toString();
    wifiWasConnected = true;
    statusText = "Wi-Fi connected";
    publishStatus();
    pushRelayStatus();
    return true;
  }
  wifiWasConnected = false;
  statusText = String("Wi-Fi failed (") + String(finalStatus) + ")";
  publishStatus();
  return false;
}

bool wifiJoinInProgress() {
  return wifiJoinActive && millis() - lastWifiBeginMs < 30000;
}

void markWifiJoinStarted() {
  wifiJoinActive = true;
  lastWifiBeginMs = millis();
}

void markWifiJoinFinished() {
  wifiJoinActive = false;
}

void scanWifiNetworks() {
  statusText = "Scanning Wi-Fi";
  publishStatus();
  const bool shouldRestoreWifi = !currentSsid.isEmpty() && storedWifiPass.length() > 0;
  if (wifiJoinInProgress()) markWifiJoinFinished();
  if (bootWifiRestorePending) bootWifiRestorePending = false;
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(false, false);
    delay(300);
  } else {
    WiFi.disconnect(false, false);
    delay(150);
  }
  configureWifiStaMode();
  delay(120);
  WiFi.scanDelete();
  availableWifiNetworkCount = 0;
  int foundNetworks = WiFi.scanNetworks(false, false, false, 300);
  if (foundNetworks < 0) {
    Serial.printf("[WIFI] Scan failed with code %d, retrying once.\n", foundNetworks);
    statusText = String("Retrying Wi-Fi scan (") + String(foundNetworks) + ")";
    publishStatus();
    WiFi.scanDelete();
    delay(250);
    configureWifiStaMode();
    delay(120);
    foundNetworks = WiFi.scanNetworks(false, false, false, 300);
  }
  for (int i = 0; i < foundNetworks && availableWifiNetworkCount < 10; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    bool duplicate = false;
    for (int existing = 0; existing < availableWifiNetworkCount; existing++) {
      if (availableWifiNetworks[existing] == ssid) { duplicate = true; break; }
    }
    if (!duplicate) availableWifiNetworks[availableWifiNetworkCount++] = ssid;
  }
  WiFi.scanDelete();
  if (shouldRestoreWifi) {
    configureWifiStaMode();
    delay(100);
    WiFi.setAutoReconnect(true);
    WiFi.begin(currentSsid.c_str(), storedWifiPass.c_str());
    markWifiJoinStarted();
    lastWifiCheckMs = millis();
  }
  if (foundNetworks < 0) {
    statusText = String("Wi-Fi scan failed (") + String(foundNetworks) + ")";
  } else {
    statusText = availableWifiNetworkCount > 0 ? "Wi-Fi list updated" : "No Wi-Fi found";
  }
  publishStatusWithNetworks();
}

// ─── BLE callbacks and setup (identical to mini) ───

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    (void)server;
    publishStatus();
  }
  void onDisconnect(BLEServer* server) override {
    (void)server;
    BLEDevice::startAdvertising();
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    const auto value = characteristic->getValue();
    const String body = bleValueToString(value);
    // Queue for loop() — calling handleCommandJson here (BLE task) crashes
    // when commands like show_weather trigger HTTPS requests (stack overflow).
    if (!body.isEmpty()) {
      pendingBleCommand = body;
      bleCommandPending = true;
    }
  }
};

class ImageCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    const auto value = characteristic->getValue();
    const size_t len = bleValueLength(value);
    const uint8_t* data = bleValueData(value);
    // Route to mono or color image buffer depending on active transfer
    if (imageTransferActive) {
      const size_t remaining = sizeof(imageBuffer) - receivedImageBytes;
      const size_t chunkSize = len < remaining ? len : remaining;
      memcpy(imageBuffer + receivedImageBytes, data, chunkSize);
      receivedImageBytes += chunkSize;
    } else if (colorImageTransferActive && colorImageBuffer) {
      const size_t totalBytes = SCREEN_WIDTH * SCREEN_HEIGHT * 2;
      const size_t remaining = totalBytes - receivedColorBytes;
      const size_t chunkSize = len < remaining ? len : remaining;
      memcpy(((uint8_t*)colorImageBuffer) + receivedColorBytes, data, chunkSize);
      receivedColorBytes += chunkSize;
    }
  }
};

// ═════════════════════════════════════════════════════════════════════════════
//  FIRESTORE RELAY — Firebase Auth (Identity Toolkit REST) + Firestore REST.
//  The device signs in as a dedicated email/password account, refreshes its id
//  token hourly, polls devices/{deviceToken} for the latest command, and PATCHes
//  its status back. All HTTPS runs on the core-0 task so rendering never stalls.
// ═════════════════════════════════════════════════════════════════════════════

// Decode JSON string escapes (\" \\ \/ \n \t \r \b \f \uXXXX) back to raw bytes.
static String jsonUnescape(const String& in) {
  String out; out.reserve(in.length());
  for (int i = 0; i < (int)in.length(); i++) {
    char c = in[i];
    if (c != '\\' || i + 1 >= (int)in.length()) { out += c; continue; }
    char n = in[++i];
    switch (n) {
      case '"': out += '"'; break;
      case '\\': out += '\\'; break;
      case '/': out += '/'; break;
      case 'b': out += '\b'; break;
      case 'f': out += '\f'; break;
      case 'n': out += '\n'; break;
      case 'r': out += '\r'; break;
      case 't': out += '\t'; break;
      case 'u': {
        if (i + 4 < (int)in.length()) {
          char hex[5] = { in[i+1], in[i+2], in[i+3], in[i+4], 0 };
          long cp = strtol(hex, nullptr, 16);
          i += 4;
          if (cp < 0x80) out += (char)cp;
          else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
          else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        }
        break;
      }
      default: out += n; break;
    }
  }
  return out;
}

// Extract a Firestore `"field":{"stringValue":"..."}` value (unescaped).
static String firestoreExtractString(const String& resp, const char* field) {
  String key = String("\"") + field + "\"";
  int k = resp.indexOf(key);
  if (k < 0) return "";
  int sv = resp.indexOf("\"stringValue\"", k);
  if (sv < 0) return "";
  int q = resp.indexOf('"', sv + 13);          // opening quote of the value
  if (q < 0) return "";
  int i = q + 1;
  String raw;
  while (i < (int)resp.length()) {
    char c = resp[i];
    if (c == '\\') { raw += c; if (i + 1 < (int)resp.length()) raw += resp[i + 1]; i += 2; continue; }
    if (c == '"') break;
    raw += c; i++;
  }
  return jsonUnescape(raw);
}

// Extract a Firestore `"field":{"integerValue":"123"}` value.
static long firestoreExtractInt(const String& resp, const char* field, long fallback) {
  String key = String("\"") + field + "\"";
  int k = resp.indexOf(key);
  if (k < 0) return fallback;
  int iv = resp.indexOf("\"integerValue\"", k);
  if (iv < 0) return fallback;
  int q = resp.indexOf('"', iv + 14);
  if (q < 0) return fallback;
  int e = resp.indexOf('"', q + 1);
  if (e < 0) return fallback;
  return resp.substring(q + 1, e).toInt();
}

static String fbFirestoreDocUrl() {
  return "https://firestore.googleapis.com/v1/projects/" + fbProjectId +
         "/databases/(default)/documents/devices/" + deviceToken;
}

// Firestore typed-JSON body for the device's status PATCH.
static String buildFirestoreStatusBody() {
  String s = "{\"fields\":{";
  s += "\"status\":{\"stringValue\":\"" + jsonEscape(statusText) + "\"},";
  s += "\"online\":{\"booleanValue\":true},";
  s += "\"lastSeenMs\":{\"integerValue\":\"" + String(millis()) + "\"},";
  s += "\"lastSeenEpoch\":{\"integerValue\":\"" + String((long)time(nullptr)) + "\"},";
  s += "\"firmware\":{\"stringValue\":\"tft-2.0\"},";
  s += "\"ip\":{\"stringValue\":\"" + jsonEscape(ipAddress) + "\"},";
  s += "\"ssid\":{\"stringValue\":\"" + jsonEscape(currentSsid) + "\"},";
  s += "\"bond\":{\"integerValue\":\"" + String(bondLevel) + "\"},";
  s += "\"energy\":{\"integerValue\":\"" + String(energyLevel) + "\"},";
  s += "\"boredom\":{\"integerValue\":\"" + String(boredomLevel) + "\"},";
  s += "\"mode\":{\"stringValue\":\"" + jsonEscape(String(modeName(currentMode))) + "\"}";
  s += "}}";
  return s;
}

// Generic async HTTPS worker: executes the request described by the s_req* globals.
void relayBgTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // sleep until main loop signals work
    s_relayResCode   = 0;
    s_relayResBody[0] = '\0';

    WiFiClientSecure secureClient;
    secureClient.setInsecure();   // Google certs aren't pinned on-device
    HTTPClient http;

    const String url(s_reqUrl);
    if (http.begin(secureClient, url)) {
      http.setTimeout(6000);        // Firestore/Google TLS is slower than the old relay
      http.setConnectTimeout(4000);
      if (s_reqCtype[0]) http.addHeader("Content-Type", s_reqCtype);
      if (s_reqAuth[0])  http.addHeader("Authorization", String("Bearer ") + s_reqAuth);
      int code;
      if (strcmp(s_reqMethod, "GET") == 0) {
        code = http.GET();
      } else {
        code = http.sendRequest(s_reqMethod, (uint8_t*)s_reqBody, strlen(s_reqBody));
      }
      s_relayResCode = code;
      if (code > 0) strlcpy(s_relayResBody, http.getString().c_str(), sizeof(s_relayResBody));
      else          strlcpy(s_relayResBody, http.errorToString(code).c_str(), sizeof(s_relayResBody));
      http.end();
    } else {
      s_relayResCode = -1;
      strlcpy(s_relayResBody, "begin failed", sizeof(s_relayResBody));
    }
    s_relayResultReady = true;
    s_relayBusy = false;
  }
}

// Manual "push status now" → just flag it; the loop state machine sends it.
void pushRelayStatus() { relayStatusDirty = true; }
// Legacy no-op (polling is driven by the loop state machine).
void pollRelay() {}

void setupBle() {
  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(517);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());
  BLEService* service = bleServer->createService(SERVICE_UUID);
  commandCharacteristic = service->createCharacteristic(COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
  commandCharacteristic->setCallbacks(new CommandCallbacks());
  statusCharacteristic = service->createCharacteristic(STATUS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  statusCharacteristic->addDescriptor(new BLE2902());
  imageCharacteristic = service->createCharacteristic(IMAGE_UUID, BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_WRITE);
  imageCharacteristic->setCallbacks(new ImageCallbacks());
  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

// ─── Display setup (ST7789 via SPI + FT6336U cap touch via I2C) ───

void setupDisplay() {
  // Enable backlight
  Serial.println("[TFT] Enabling backlight...");
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Initialize SPI bus and Adafruit_ST7789 display
  Serial.println("[TFT] Initializing SPI + ST7789...");
  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  SPI.setFrequency(40000000);   // 40 MHz — proven stable on this board (80 caused garbage)
  pTft = new Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
  tft.init(240, 320);
  tft.setSPISpeed(40000000);
  tft.setRotation(1);  // landscape 320×240
  Serial.println("[TFT] tft.init() done.");

  // ── Direct-to-screen test: proves SPI is working ──
  tft.fillScreen(ST77XX_BLUE);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLUE);
  tft.setTextSize(2);
  tft.setCursor(60, 100);
  tft.print("Desk Companion");
  tft.setCursor(90, 130);
  tft.print("Booting...");
  Serial.println("[TFT] Test pattern drawn (blue screen). If you see blue, SPI works.");

  // Override MADCTL for Freenove panel
  static const uint8_t LANDSCAPE_MADCTL[] = { 0x28, 0x68, 0xA8, 0xE8 };
  preferences.begin("desk-cfg", true);
  int displayRot = preferences.getInt("display_rot", 0);
  userEyeColor    = preferences.getUShort("col_eye",    COL_FG);
  userFaceColor   = preferences.getUShort("col_face",   COL_FG);
  userAccentColor = preferences.getUShort("col_accent", COL_ACCENT);
  userBodyColor   = preferences.getUShort("col_body",   COL_ROSE);
  userHairColor   = preferences.getUShort("col_hair",   COL_FG);
  userHatColor    = preferences.getUShort("col_hat",    COL_FG);
  userMustacheColor = preferences.getUShort("col_stache", COL_FG);
  userMouthColor  = preferences.getUShort("col_mouth",  COL_FG);
  idleShowClock   = preferences.getBool("idle_clock",   true);
  idleShowWeather = preferences.getBool("idle_weather", true);
  idleShowFace    = preferences.getBool("idle_face",    true);
  idleShowWifi    = preferences.getBool("idle_wifi",    true);
  idleClock12Hour = preferences.getBool("idle_clock_12h", false);
  idleExpression  = preferences.getString("idle_expr", "neutral");
  anniversaryEpoch = preferences.getUInt("anniversary", 0);
  idleClockFace   = preferences.getUChar("clock_face", 0);
  idleBottomLine = preferences.getUChar("idle_bline", 0);
  idleAffirmationSize = preferences.getUChar("idle_asize", 2);
  idleShowForecast = preferences.getBool("idle_fcast", false);
  idleStatusSign  = preferences.getString("status_sign", "");
  weatherTextSize = (int8_t)preferences.getInt("weather_size", 1);
  displayBrightness = preferences.getUChar("brightness", 128);
  preferences.end();
  displayRot = displayRot & 3;
  { uint8_t m = LANDSCAPE_MADCTL[displayRot]; tft.sendCommand(0x36, &m, 1); }
  Serial.printf("[TFT] MADCTL override: slot %d → 0x%02X\n", displayRot, LANDSCAPE_MADCTL[displayRot]);

  // Apply saved brightness
  analogWrite(TFT_BL, displayBrightness);

  // Create GFXcanvas16 framebuffer for flicker-free double-buffered rendering
  Serial.printf("[TFT] Heap before canvas: %u  PSRAM: %u  Largest block: %u\n",
                ESP.getFreeHeap(), ESP.getFreePsram(),
                heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
  canvas = new GFXcanvas16(SCREEN_WIDTH, SCREEN_HEIGHT);
  if (canvas && canvas->getBuffer()) {
    spriteReady = true;
    gfx = canvas;
    canvas->fillScreen(COL_BG);
    pushCanvas();
    Serial.printf("[TFT] Canvas %dx%d ready. Heap now: %u  PSRAM: %u\n",
                  SCREEN_WIDTH, SCREEN_HEIGHT, ESP.getFreeHeap(), ESP.getFreePsram());
  } else {
    if (canvas) { delete canvas; canvas = nullptr; }
    spriteReady = false;
    gfx = pTft;  // draw directly to screen (may flicker, but works)
    tft.fillScreen(COL_BG);
    Serial.println("[TFT] WARNING: Canvas alloc failed. Using direct drawing (may flicker).");
  }
  displayAvailable = true;
  Serial.println("[TFT] Display ready.");

  // Initialize FT6336U capacitive touch via I2C (bare register access, no library)
  Serial.printf("[TFT] Starting I2C for touch (SDA=%d SCL=%d)...\n", TOUCH_SDA, TOUCH_SCL);
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(400000);

  const unsigned long touchInitStartMs = millis();
  bool foundTouch = false;
  while (millis() - touchInitStartMs < 2000UL) {
    if (ft6336u_init()) {
      foundTouch = true;
      break;
    }
    delay(100);
  }

  if (foundTouch) {
    touchAvailable = true;
    Serial.println("[TFT] FT6336U found at 0x38. Touch ready.");
  } else {
    touchAvailable = false;
    Serial.println("[TFT] WARNING: Touch controller not found after 2s. Continuing without touch.");
  }

  Serial.printf("[TFT] ST7789 initialized with canvas buffer. Free heap: %u  PSRAM: %u\n",
                ESP.getFreeHeap(), ESP.getFreePsram());
}

// ─── Touch handling (FT6336U capacitive, replaces physical buttons) ───

void handleTouch() {
  if (!touchAvailable) return;
  uint8_t touchCount = ft6336u_touched();
  bool isTouched = (touchCount > 0);
  unsigned long now = millis();

  // Menu timeout
  if (currentMode == MODE_MENU && now - menuOpenedMs >= MENU_TIMEOUT_MS) {
    currentMode = menuResumeMode;
    renderCurrentMode();
    return;
  }

  if (isTouched && !touchActive) {
    touchActive  = true;
    holdFired    = false;
    touchStartMs = now;
    TouchPoint p = ft6336u_getPoint();
    // Store raw coordinates for debug
    touchRawX = (int)p.x;
    touchRawY = (int)p.y;
    // Map raw portrait touch coords → landscape screen coords for MADCTL 0x28
    touchStartX  = 319 - (int)p.y;
    touchStartY  = (int)p.x;
  }

  // ─── While still holding: auto-fire 3s hold ───
  if (isTouched && touchActive && !holdFired) {
    if (now - touchStartMs >= BTN_HOLD_MS &&
        currentMode != MODE_CONFIRM_CLEAR && currentMode != MODE_MENU) {
      holdFired = true;
      menuResumeMode = currentMode;
      currentMode = MODE_CONFIRM_CLEAR;
      renderConfirmClear();
      return;
    }
  }

  if (!isTouched && touchActive) {
    touchActive = false;
    if (holdFired) { holdFired = false; return; } // already handled
    unsigned long holdDuration = now - touchStartMs;
    // Debounce: ignore very short touches (ghost/noise)
    if (holdDuration < TOUCH_MIN_HOLD_MS) return;
    lastIdleInteractionMs = now;

    // ─── Confirm Clear mode: tap Yes or No ───
    if (currentMode == MODE_CONFIRM_CLEAR) {
      if (touchStartY >= 100 && touchStartY <= 160) {
        if (touchStartX >= 30 && touchStartX <= 140) {
          // Yes → clear
          noteQueueCount = 0; noteQueueIndex = 0;
          currentMode = MODE_IDLE; currentNote = "";
          preferences.begin("desk-cfg", false);
          preferences.remove("note_text");
          preferences.end();
          setIdleStatus("Ready");
          renderCurrentMode(); publishStatus();
          energyLevel   = clampLevel(energyLevel + 4);
          boredomLevel  = clampLevel(boredomLevel + 6);
          persistPetState();
          startTransientExpression(pickReactionExpression("button_clear"), 1600, "Miss my notes");
        } else if (touchStartX >= 180 && touchStartX <= 290) {
          // No → cancel, go back
          currentMode = menuResumeMode;
          renderCurrentMode();
        }
      }
      return;
    }

    // ─── Menu mode: handle menu taps ───
    if (currentMode == MODE_MENU) {
      Serial.printf("[MENU] tap X=%d Y=%d raw(%d,%d) hold=%lums\n", touchStartX, touchStartY, touchRawX, touchRawY, holdDuration);
      // Store for on-screen debug display
      lastMenuTapX = touchStartX;
      lastMenuTapY = touchStartY;
      menuOpenedMs = now; // reset timeout on interaction

      // Determine which band was tapped using simple vertical division:
      //   Y < 34          → close (title bar / X button)
      //   Y 34..195       → items 0-3 (each ~40px band)
      //   Y >= 196        → arrow row (wider zone to avoid accidental item hits)
      if (touchStartY < 34) {
        // Close menu
        Serial.println("[MENU] close zone");
        lastMenuTapX = -1;
        currentMode = menuResumeMode;
        renderCurrentMode();
        return;
      }
      if (touchStartY >= 196) {
        // Arrow row
        if (touchStartX < SCREEN_WIDTH / 2) {
          Serial.println("[MENU] left arrow");
          menuPage = (menuPage + MENU_PAGES - 1) % MENU_PAGES;
        } else {
          Serial.println("[MENU] right arrow");
          menuPage = (menuPage + 1) % MENU_PAGES;
        }
        renderMenuFrame();
        return;
      }
      // Items: Y 34..195 divided into 4 equal bands of ~40px
      int itemIdx = (touchStartY - 34) / 41;
      if (itemIdx < 0) itemIdx = 0;
      if (itemIdx > 3) itemIdx = 3;
      Serial.printf("[MENU] hit item %d on page %d\n", itemIdx, menuPage);
      executeMenuAction(menuPage, itemIdx);
      // Page 1 quick-actions change display mode → close menu
      if (menuPage == 1) {
        lastMenuTapX = -1;
        renderCurrentMode();
        return;
      }
      // All other pages: stay in menu
      currentMode = MODE_MENU;
      renderMenuFrame();
      return;
    }

    // ─── Normal mode touch handling (short taps only; long-press handled above) ───
    {
      // ─ Short tap
      if (currentMode == MODE_NOTE && noteQueueCount > 1) {
        noteQueueIndex      = (noteQueueIndex + 1) % noteQueueCount;
        currentNote         = noteQueue[noteQueueIndex];
        currentNoteFontSize = noteFontSizeQueue[noteQueueIndex];
        currentMode         = MODE_NOTE;
        statusText          = "Showing note";
        renderCurrentMode(); publishStatus();

      } else if (currentMode == MODE_SLEEP) {
        // Wake from sleep → idle, no reaction expression
        setIdleStatus("Ready");

      } else if (touchStartX >= SCREEN_WIDTH / 2 && touchStartY <= SCREEN_HEIGHT / 2) {
        // Top-right quarter tap → open menu (works in any mode except sleep/note)
        menuResumeMode = currentMode;
        menuPage = 0;
        menuOpenedMs = now;
        currentMode = MODE_MENU;
        renderMenuFrame();
        return;
      }
      // All other taps: do nothing — content stays on screen
    } // end short-tap block
  }
}

// ─── Main ───

void setup() {
  Serial.begin(115200);
  delay(2000);  // extra time to open serial monitor
  Serial.println("\n\n=== Desk Companion TFT 2.8\" boot ===");
  Serial.printf("[BOOT] Reset reason: %d\n", static_cast<int>(esp_reset_reason()));

  // Try to enable PSRAM (required for sprite buffer)
  if (!psramFound()) {
    psramInit();
    Serial.printf("[BOOT] psramInit() called. PSRAM now: %u\n", ESP.getFreePsram());
  }
  Serial.printf("[BOOT] Free heap: %u  PSRAM: %u\n", ESP.getFreeHeap(), ESP.getFreePsram());

  WiFi.persistent(false);
  configureWifiStaMode();
  Serial.println("[BOOT] Wi-Fi STA mode configured.");

  Serial.println("[BOOT] setupDisplay...");
  setupDisplay();
  if (!displayAvailable || gfx == nullptr) {
    Serial.println("[BOOT] FATAL: Display init failed. Halting.");
    while (true) delay(1000);
  }

  gfx->fillScreen(COL_BG);
  gfx->setCursor(10, 10);
  gfx->setTextColor(COL_FG);
  gfx->setTextSize(2);
  gfx->println("Booting...");
  pushCanvas();

  Serial.println("[BOOT] clearImageBuffer...");
  clearImageBuffer();

  Serial.println("[BOOT] tryStoredPrefs...");
  tryStoredPrefs();
  Serial.println("[BOOT] Stored prefs applied.");

  Serial.println("[BOOT] setupBle...");
  setupBle();
  Serial.println("[BOOT] BLE server setup complete.");

  if (!currentSsid.isEmpty() && storedWifiPass.length() > 0) {
    bootWifiRestorePending = true;
    statusText = "Wi-Fi queued";
    Serial.println("[BOOT] Deferring Wi-Fi reconnect until boot settles.");
  }

  bootCompletedAtMs = millis();
  lastWifiCheckMs = millis();

  // Relay HTTP task on core 0 — keeps blocking HTTP calls off the render loop.
  Serial.println("[BOOT] Starting relay background task...");
  BaseType_t relayTaskOk = xTaskCreatePinnedToCore(
    relayBgTask, "relay_bg", 12288, nullptr, 1, &s_relayTaskHandle, 0
  );
  if (relayTaskOk != pdPASS) {
    Serial.println("[BOOT] ERROR: Failed to start relay background task.");
  } else {
    Serial.println("[BOOT] Relay background task started.");
  }

  renderCurrentMode();
  publishStatus();
  Serial.println("[BOOT] Setup complete. Entering main loop.");
}

void loop() {
  // Drain BLE command queue (deferred from BLE task to avoid stack overflow)
  if (bleCommandPending) {
    bleCommandPending = false;
    String cmd = pendingBleCommand;
    pendingBleCommand = "";
    handleCommandJson(cmd);
  }

  // Auto-expire note display and return to idle
  if (currentMode == MODE_NOTE && noteDisplayEndsMs > 0 && millis() >= noteDisplayEndsMs) {
    noteDisplayEndsMs = 0;
    currentMode = MODE_IDLE;
    statusText = petAmbientStatus();
    renderCurrentMode();
    publishStatus();
  }

  if (millis() - lastDecorTickMs >= 50) {
    lastDecorTickMs = millis();
    idleOrbit = (idleOrbit + 1) % 16;
    if (currentMode == MODE_IDLE) {
      updateIdleBehavior();   // advance the autonomous "alive" behavior state
      renderIdle();
    }
  }

  if (currentMode == MODE_BANNER) {
    const unsigned long now = millis();
    const unsigned long interval = bannerSpeed <= 0 ? 40 : 1000UL / bannerSpeed;
    if (now - lastBannerTickMs >= interval) {
      lastBannerTickMs = now;
      bannerOffset -= 3;  // 3px per tick for wider screen
      const int textWidth = currentBanner.length() * 24;  // textSize 4 → 24px/char
      if (bannerOffset < -textWidth) {
        bannerOffset = SCREEN_WIDTH;
      }
      renderBannerFrame();
    }
  }

  if (currentMode == MODE_EXPRESSION) {
    const unsigned long now = millis();
    // Pace at ~30fps. Pushing faster than the panel can accept a full 320x240
    // frame causes tearing/contention (drifting gray bars), so give each blit room.
    if (now - lastExpressionTickMs >= 33) {
      lastExpressionTickMs = now;
      for (uint8_t s = 0; s < expressionSpeedMul; s++)
        expressionPhase = (expressionPhase + 1) % 64;
      renderExpressionFrame();
    }
  }

  if (currentMode == MODE_FLOWER) {
    const unsigned long now = millis();
    if (now - lastExpressionTickMs >= 33) {
      lastExpressionTickMs = now;
      for (uint8_t s = 0; s < expressionSpeedMul; s++)
        expressionPhase = (expressionPhase + 1) % 64;
      renderFlowerFrame();
    }
  }

  if (currentMode == MODE_FIREWORKS || currentMode == MODE_HEART_RAIN ||
      currentMode == MODE_SNOWFALL  || currentMode == MODE_STARFIELD ||
      currentMode == MODE_AURORA || currentMode == MODE_FIREFLIES ||
      currentMode == MODE_FALLING_LEAVES || currentMode == MODE_STORM ||
      currentMode == MODE_TORNADO ||
      currentMode == MODE_ANIMATED_NOTE || currentMode == MODE_FIRECRACKER) {
    const unsigned long now = millis();
    // Animated notes repaint the whole text background each frame, so 30fps is
    // plenty and keeps them smooth; pure particle scenes get the faster tick.
    const unsigned long tick = (currentMode == MODE_ANIMATED_NOTE) ? 33UL : 20UL;
    if (now - lastParticleTickMs >= tick) {
      lastParticleTickMs = now;
      renderCurrentMode();
    }
  }

  if (currentMode == MODE_COUNTDOWN) {
    const unsigned long now = millis();
    if (now - lastParticleTickMs >= 500) {
      lastParticleTickMs = now;
      renderCountdownFrame();
    }
  }

  if (currentMode == MODE_POMODORO) {
    const unsigned long now = millis();
    if (now - pomoStartMs >= pomoPhaseDurMs) {
      if (pomoPhase == 0) {                       // focus → break
        pomoPhase = 1;
        pomoPhaseDurMs = (unsigned long)pomoBreakMin * 60000UL;
        pomoStartMs = now;
      } else {                                    // break → next round or finish
        pomoRoundDone++;
        if (pomoRoundDone >= pomoRounds) {
          setParticleMode("fireworks");           // celebrate the finished session
        } else {
          pomoPhase = 0;
          pomoPhaseDurMs = (unsigned long)pomoWorkMin * 60000UL;
          pomoStartMs = now;
        }
      }
    }
    if (currentMode == MODE_POMODORO && now - lastPomoTickMs >= 250) {
      lastPomoTickMs = now;
      renderPomodoroFrame();
    }
  }

  // Sleep mode tick
  if (currentMode == MODE_SLEEP) {
    const unsigned long now = millis();
    if (now - lastExpressionTickMs >= 33) {
      lastExpressionTickMs = now;
      expressionPhase = (expressionPhase + 1) % 64;
      renderSleepFrame();
    }
  }

  // Keep network maintenance from blocking render loops while an animation is active.
  const bool inAnimatedMode = isAnyAnimatedMode();
  const unsigned long weatherRefreshMs = weatherCode >= 0 ? 600000UL : 60000UL;

  // Weather auto-fetch every 10 min when WiFi+location set (skip while an animation is running)
  if (!inAnimatedMode &&
      WiFi.status() == WL_CONNECTED &&
      (weatherLat != 0.f || weatherLon != 0.f) &&
      (lastWeatherFetchMs == 0 || millis() - lastWeatherFetchMs >= weatherRefreshMs)) {
    fetchWeather();
  }

  if (bootWifiRestorePending &&
      !currentSsid.isEmpty() &&
      storedWifiPass.length() > 0 &&
      !wifiJoinInProgress() &&
      WiFi.status() != WL_CONNECTED &&
      millis() - bootCompletedAtMs >= BOOT_WIFI_DELAY_MS) {
    bootWifiRestorePending = false;
    statusText = "Starting Wi-Fi";
    publishStatus();
    Serial.println("[BOOT] Starting deferred Wi-Fi reconnect.");
    configureWifiStaMode();
    delay(100);
    WiFi.setAutoReconnect(true);
    WiFi.begin(currentSsid.c_str(), storedWifiPass.c_str());
    markWifiJoinStarted();
    lastWifiCheckMs = millis();
  }

  const bool wifiNow = WiFi.status() == WL_CONNECTED;
  if (wifiNow) {
    ipAddress = WiFi.localIP().toString();
    markWifiJoinFinished();
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      statusText = "Wi-Fi connected";
      syncNtp();
      lastRelaySuccessMs = 0;
      lastRelayPollMs = 0;
      lastRelayStatusPushMs = 0;
      relayStatusDirty = true;
      publishStatus();
    }
    lastWifiCheckMs = millis();
  } else {
    ipAddress = "";
    if (wifiJoinActive && millis() - lastWifiBeginMs >= 30000) {
      markWifiJoinFinished();
      statusText = "Wi-Fi connect failed";
      publishStatus();
    }
    if (wifiWasConnected) {
      wifiWasConnected = false;
      statusText = "Wi-Fi reconnecting";
      lastWifiCheckMs = millis();
      publishStatus();
    }
    if (!currentSsid.isEmpty() &&
        storedWifiPass.length() > 0 &&
        !wifiJoinInProgress() &&
        millis() - lastWifiCheckMs >= 60000) {
      statusText = "Retrying Wi-Fi";
      publishStatus();
      configureWifiStaMode();
      delay(100);
      WiFi.setAutoReconnect(true);
      WiFi.begin(currentSsid.c_str(), storedWifiPass.c_str());
      markWifiJoinStarted();
      lastWifiCheckMs = millis();
    }
  }

  if (wifiScanPending) {
    wifiScanPending = false;
    scanWifiNetworks();
  }

  if (wifiConnectPending) {
    wifiConnectPending = false;
    connectToWifi(pendingWifiSsid, pendingWifiPass);
    pendingWifiSsid = "";
    pendingWifiPass = "";
  }

  // ── Stale-cloud / WiFi watchdog ───────────────────────────────────────────
  if (WiFi.status() == WL_CONNECTED && fbConfigured() &&
      lastRelaySuccessMs > 0 && millis() - lastRelaySuccessMs >= 60000) {
    relayStatusDirty = true;
    fbIdToken = "";  // force a fresh sign-in
    if (millis() - lastRelaySuccessMs >= 120000 &&
        !currentSsid.isEmpty() && storedWifiPass.length() > 0 &&
        !wifiJoinInProgress()) {
      statusText = "Cloud stalled, reconnecting Wi-Fi";
      publishStatus();
      WiFi.disconnect(false, false);
      delay(100);
      configureWifiStaMode();
      WiFi.begin(currentSsid.c_str(), storedWifiPass.c_str());
      markWifiJoinStarted();
      lastWifiCheckMs = millis();
      lastRelaySuccessMs = millis();
    }
  }

  // ── Firebase relay: async auth + Firestore poll/status on core 0 ──
  if (!s_relayBusy) {
    // Process the previous result on the main loop (display/state mutations are safe here).
    if (s_relayResultReady) {
      s_relayResultReady = false;
      const int code = s_relayResCode;
      const uint8_t kind = s_reqKind;
      const String resp(s_relayResBody);
      if (kind == FB_SIGNIN || kind == FB_REFRESH) {
        if (code == 200) {
          fbIdToken      = extractJsonStringField(resp, kind == FB_SIGNIN ? "idToken" : "id_token");
          fbRefreshToken = extractJsonStringField(resp, kind == FB_SIGNIN ? "refreshToken" : "refresh_token");
          long ttl = extractJsonStringField(resp, kind == FB_SIGNIN ? "expiresIn" : "expires_in", "3600").toInt();
          fbTokenAcquiredMs = millis();
          fbTokenTtlMs = (unsigned long)(ttl > 60 ? ttl : 3600) * 1000UL;
          lastRelaySuccessMs = millis();
          relayStatusDirty = true;       // push status right after linking
          statusText = "Cloud linked";
          publishStatus();
        } else {
          fbIdToken = ""; fbRefreshToken = "";
          statusText = String("Auth failed ") + code;
          publishStatus();
        }
      } else if (kind == FB_POLL) {
        if (code == 200) {
          lastRelaySuccessMs = millis();
          long cmdId = firestoreExtractInt(resp, "cmdId", -1);
          if (cmdId >= 0 && cmdId != fbLastCmdId) {
            fbLastCmdId = cmdId;
            String cmd = firestoreExtractString(resp, "cmd");
            if (cmd.length()) handleCommandJson(cmd);
          }
        } else if (code == 401) {
          fbIdToken = "";              // token expired/invalid → re-auth
        } else if (code == 403 || code == 404) {
          lastRelaySuccessMs = millis(); // not claimed yet / rules — harmless, token is fine
        }
      } else if (kind == FB_STATUS) {
        if (code >= 200 && code < 300) { lastRelaySuccessMs = millis(); relayStatusDirty = false; }
        else if (code == 401) fbIdToken = "";   // 403 = doc not claimed yet; keep token
      }
      s_reqKind = FB_NONE;
    }

    // Decide and dispatch the next request.
    if (WiFi.status() == WL_CONNECTED && fbConfigured() && s_relayTaskHandle) {
      const bool haveToken = fbIdToken.length() > 0;
      const unsigned long refreshAt = fbTokenTtlMs > 300000UL ? fbTokenTtlMs - 300000UL : fbTokenTtlMs / 2;
      const bool tokenExpiring = haveToken && (millis() - fbTokenAcquiredMs >= refreshAt);
      uint8_t kind = FB_NONE;
      if (!haveToken)            kind = fbRefreshToken.length() ? FB_REFRESH : FB_SIGNIN;
      else if (tokenExpiring)    kind = FB_REFRESH;
      else if (millis() - lastRelayPollMs >= relayPollIntervalMs()) kind = FB_POLL;
      else if (relayStatusDirty || millis() - lastRelayStatusPushMs >= relayStatusIntervalMs()) kind = FB_STATUS;

      if (kind != FB_NONE) {
        s_relayBusy = true;
        s_relayResultReady = false;
        s_reqKind = kind;
        s_reqAuth[0] = '\0'; s_reqCtype[0] = '\0'; s_reqBody[0] = '\0';
        if (kind == FB_SIGNIN) {
          strlcpy(s_reqMethod, "POST", sizeof(s_reqMethod));
          String url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=" + fbApiKey;
          strlcpy(s_reqUrl, url.c_str(), sizeof(s_reqUrl));
          String body = "{\"email\":\"" + jsonEscape(fbDeviceEmail) + "\",\"password\":\"" +
                        jsonEscape(fbDevicePassword) + "\",\"returnSecureToken\":true}";
          strlcpy(s_reqBody, body.c_str(), sizeof(s_reqBody));
          strlcpy(s_reqCtype, "application/json", sizeof(s_reqCtype));
        } else if (kind == FB_REFRESH) {
          strlcpy(s_reqMethod, "POST", sizeof(s_reqMethod));
          String url = "https://securetoken.googleapis.com/v1/token?key=" + fbApiKey;
          strlcpy(s_reqUrl, url.c_str(), sizeof(s_reqUrl));
          String body = "grant_type=refresh_token&refresh_token=" + fbRefreshToken;
          strlcpy(s_reqBody, body.c_str(), sizeof(s_reqBody));
          strlcpy(s_reqCtype, "application/x-www-form-urlencoded", sizeof(s_reqCtype));
        } else if (kind == FB_POLL) {
          lastRelayPollMs = millis();
          strlcpy(s_reqMethod, "GET", sizeof(s_reqMethod));
          strlcpy(s_reqUrl, fbFirestoreDocUrl().c_str(), sizeof(s_reqUrl));
          strlcpy(s_reqAuth, fbIdToken.c_str(), sizeof(s_reqAuth));
        } else { // FB_STATUS
          lastRelayStatusPushMs = millis();
          strlcpy(s_reqMethod, "PATCH", sizeof(s_reqMethod));
          String url = fbFirestoreDocUrl() +
            "?updateMask.fieldPaths=status&updateMask.fieldPaths=online&updateMask.fieldPaths=lastSeenMs"
            "&updateMask.fieldPaths=lastSeenEpoch"
            "&updateMask.fieldPaths=firmware&updateMask.fieldPaths=ip&updateMask.fieldPaths=ssid"
            "&updateMask.fieldPaths=bond&updateMask.fieldPaths=energy&updateMask.fieldPaths=boredom"
            "&updateMask.fieldPaths=mode";
          strlcpy(s_reqUrl, url.c_str(), sizeof(s_reqUrl));
          strlcpy(s_reqAuth, fbIdToken.c_str(), sizeof(s_reqAuth));
          strlcpy(s_reqBody, buildFirestoreStatusBody().c_str(), sizeof(s_reqBody));
          strlcpy(s_reqCtype, "application/json", sizeof(s_reqCtype));
        }
        xTaskNotifyGive(s_relayTaskHandle);
      }
    }
  }

  updatePetBehavior();
  checkTimeGreetings();

  handleTouch();

  // Keep BLE advertising alive — restart every 30 s if no client is connected
  if (bleServer && millis() - lastBleAdvertRestartMs >= 30000UL) {
    lastBleAdvertRestartMs = millis();
    if (bleServer->getConnectedCount() == 0) {
      BLEDevice::startAdvertising();
    }
  }

  delay(1);
}
