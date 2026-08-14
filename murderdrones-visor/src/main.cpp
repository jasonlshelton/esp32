#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <FastLED.h>

#define LED_PIN     16
#define NUM_LEDS    60
#define BRIGHTNESS  80

CRGB leds[NUM_LEDS];
AsyncWebServer server(80);

// current state
uint8_t currentR = 255, currentG = 0, currentB = 0;
uint8_t currentBrightness = BRIGHTNESS;
String currentMode = "solid";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Visor Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; background:#111; color:#eee; text-align:center; padding:20px; }
    input[type=color] { width:100px; height:60px; border:none; }
    input[type=range] { width:80%; max-width:320px; accent-color:#e33; }
    label { font-size:14px; color:#aaa; }
    button { padding:12px 20px; margin:6px; font-size:16px; border-radius:8px; border:none; background:#333; color:#eee; }
    button.active { background:#e33; }
  </style>
</head>
<body>
  <h2>Murder Drones Visor</h2>
  <input type="color" id="colorPicker" value="#ff0000">
  <br><br>
  <label for="brightness">Brightness <span id="brightVal">80</span></label><br>
  <input type="range" id="brightness" min="1" max="255" value="80">
  <br><br>
  <button onclick="setMode('solid')">Solid</button>
  <button onclick="setMode('pulse')">Pulse</button>
  <button onclick="setMode('rainbow')">Rainbow</button>
  <button onclick="setMode('off')">Off</button>

  <script>
    const picker = document.getElementById('colorPicker');
    picker.addEventListener('input', () => {
      const hex = picker.value.substring(1);
      const r = parseInt(hex.substring(0,2), 16);
      const g = parseInt(hex.substring(2,4), 16);
      const b = parseInt(hex.substring(4,6), 16);
      fetch(`/setcolor?r=${r}&g=${g}&b=${b}`);
    });

    function setMode(mode) {
      fetch(`/setmode?mode=${mode}`);
    }

    // Dragging the slider fires 'input' far faster than the ESP32 can service
    // requests, so coalesce: one in flight at a time, always send the last value.
    const bright = document.getElementById('brightness');
    const brightVal = document.getElementById('brightVal');
    let pending = null, inflight = false;

    function sendBrightness(v) {
      if (inflight) { pending = v; return; }
      inflight = true;
      fetch(`/setbrightness?v=${v}`).finally(() => {
        inflight = false;
        if (pending !== null) { const next = pending; pending = null; sendBrightness(next); }
      });
    }

    bright.addEventListener('input', () => {
      brightVal.textContent = bright.value;
      sendBrightness(bright.value);
    });
  </script>
</body>
</html>
)rawliteral";

void applyLEDs() {
  FastLED.setBrightness(currentBrightness);
  if (currentMode == "solid") {
    fill_solid(leds, NUM_LEDS, CRGB(currentR, currentG, currentB));
    FastLED.show();
  } else if (currentMode == "off") {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
  }
  // pulse/rainbow handled in loop() since they're animated
}

void setup() {
  Serial.begin(115200);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
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

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/setcolor", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("r") && request->hasParam("g") && request->hasParam("b")) {
      currentR = request->getParam("r")->value().toInt();
      currentG = request->getParam("g")->value().toInt();
      currentB = request->getParam("b")->value().toInt();
      currentMode = "solid";
      applyLEDs();
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/setbrightness", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("v")) {
      currentBrightness = constrain(request->getParam("v")->value().toInt(), 0, 255);
      applyLEDs();
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/setmode", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("mode")) {
      currentMode = request->getParam("mode")->value();
      applyLEDs();
    }
    request->send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  if (currentMode == "pulse") {
    static uint8_t brightness = 0;
    static int8_t direction = 5;
    brightness += direction;
    if (brightness == 0 || brightness == 255) direction = -direction;
    fill_solid(leds, NUM_LEDS, CRGB(currentR, currentG, currentB));
    FastLED.setBrightness(brightness);
    FastLED.show();
    delay(20);
  } else if (currentMode == "rainbow") {
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue, 7);
    FastLED.setBrightness(currentBrightness);
    FastLED.show();
    hue++;
    delay(20);
  }
}