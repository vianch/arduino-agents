/*
 * RoboEyes_TFT.h — FluxGarage RoboEyes V1.1.1, ported to framebuffer-less
 * SPI color TFTs (ST7735 etc.).
 *
 * Based on: FluxGarage RoboEyes for OLED Displays V1.1.1
 * Copyright (C) 2024-2025 Dennis Hoelscher — www.fluxgarage.com
 * GPL-3.0-or-later; this modified version stays GPL.
 *
 * Changes from the original:
 *  - No clearDisplay()/display(): the previous frame's eye rectangles are
 *    erased with dirty-rect strips, so it runs flicker-free on TFTs that
 *    have no framebuffer.
 *  - 16-bit per-instance colors (setDisplayColors) instead of globals.
 *  - setOrigin(x, y): the eyes live in a sub-region of the screen, leaving
 *    room for a status bar, a mouth, and a message bar.
 *  - New mood SUSPICIOUS: flat half eyelids over both eyes.
 *  - Fixed member initialization order (spaceBetween before coordinates —
 *    the original read it uninitialized).
 *  - Removed cyclops mode and the sweat animation.
 */
#ifndef _ROBOEYES_TFT_H
#define _ROBOEYES_TFT_H

// Mood types
#ifdef DEFAULT            // AVR core defines DEFAULT for analogReference()
#undef DEFAULT
#endif
#define DEFAULT 0
#define TIRED 1
#define ANGRY 2
#define HAPPY 3
#define SUSPICIOUS 4

// For turning things on or off
#define ON 1
#define OFF 0

// Predefined gaze positions ("for middle center set DEFAULT")
#define N 1
#define NE 2
#define E 3
#define SE 4
#define S 5
#define SW 6
#define W 7
#define NW 8

