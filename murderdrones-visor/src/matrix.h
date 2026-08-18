#pragma once
#include <FastLED.h>

// ---- Panel geometry -------------------------------------------------------
#define MATRIX_WIDTH   16
#define MATRIX_HEIGHT  8
#define NUM_LEDS       (MATRIX_WIDTH * MATRIX_HEIGHT)

// ---- Wiring ---------------------------------------------------------------
// Default: pixel 0 at top-left, chain runs in rows of 16 that zigzag.
// If the "test" mode looks wrong, flip these one at a time. The test pattern
// draws a white pixel at (0,0), a red ramp along +X, a blue ramp along +Y, and
// a green dot walking the raw chain order.
#define MATRIX_SERPENTINE       1  // 1 = alternate lines run backwards
#define MATRIX_VERTICAL_STRIPS  0  // 1 = chain runs in columns of 8, not rows of 16
#define MATRIX_FLIP_X           0
#define MATRIX_FLIP_Y           0

// leds[NUM_LEDS] is a scratch pixel that absorbs out-of-bounds writes, so
// drawing code can offset shapes off the edge without bounds-checking itself.
extern CRGB leds[NUM_LEDS + 1];

static inline uint16_t XY(int x, int y) {
  if (x < 0 || y < 0 || x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return NUM_LEDS;
#if MATRIX_FLIP_X
  x = MATRIX_WIDTH - 1 - x;
#endif
#if MATRIX_FLIP_Y
  y = MATRIX_HEIGHT - 1 - y;
#endif
#if MATRIX_VERTICAL_STRIPS
  #if MATRIX_SERPENTINE
  if (x & 1) y = MATRIX_HEIGHT - 1 - y;
  #endif
  return (uint16_t)(x * MATRIX_HEIGHT + y);
#else
  #if MATRIX_SERPENTINE
  if (y & 1) x = MATRIX_WIDTH - 1 - x;
  #endif
  return (uint16_t)(y * MATRIX_WIDTH + x);
#endif
}

static inline void setPixel(int x, int y, const CRGB &c) { leds[XY(x, y)] = c; }
static inline void clearMatrix() { fill_solid(leds, NUM_LEDS, CRGB::Black); }
