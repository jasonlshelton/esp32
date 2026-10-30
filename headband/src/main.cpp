#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN     D2   // GPIO4; avoid D3/D4/D8 (boot-strap pins)
#define LED_COUNT   10
#define BRIGHTNESS  128
#define COLOR_R     255  // 255,170,0 reads as yellow; 255,255,0 looks green
#define COLOR_G     170
#define COLOR_B     0

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  WiFi.mode(WIFI_OFF);

  Serial.begin(115200);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.fill(strip.Color(COLOR_R, COLOR_G, COLOR_B));
  strip.show();

  Serial.println("LEDs on");
}

void loop() {
}