template<typename AdafruitDisplay>
class RoboEyes {
public:

AdafruitDisplay *display;

// Colors and drawing origin (TFT port)
uint16_t bgColor = 0x0000;
uint16_t mainColor = 0xFFFF;
int offsetX = 0;
int offsetY = 0;

// General setup - region size and max frame rate
int screenWidth = 128;
int screenHeight = 64;
int frameInterval = 20;
unsigned long fpsTimer = 0;

// Moods and expressions
bool tired = 0;
bool angry = 0;
bool happy = 0;
bool suspicious = 0;
bool curious = 0;   // outer eye grows when looking left/right
bool eyeL_open = 0;
bool eyeR_open = 0;

//*********************************************************************
//  Eyes Geometry
//*********************************************************************

// Space between eyes (declared before the coordinates that use it)
int spaceBetweenDefault = 10;
int spaceBetweenCurrent = spaceBetweenDefault;
int spaceBetweenNext = spaceBetweenDefault;

// EYE LEFT - size and border radius
int eyeLwidthDefault = 36;
int eyeLheightDefault = 36;
int eyeLwidthCurrent = eyeLwidthDefault;
int eyeLheightCurrent = 1;   // start closed
int eyeLwidthNext = eyeLwidthDefault;
int eyeLheightNext = eyeLheightDefault;
int eyeLheightOffset = 0;
byte eyeLborderRadiusDefault = 8;
byte eyeLborderRadiusCurrent = eyeLborderRadiusDefault;
byte eyeLborderRadiusNext = eyeLborderRadiusDefault;

// EYE RIGHT - size and border radius
int eyeRwidthDefault = eyeLwidthDefault;
int eyeRheightDefault = eyeLheightDefault;
int eyeRwidthCurrent = eyeRwidthDefault;
int eyeRheightCurrent = 1;   // start closed
int eyeRwidthNext = eyeRwidthDefault;
int eyeRheightNext = eyeRheightDefault;
int eyeRheightOffset = 0;
byte eyeRborderRadiusDefault = 8;
byte eyeRborderRadiusCurrent = eyeRborderRadiusDefault;
byte eyeRborderRadiusNext = eyeRborderRadiusDefault;

// EYE LEFT - coordinates
int eyeLx = 0;
int eyeLy = 0;
int eyeLxNext = 0;
int eyeLyNext = 0;

// EYE RIGHT - coordinates
int eyeRx = 0;
int eyeRy = 0;
int eyeRxNext = 0;
int eyeRyNext = 0;

// Eyelids
byte eyelidsTiredHeight = 0;
byte eyelidsTiredHeightNext = 0;
byte eyelidsAngryHeight = 0;
byte eyelidsAngryHeightNext = 0;
byte eyelidsHappyBottomOffset = 0;
byte eyelidsHappyBottomOffsetNext = 0;
byte eyelidsSuspiciousHeight = 0;
byte eyelidsSuspiciousHeightNext = 0;

//*********************************************************************
//  Macro Animations
//*********************************************************************

bool hFlicker = 0;
bool hFlickerAlternate = 0;
byte hFlickerAmplitude = 2;

bool vFlicker = 0;
bool vFlickerAlternate = 0;
byte vFlickerAmplitude = 10;

// Flicker toggles on a timer instead of every frame (the original shook at
// full framerate, way too violent on a 50fps TFT).
unsigned long flickerTimer = 0;
uint16_t flickerStepMs = 70;

bool autoblinker = 0;
int blinkInterval = 1;           // seconds
int blinkIntervalVariation = 4;  // seconds
unsigned long blinktimer = 0;

bool idle = 0;                   // eyes wander to random positions
int idleInterval = 1;
int idleIntervalVariation = 3;
unsigned long idleAnimationTimer = 0;

bool confused = 0;               // one-shot horizontal shake
unsigned long confusedAnimationTimer = 0;
int confusedAnimationDuration = 500;
bool confusedToggle = 1;

bool laugh = 0;                  // one-shot vertical bounce
unsigned long laughAnimationTimer = 0;
int laughAnimationDuration = 500;
bool laughToggle = 1;

// Previously drawn geometry (TFT port dirty-rect state)
int prevLx = 0, prevLy = 0, prevLw = 0, prevLh = 0;
int prevRx = 0, prevRy = 0, prevRw = 0, prevRh = 0;
byte prevTired = 255, prevAngry = 255, prevHappy = 255, prevSusp = 255;
byte prevRadL = 255, prevRadR = 255;

//*********************************************************************
//  GENERAL METHODS
//*********************************************************************

RoboEyes(AdafruitDisplay &disp) : display(&disp) {}

void begin(int width, int height, byte frameRate) {
  screenWidth = width;
  screenHeight = height;
  eyeLheightCurrent = 1;   // start with closed eyes
  eyeRheightCurrent = 1;
  setFramerate(frameRate);
  setPosition(DEFAULT);
  eyeLx = eyeLxNext; eyeLy = eyeLyNext;   // don't tween in from garbage
  eyeRx = eyeLx + eyeLwidthCurrent + spaceBetweenCurrent; eyeRy = eyeLy;
  display->fillRect(offsetX, offsetY, screenWidth, screenHeight, bgColor);
}

void update() {
  if (millis() - fpsTimer >= (unsigned long)frameInterval) {
    drawEyes();
    fpsTimer = millis();
  }
}

//*********************************************************************
//  SETTERS
//*********************************************************************

void setFramerate(byte fps) { frameInterval = 1000 / fps; }

void setDisplayColors(uint16_t background, uint16_t main) {
  bgColor = background;
  mainColor = main;
}

void setOrigin(int x, int y) { offsetX = x; offsetY = y; }

void setWidth(byte leftEye, byte rightEye) {
  eyeLwidthNext = leftEye;   eyeRwidthNext = rightEye;
  eyeLwidthDefault = leftEye; eyeRwidthDefault = rightEye;
}

void setHeight(byte leftEye, byte rightEye) {
  eyeLheightNext = leftEye;    eyeRheightNext = rightEye;
  eyeLheightDefault = leftEye; eyeRheightDefault = rightEye;
}

void setBorderradius(byte leftEye, byte rightEye) {
  eyeLborderRadiusNext = leftEye;    eyeRborderRadiusNext = rightEye;
  eyeLborderRadiusDefault = leftEye; eyeRborderRadiusDefault = rightEye;
}

void setSpacebetween(int space) {
  spaceBetweenNext = space;
  spaceBetweenDefault = space;
}

void setMood(unsigned char mood) {
  tired = angry = happy = suspicious = 0;
  switch (mood) {
    case TIRED:      tired = 1;      break;
    case ANGRY:      angry = 1;      break;
    case HAPPY:      happy = 1;      break;
    case SUSPICIOUS: suspicious = 1; break;
    default: break;
  }
}

void setPosition(unsigned char position) {
  switch (position) {
    case N:  eyeLxNext = getScreenConstraint_X() / 2; eyeLyNext = 0; break;
    case NE: eyeLxNext = getScreenConstraint_X();     eyeLyNext = 0; break;
    case E:  eyeLxNext = getScreenConstraint_X();     eyeLyNext = getScreenConstraint_Y() / 2; break;
    case SE: eyeLxNext = getScreenConstraint_X();     eyeLyNext = getScreenConstraint_Y(); break;
    case S:  eyeLxNext = getScreenConstraint_X() / 2; eyeLyNext = getScreenConstraint_Y(); break;
    case SW: eyeLxNext = 0;                           eyeLyNext = getScreenConstraint_Y(); break;
    case W:  eyeLxNext = 0;                           eyeLyNext = getScreenConstraint_Y() / 2; break;
    case NW: eyeLxNext = 0;                           eyeLyNext = 0; break;
    default: eyeLxNext = getScreenConstraint_X() / 2; eyeLyNext = getScreenConstraint_Y() / 2; break;
  }
}

void setAutoblinker(bool active, int interval, int variation) {
  autoblinker = active;
  blinkInterval = interval;
  blinkIntervalVariation = variation;
}
void setAutoblinker(bool active) { autoblinker = active; }

void setIdleMode(bool active, int interval, int variation) {
  idle = active;
  idleInterval = interval;
  idleIntervalVariation = variation;
}
void setIdleMode(bool active) { idle = active; }

void setCuriosity(bool curiousBit) { curious = curiousBit; }

void setHFlicker(bool flickerBit, byte amplitude) { hFlicker = flickerBit; hFlickerAmplitude = amplitude; }
void setHFlicker(bool flickerBit) { hFlicker = flickerBit; }
void setVFlicker(bool flickerBit, byte amplitude) { vFlicker = flickerBit; vFlickerAmplitude = amplitude; }
void setVFlicker(bool flickerBit) { vFlicker = flickerBit; }

//*********************************************************************
//  GETTERS
//*********************************************************************

int getScreenConstraint_X() {
  return screenWidth - eyeLwidthCurrent - spaceBetweenCurrent - eyeRwidthCurrent;
}

int getScreenConstraint_Y() {
  return screenHeight - eyeLheightDefault;
}

//*********************************************************************
//  BASIC ANIMATION METHODS
//*********************************************************************

void close() {
  eyeLheightNext = 1; eyeRheightNext = 1;
  eyeL_open = 0; eyeR_open = 0;
}

void open() { eyeL_open = 1; eyeR_open = 1; }

void blink() { close(); open(); }

void close(bool left, bool right) {
  if (left)  { eyeLheightNext = 1; eyeL_open = 0; }
  if (right) { eyeRheightNext = 1; eyeR_open = 0; }
}

void open(bool left, bool right) {
  if (left)  eyeL_open = 1;
  if (right) eyeR_open = 1;
}

void blink(bool left, bool right) { close(left, right); open(left, right); }

//*********************************************************************
//  MACRO ANIMATION METHODS
//*********************************************************************

void anim_confused() { confused = 1; }
void anim_laugh()    { laugh = 1; }

//*********************************************************************
//  PRE-CALCULATIONS AND ACTUAL DRAWINGS
//*********************************************************************

// Erase the parts of the old rectangle not covered by the new one
// (up to four strips). Keeps blinks and moves flicker-free.
void eraseOutside(int ox, int oy, int ow, int oh, int nx, int ny, int nw, int nh) {
  if (ow <= 0 || oh <= 0) return;
  if (nx >= ox + ow || nx + nw <= ox || ny >= oy + oh || ny + nh <= oy) {
    display->fillRect(ox, oy, ow, oh, bgColor);   // no overlap at all
    return;
  }
  if (ny > oy)           display->fillRect(ox, oy, ow, ny - oy, bgColor);
  if (ny + nh < oy + oh) display->fillRect(ox, ny + nh, ow, (oy + oh) - (ny + nh), bgColor);
  int iy = (oy > ny) ? oy : ny;
  int ih = ((oy + oh < ny + nh) ? (oy + oh) : (ny + nh)) - iy;
  if (nx > ox)           display->fillRect(ox, iy, nx - ox, ih, bgColor);
  if (nx + nw < ox + ow) display->fillRect(nx + nw, iy, (ox + ow) - (nx + nw), ih, bgColor);
}

void drawEyes() {

  //// PRE-CALCULATIONS - TWEENING (identical math to the original) ////

  if (curious) {
    if (eyeLxNext <= 10) { eyeLheightOffset = 8; } else { eyeLheightOffset = 0; }
    if (eyeRxNext >= screenWidth - eyeRwidthCurrent - 10) { eyeRheightOffset = 8; } else { eyeRheightOffset = 0; }
  } else {
    eyeLheightOffset = 0;
    eyeRheightOffset = 0;
  }

  eyeLheightCurrent = (eyeLheightCurrent + eyeLheightNext + eyeLheightOffset) / 2;
  eyeLy += ((eyeLheightDefault - eyeLheightCurrent) / 2);
  eyeLy -= eyeLheightOffset / 2;
  eyeRheightCurrent = (eyeRheightCurrent + eyeRheightNext + eyeRheightOffset) / 2;
  eyeRy += (eyeRheightDefault - eyeRheightCurrent) / 2;
  eyeRy -= eyeRheightOffset / 2;

  if (eyeL_open && eyeLheightCurrent <= 1 + eyeLheightOffset) { eyeLheightNext = eyeLheightDefault; }
  if (eyeR_open && eyeRheightCurrent <= 1 + eyeRheightOffset) { eyeRheightNext = eyeRheightDefault; }

  eyeLwidthCurrent = (eyeLwidthCurrent + eyeLwidthNext) / 2;
  eyeRwidthCurrent = (eyeRwidthCurrent + eyeRwidthNext) / 2;

  spaceBetweenCurrent = (spaceBetweenCurrent + spaceBetweenNext) / 2;

  eyeLx = (eyeLx + eyeLxNext) / 2;
  eyeLy = (eyeLy + eyeLyNext) / 2;
  eyeRxNext = eyeLxNext + eyeLwidthCurrent + spaceBetweenCurrent;
  eyeRyNext = eyeLyNext;
  eyeRx = (eyeRx + eyeRxNext) / 2;
  eyeRy = (eyeRy + eyeRyNext) / 2;

  eyeLborderRadiusCurrent = (eyeLborderRadiusCurrent + eyeLborderRadiusNext) / 2;
  eyeRborderRadiusCurrent = (eyeRborderRadiusCurrent + eyeRborderRadiusNext) / 2;

  //// MACRO ANIMATIONS ////

  if (autoblinker && millis() >= blinktimer) {
    blink();
    blinktimer = millis() + (blinkInterval * 1000UL) + (random(blinkIntervalVariation) * 1000UL);
  }

  if (laugh) {
    if (laughToggle) {
      setVFlicker(1, 2);
      laughAnimationTimer = millis();
      laughToggle = 0;
    } else if (millis() >= laughAnimationTimer + (unsigned long)laughAnimationDuration) {
      setVFlicker(0, 0);
      laughToggle = 1;
      laugh = 0;
    }
  }

  if (confused) {
    if (confusedToggle) {
      setHFlicker(1, 4);
      confusedAnimationTimer = millis();
      confusedToggle = 0;
    } else if (millis() >= confusedAnimationTimer + (unsigned long)confusedAnimationDuration) {
      setHFlicker(0, 0);
      confusedToggle = 1;
      confused = 0;
    }
  }

  if (idle && millis() >= idleAnimationTimer) {
    eyeLxNext = random(getScreenConstraint_X());
    eyeLyNext = random(getScreenConstraint_Y());
    idleAnimationTimer = millis() + (idleInterval * 1000UL) + (random(idleIntervalVariation) * 1000UL);
  }

  // Flicker is a draw-time offset (original mutated the coordinates every
  // frame; with a timed toggle that would drift).
  if ((hFlicker || vFlicker) && millis() - flickerTimer >= flickerStepMs) {
    hFlickerAlternate = !hFlickerAlternate;
    vFlickerAlternate = !vFlickerAlternate;
    flickerTimer = millis();
  }
  int flickX = hFlicker ? (hFlickerAlternate ? hFlickerAmplitude : -hFlickerAmplitude) : 0;
  int flickY = vFlicker ? (vFlickerAlternate ? vFlickerAmplitude : -vFlickerAmplitude) : 0;

  //// EYELID TARGETS AND TWEENS ////

  if (tired) { eyelidsTiredHeightNext = eyeLheightCurrent / 2; } else { eyelidsTiredHeightNext = 0; }
  if (angry) { eyelidsAngryHeightNext = eyeLheightCurrent / 2; } else { eyelidsAngryHeightNext = 0; }
  if (happy) { eyelidsHappyBottomOffsetNext = eyeLheightCurrent / 2; } else { eyelidsHappyBottomOffsetNext = 0; }
  if (suspicious) { eyelidsSuspiciousHeightNext = (eyeLheightCurrent * 2) / 5; } else { eyelidsSuspiciousHeightNext = 0; }

  eyelidsTiredHeight = (eyelidsTiredHeight + eyelidsTiredHeightNext) / 2;
  eyelidsAngryHeight = (eyelidsAngryHeight + eyelidsAngryHeightNext) / 2;
  eyelidsHappyBottomOffset = (eyelidsHappyBottomOffset + eyelidsHappyBottomOffsetNext) / 2;
  eyelidsSuspiciousHeight = (eyelidsSuspiciousHeight + eyelidsSuspiciousHeightNext) / 2;

  //// DIRTY CHECK - skip drawing entirely when nothing moved ////

  int newLx = offsetX + eyeLx + flickX, newLy = offsetY + eyeLy + flickY;
  int newRx = offsetX + eyeRx + flickX, newRy = offsetY + eyeRy + flickY;

  bool changed =
    newLx != prevLx || newLy != prevLy || eyeLwidthCurrent != prevLw || eyeLheightCurrent != prevLh ||
    newRx != prevRx || newRy != prevRy || eyeRwidthCurrent != prevRw || eyeRheightCurrent != prevRh ||
    eyelidsTiredHeight != prevTired || eyelidsAngryHeight != prevAngry ||
    eyelidsHappyBottomOffset != prevHappy || eyelidsSuspiciousHeight != prevSusp ||
    eyeLborderRadiusCurrent != prevRadL || eyeRborderRadiusCurrent != prevRadR;
  if (!changed) return;

  //// ACTUAL DRAWINGS ////

  eraseOutside(prevLx, prevLy, prevLw, prevLh, newLx, newLy, eyeLwidthCurrent, eyeLheightCurrent);
  eraseOutside(prevRx, prevRy, prevRw, prevRh, newRx, newRy, eyeRwidthCurrent, eyeRheightCurrent);

  display->fillRoundRect(newLx, newLy, eyeLwidthCurrent, eyeLheightCurrent, eyeLborderRadiusCurrent, mainColor);
  display->fillRoundRect(newRx, newRy, eyeRwidthCurrent, eyeRheightCurrent, eyeRborderRadiusCurrent, mainColor);

  // Tired top eyelids (outer wedge)
  if (eyelidsTiredHeight > 0) {
    display->fillTriangle(newLx, newLy - 1, newLx + eyeLwidthCurrent, newLy - 1, newLx, newLy + eyelidsTiredHeight - 1, bgColor);
    display->fillTriangle(newRx, newRy - 1, newRx + eyeRwidthCurrent, newRy - 1, newRx + eyeRwidthCurrent, newRy + eyelidsTiredHeight - 1, bgColor);
  }

  // Angry top eyelids (inner wedge)
  if (eyelidsAngryHeight > 0) {
    display->fillTriangle(newLx, newLy - 1, newLx + eyeLwidthCurrent, newLy - 1, newLx + eyeLwidthCurrent, newLy + eyelidsAngryHeight - 1, bgColor);
    display->fillTriangle(newRx, newRy - 1, newRx + eyeRwidthCurrent, newRy - 1, newRx, newRy + eyelidsAngryHeight - 1, bgColor);
  }

  // Suspicious flat half lids
  if (eyelidsSuspiciousHeight > 0) {
    display->fillRect(newLx, newLy, eyeLwidthCurrent, eyelidsSuspiciousHeight, bgColor);
    display->fillRect(newRx, newRy, eyeRwidthCurrent, eyelidsSuspiciousHeight, bgColor);
  }

  // Happy bottom eyelids (height trimmed to the lid itself so it never
  // spills far below the eye and eats the mouth)
  if (eyelidsHappyBottomOffset > 0) {
    display->fillRoundRect(newLx - 1, (newLy + eyeLheightCurrent) - eyelidsHappyBottomOffset + 1, eyeLwidthCurrent + 2, eyelidsHappyBottomOffset + 2, eyeLborderRadiusCurrent, bgColor);
    display->fillRoundRect(newRx - 1, (newRy + eyeRheightCurrent) - eyelidsHappyBottomOffset + 1, eyeRwidthCurrent + 2, eyelidsHappyBottomOffset + 2, eyeRborderRadiusCurrent, bgColor);
  }

  prevLx = newLx; prevLy = newLy; prevLw = eyeLwidthCurrent; prevLh = eyeLheightCurrent;
  prevRx = newRx; prevRy = newRy; prevRw = eyeRwidthCurrent; prevRh = eyeRheightCurrent;
  prevTired = eyelidsTiredHeight; prevAngry = eyelidsAngryHeight;
  prevHappy = eyelidsHappyBottomOffset; prevSusp = eyelidsSuspiciousHeight;
  prevRadL = eyeLborderRadiusCurrent; prevRadR = eyeRborderRadiusCurrent;
}

}; // end of class RoboEyes

#endif
