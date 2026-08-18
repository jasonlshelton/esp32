#include "eyes.h"
#include "matrix.h"

// Each eye is a 7x8 sprite. The left eye sits at x=0, the right eye is the
// same sprite mirrored at x=9, leaving a 2-column gap down the middle of the
// 16-wide panel. Sprites are authored for the LEFT eye, so "inner edge" means
// the right-hand columns.
#define EYE_W 7
#define EYE_H 8
#define RIGHT_EYE_X (MATRIX_WIDTH - EYE_W)

struct EyeShape {
  const char rows[EYE_H][EYE_W + 1];
  bool animated;  // blink + gaze drift apply to this shape
};

static const EyeShape SHAPES[EXP_COUNT] = {
  // EXP_NEUTRAL - rounded block, inset one pixel so it has room to drift
  {{".......",
    "..###..",
    ".#####.",
    ".#####.",
    ".#####.",
    ".#####.",
    "..###..",
    "......."}, true},

  // EXP_X - kill mode, corner to corner
  {{"#.....#",
    ".#...#.",
    "..#.#..",
    "...#...",
    "...#...",
    "..#.#..",
    ".#...#.",
    "#.....#"}, false},

  // EXP_ANGRY - brow slants down toward the centre of the visor
  {{".......",
    ".#.....",
    ".###...",
    ".#####.",
    ".#####.",
    ".#####.",
    "..###..",
    "......."}, true},

  // EXP_HAPPY - upward chevron, already a closed eye so it never blinks
  {{".......",
    ".......",
    "...#...",
    "..###..",
    ".##.##.",
    "##...##",
    ".......",
    "......."}, false},
};

static const char *const EXPRESSION_NAMES[EXP_COUNT] = {
  "neutral", "x", "angry", "happy"
};

const char *expressionName(Expression e) {
  return (e < EXP_COUNT) ? EXPRESSION_NAMES[e] : EXPRESSION_NAMES[EXP_NEUTRAL];
}

bool expressionFromName(const String &s, Expression &out) {
  for (uint8_t i = 0; i < EXP_COUNT; i++) {
    if (s == EXPRESSION_NAMES[i]) { out = (Expression)i; return true; }
  }
  return false;
}

// ---- Blink ----------------------------------------------------------------
// Lids close symmetrically from top and bottom; level 4 is fully shut and
// draws a single lit line instead. ~180 ms end to end.
static const uint8_t BLINK_SEQ[] = {1, 2, 3, 4, 4, 3, 2, 1};
static const uint8_t BLINK_STEPS = sizeof(BLINK_SEQ);
static const uint8_t BLINK_STEP_MS = 22;
static const uint8_t LID_CLOSED = 4;

static uint32_t nextBlinkAt = 0;
static uint32_t blinkStart = 0;
static bool blinking = false;

// ---- Idle gaze ------------------------------------------------------------
static uint32_t nextGazeAt = 0;
static int8_t gazeX = 0, gazeY = 0;

static void scheduleBlink(uint32_t now) { nextBlinkAt = now + random(2200, 6000); }

void resetEyeAnimation(uint32_t now) {
  blinking = false;
  scheduleBlink(now);
  gazeX = gazeY = 0;
  nextGazeAt = now + 800;
}

static void drawEye(const EyeShape &s, int originX, bool mirror,
                    int dx, int dy, uint8_t lid, const CRGB &c) {
  if (lid >= LID_CLOSED) {
    for (int i = 1; i < EYE_W - 1; i++) setPixel(originX + i + dx, 3 + dy, c);
    return;
  }
  for (int y = lid; y < EYE_H - lid; y++) {
    for (int x = 0; x < EYE_W; x++) {
      if (s.rows[y][x] != '#') continue;
      int sx = mirror ? (EYE_W - 1 - x) : x;
      setPixel(originX + sx + dx, y + dy, c);
    }
  }
}

void drawEyes(Expression e, const CRGB &color, uint32_t now, bool idleMotion) {
  if (e >= EXP_COUNT) e = EXP_NEUTRAL;
  const EyeShape &s = SHAPES[e];

  uint8_t lid = 0;
  int dx = 0, dy = 0;

  if (s.animated) {
    if (!blinking && (int32_t)(now - nextBlinkAt) >= 0) {
      blinking = true;
      blinkStart = now;
    }
    if (blinking) {
      uint32_t step = (now - blinkStart) / BLINK_STEP_MS;
      if (step >= BLINK_STEPS) {
        blinking = false;
        scheduleBlink(now);
      } else {
        lid = BLINK_SEQ[step];
      }
    }

    if (idleMotion) {
      if ((int32_t)(now - nextGazeAt) >= 0) {
        gazeX = (int8_t)random(-1, 2);
        gazeY = (random8() < 64) ? (int8_t)random(-1, 2) : 0;  // vertical drift is rarer
        nextGazeAt = now + random(1200, 3800);
      }
      dx = gazeX;
      dy = gazeY;
    } else {
      gazeX = gazeY = 0;
    }
  }

  clearMatrix();
  drawEye(s, 0,           false, dx, dy, lid, color);
  drawEye(s, RIGHT_EYE_X, true,  dx, dy, lid, color);
}
