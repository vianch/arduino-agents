/*
 * RoboMouth_TFT.h — small curvy mouth companion to RoboEyes_TFT.
 * Smooth arcs drawn with Adafruit_GFX circle helpers, sized for a small
 * dark face screen (like the "pet bot" reference). Dirty-box rendering:
 * repaints only when shape, animation frame, or color changes.
 *
 * Shapes:
 *   MOUTH_FLAT    — rounded neutral bar
 *   MOUTH_SMILE   — thin upward arc
 *   MOUTH_FROWN   — thin downward arc
 *   MOUTH_GRIN    — filled half-circle open smile
 *   MOUTH_O       — surprised ring
 *   MOUTH_SMIRK   — half arc to one side
 *   MOUTH_ZIGZAG  — square-wave worry/error mouth
 *   MOUTH_TALK    — animated capsule opening and closing
 *   MOUTH_DOTS    — animated "..." thinking indicator
 */
#ifndef _ROBOMOUTH_TFT_H
#define _ROBOMOUTH_TFT_H

#define MOUTH_FLAT   0
#define MOUTH_SMILE  1
#define MOUTH_FROWN  2
#define MOUTH_GRIN   3
#define MOUTH_O      4
#define MOUTH_SMIRK  5
#define MOUTH_ZIGZAG 6
#define MOUTH_TALK   7
#define MOUTH_DOTS   8

template<typename AdafruitDisplay>
class RoboMouth {
public:

AdafruitDisplay *display;

uint16_t bgColor = 0x0000;
uint16_t mainColor = 0xFFFF;
int cx = 64, cy = 70;    // mouth center

uint8_t shape = MOUTH_FLAT;
uint8_t frame = 0;
uint16_t talkInterval = 160;
uint16_t dotsInterval = 350;
unsigned long animTimer = 0;

uint8_t drawnShape = 255;
uint8_t drawnFrame = 255;
uint16_t drawnColor = 0;

RoboMouth(AdafruitDisplay &disp) : display(&disp) {}

void begin(int centerX, int centerY, uint16_t background, uint16_t main) {
  cx = centerX; cy = centerY;
  bgColor = background; mainColor = main;
}

void setShape(uint8_t newShape) {
  if (shape != newShape) {
    shape = newShape;
    frame = 0;
    animTimer = millis();
  }
}

void setColor(uint16_t main) { mainColor = main; }

void update() {
  if (shape == MOUTH_TALK && millis() - animTimer >= talkInterval) {
    frame = (frame + 1) & 3;
    animTimer = millis();
  }
  if (shape == MOUTH_DOTS && millis() - animTimer >= dotsInterval) {
    frame = (frame + 1) % 3;
    animTimer = millis();
  }

  if (shape == drawnShape && frame == drawnFrame && mainColor == drawnColor) return;

  display->fillRect(cx - 18, cy - 12, 36, 24, bgColor);   // mouth box
  draw();
  drawnShape = shape; drawnFrame = frame; drawnColor = mainColor;
}

// Thick arc: bottom (smile, corners 0xC) or top (frown, corners 0x3) half
// of a circle, drawn at two radii for a 2px stroke.
void arc(int x, int y, int r, uint8_t corners) {
  display->drawCircleHelper(x, y, r, corners, mainColor);
  display->drawCircleHelper(x, y, r - 1, corners, mainColor);
}

void draw() {
  switch (shape) {

    case MOUTH_SMILE:
      arc(cx, cy - 4, 11, 0x4 | 0x8);                      // upward curve
      break;

    case MOUTH_FROWN:
      arc(cx, cy + 4, 11, 0x1 | 0x2);                      // downward curve
      break;

    case MOUTH_GRIN:                                       // open half-circle
      display->fillCircle(cx, cy, 10, mainColor);
      display->fillRect(cx - 11, cy - 11, 23, 11, bgColor);   // keep the bottom half
      break;

    case MOUTH_O:                                          // surprised ring
      display->drawCircle(cx, cy, 7, mainColor);
      display->drawCircle(cx, cy, 6, mainColor);
      break;

    case MOUTH_SMIRK:
      arc(cx - 2, cy - 4, 11, 0x4);                        // right half-arc
      display->fillRoundRect(cx - 14, cy - 2, 11, 4, 2, mainColor);
      break;

    case MOUTH_ZIGZAG: {                                   // square-wave worry
      for (uint8_t i = 0; i < 4; i++) {
        int y = (i & 1) ? cy + 1 : cy - 4;
        display->fillRect(cx - 16 + i * 8, y, 8, 4, mainColor);
      }
      break;
    }

    case MOUTH_TALK: {                                     // open/close chatter
      static const uint8_t talkHeights[4] = { 4, 8, 14, 8 };
      uint8_t h = talkHeights[frame & 3];
      display->fillRoundRect(cx - 10, cy - h / 2, 20, h, h / 2, mainColor);
      break;
    }

    case MOUTH_DOTS: {                                     // thinking "..."
      uint8_t dots = frame + 1;                            // 1..3
      for (uint8_t i = 0; i < dots; i++) {
        display->fillCircle(cx - 11 + i * 11, cy, 3, mainColor);
      }
      break;
    }

    default:                                               // MOUTH_FLAT
      display->fillRoundRect(cx - 11, cy - 2, 22, 5, 2, mainColor);
      break;
  }
}

}; // end of class RoboMouth

#endif
