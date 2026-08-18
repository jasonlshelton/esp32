#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <FastLED.h>

#include "matrix.h"
#include "eyes.h"
#include "web_ui.h"

#define LED_PIN        16
#define BRIGHTNESS     10
#define FRAME_MS       16          // ~60 fps
#define PSU_VOLTS      5
#define PSU_MILLIAMPS  2000        // 128 pixels at full white would pull ~7.7 A

CRGB leds[NUM_LEDS + 1];
AsyncWebServer server(80);

enum Mode : uint8_t { MODE_EYES = 0, MODE_RAINBOW, MODE_OFF, MODE_TEST };

// Shared with the AsyncTCP task, which runs the HTTP handlers on another core.
// Everything here is a naturally-aligned scalar so reads and writes are atomic;
// colour is packed into one word so a half-applied RGB can never be rendered.
static volatile uint32_t   g_color      = 0xFF00FF;   // Uzi purple
static volatile uint8_t    g_brightness = BRIGHTNESS;
static volatile Mode       g_mode       = MODE_EYES;
static volatile Expression g_expression = EXP_NEUTRAL;
static volatile bool       g_pulse      = false;
static volatile bool       g_idle       = true;

static const char *modeName(Mode m) {
  switch (m) {
    case MODE_RAINBOW: return "rainbow";
    case MODE_OFF:     return "off";
    case MODE_TEST:    return "test";
    default:           return "eyes";
  }
}

static bool modeFromName(const String &s, Mode &out) {
  if (s == "eyes")    { out = MODE_EYES;    return true; }
  if (s == "rainbow") { out = MODE_RAINBOW; return true; }
  if (s == "off")     { out = MODE_OFF;     return true; }
  if (s == "test")    { out = MODE_TEST;    return true; }
  return false;
}

static void drawRainbow(uint32_t now) {
  uint8_t hue = now / 20;
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      setPixel(x, y, CHSV(hue + x * 8 + y * 4, 255, 255));
    }
  }
}

// Wiring calibration. White = origin (0,0). Red ramp brightens along +X, blue
// ramp brightens along +Y, green dot walks the raw chain order. If any of
// those look wrong, adjust the #defines at the top of matrix.h.
static void drawTest(uint32_t now) {
  clearMatrix();
  for (int x = 0; x < MATRIX_WIDTH; x++) {
    setPixel(x, 0, CRGB(map(x, 0, MATRIX_WIDTH - 1, 30, 255), 0, 0));
  }
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    setPixel(0, y, CRGB(0, 0, map(y, 0, MATRIX_HEIGHT - 1, 30, 255)));
  }
  setPixel(0, 0, CRGB::White);
  leds[(now / 60) % NUM_LEDS] = CRGB::Green;
}

void setup() {
  Serial.begin(115200);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(PSU_VOLTS, PSU_MILLIAMPS);
  FastLED.setBrightness(BRIGHTNESS);
  clearMatrix();
  FastLED.show();

  WiFiManager wm;
  // wm.resetSettings(); // uncomment once to force portal for testing, then comment out again

  bool res = wm.autoConnect("VisorSetup", "murderdrones"); // AP name + password for setup portal

  if (!res) {
    Serial.println("Failed to connect, restarting");
    ESP.restart();
  }

  Serial.println("Connected!");
  Serial.println(WiFi.localIP());

  resetEyeAnimation(millis());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    char hex[7];
    snprintf(hex, sizeof(hex), "%06lx", (unsigned long)(g_color & 0xFFFFFF));
    String json = "{";
    json += "\"mode\":\"";       json += modeName(g_mode);                 json += "\",";
    json += "\"expression\":\""; json += expressionName(g_expression);     json += "\",";
    json += "\"color\":\"";      json += hex;                              json += "\",";
    json += "\"brightness\":";   json += g_brightness;                     json += ",";
    json += "\"pulse\":";        json += g_pulse ? "true" : "false";       json += ",";
    json += "\"idle\":";         json += g_idle ? "true" : "false";
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/setcolor", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("r") && request->hasParam("g") && request->hasParam("b")) {
      uint32_t r = constrain(request->getParam("r")->value().toInt(), 0, 255);
      uint32_t g = constrain(request->getParam("g")->value().toInt(), 0, 255);
      uint32_t b = constrain(request->getParam("b")->value().toInt(), 0, 255);
      g_color = (r << 16) | (g << 8) | b;
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/setbrightness", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("v")) {
      g_brightness = constrain(request->getParam("v")->value().toInt(), 0, 255);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/setmode", HTTP_GET, [](AsyncWebServerRequest *request) {
    Mode m;
    if (request->hasParam("mode") && modeFromName(request->getParam("mode")->value(), m)) {
      g_mode = m;
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "bad mode");
    }
  });

  server.on("/setexpression", HTTP_GET, [](AsyncWebServerRequest *request) {
    Expression e;
    if (request->hasParam("e") && expressionFromName(request->getParam("e")->value(), e)) {
      g_expression = e;
      g_mode = MODE_EYES;
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "bad expression");
    }
  });

  server.on("/settoggle", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("pulse")) g_pulse = request->getParam("pulse")->value().toInt() != 0;
    if (request->hasParam("idle"))  g_idle  = request->getParam("idle")->value().toInt() != 0;
    request->send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  static uint32_t lastFrame = 0;
  static Expression lastExpression = EXP_NEUTRAL;

  uint32_t now = millis();
  if (now - lastFrame < FRAME_MS) {
    delay(1);  // yield to the WiFi/AsyncTCP tasks instead of spinning
    return;
  }
  lastFrame = now;

  const uint32_t rgb = g_color;
  const CRGB color((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
  const Mode mode = g_mode;
  const Expression expression = g_expression;

  if (expression != lastExpression) {
    lastExpression = expression;
    resetEyeAnimation(now);
  }

  uint8_t bright = g_brightness;
  if (g_pulse && mode == MODE_EYES) bright = scale8(bright, beatsin8(24, 70, 255));
  FastLED.setBrightness(bright);

  switch (mode) {
    case MODE_EYES:    drawEyes(expression, color, now, g_idle); break;
    case MODE_RAINBOW: drawRainbow(now);                         break;
    case MODE_TEST:    drawTest(now);                            break;
    case MODE_OFF:
    default:           clearMatrix();                            break;
  }

  FastLED.show();
}
