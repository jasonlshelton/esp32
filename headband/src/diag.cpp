// Diagnostic: cycles all pixels red -> green -> blue -> off, 1s each.
// Built only by env:diag. Upload with: pio run -e diag -t upload
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN     D2
#define LED_COUNT   10
#define BRIGHTNESS  64

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  Serial.println("diag start");
}

void loop() {
  static const uint32_t colors[] = {
    strip.Color(255, 0, 0), strip.Color(0, 255, 0), strip.Color(0, 0, 255), 0
  };
  static const char *names[] = {"red", "green", "blue", "off"};

  for (uint8_t i = 0; i < 4; i++) {
    strip.fill(colors[i]);
    strip.show();
    Serial.println(names[i]);
    delay(1000);
  }
}
