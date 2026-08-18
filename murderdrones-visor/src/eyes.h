#pragma once
#include <Arduino.h>
#include <FastLED.h>

enum Expression : uint8_t {
  EXP_NEUTRAL = 0,
  EXP_X,
  EXP_ANGRY,
  EXP_HAPPY,
  EXP_COUNT
};

const char *expressionName(Expression e);
bool expressionFromName(const String &s, Expression &out);

// Renders both eyes into leds[]. Clears the matrix first.
// `now` is millis(); `idleMotion` enables the slow gaze drift.
void drawEyes(Expression e, const CRGB &color, uint32_t now, bool idleMotion);

// Re-arms blink/gaze timers so a switch doesn't land mid-animation.
void resetEyeAnimation(uint32_t now);
