// bot_notifier.ino — "CLABOT" status card for Claude Code sessions.
// Hardware: Arduino Nano/Uno + ST7735S 128x160 SPI TFT. Protocol: README.md.
//
// Serial commands (one line each, 115200 baud):
//   ! msg  attention   > msg  busy   = msg  ready   x msg  error   . msg  idle
//
// Screen layout (pet-card style):
//   +----------------------------+
//   | CLABOT               LV.01 |  header
//   |      (o o)  <- round bot   |  play area: white body, dark face,
//   |       \_/      with feet   |  RoboEyes + curvy mouth, stars
//   | ENERGY [######  ]          |  stats panel
//   | HAPPY                      |  mood word
//   | (i) Beep! I'm ready...     |  speech bubble
//   +----------------------------+

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "RoboEyes_TFT.h"
#include "RoboMouth_TFT.h"

#define TFT_CS  10
#define TFT_RST  9
#define TFT_DC   8
#define BTN_PIN  3   // test button to GND (INPUT_PULLUP); each press cycles states
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

RoboEyes<Adafruit_ST7735> eyes(tft);
RoboMouth<Adafruit_ST7735> mouth(tft);

// ---- colors ----
#define C_BG    0x0000    // black, like the reference card
#define C_FACE  0x0861    // dark teal face screen
#define C_WHITE 0xFFFF
#define C_CYAN  0x07FF
#define C_DIM   0x0339    // dim teal for frames and stars
#define C_RED   0xF800
#define C_AMBER 0xFD20
#define C_GREEN 0x07E0

// ---- layout ----
#define EYES_X   36
#define EYES_Y   30
#define EYES_W   56
#define EYES_H   28
#define MOUTH_CX 64
#define MOUTH_CY 70

enum { S_IDLE, S_BUSY, S_READY, S_ATTN, S_ERR };

// Idle micro-moods, cycled over time so the bot feels alive.
enum { SC_WAIT, SC_LOOK, SC_HAPPY, SC_THINK, SC_IMPATIENT, SC_SLEEPY };
struct Scene { uint16_t dur; uint8_t kind; };
const Scene IDLE_SCENES[] = {
  { 9000, SC_WAIT }, { 8000, SC_LOOK }, { 6000, SC_HAPPY }, { 9000, SC_THINK },
  { 7000, SC_IMPATIENT }, { 9000, SC_WAIT }, { 11000, SC_SLEEPY }, { 7000, SC_LOOK },
};
const uint8_t SCENE_COUNT = sizeof(IDLE_SCENES) / sizeof(IDLE_SCENES[0]);

// Per-scene mood word and default bubble message.
const char* const SCENE_MOOD[6] = { "CALM", "CURIOUS", "HAPPY", "THINKING", "BORED", "SLEEPY" };
const char* const SCENE_MSG[6] = {
  "Beep. All systems go",
  "Ooh, what's that?",
  "Beep! Happy to help!",
  "Hmm, pondering...",
  "Anything to do yet?",
  "Zzz... zzz... zzz",
};

// Per-state mood word and default bubble message (S_BUSY..S_ERR).
const char* const STATE_MOOD[4] = { "WORKING", "HAPPY", "ALERT!", "ERROR" };
const char* const STATE_MSG[4] = {
  "Crunching bits...",
  "Done! Your turn!",
  "Human needed here!",
  "Ouch! Hit an error.",
};

// Header action word per state (S_IDLE..S_ERR)
const char* const STATE_ACTION[5] = { "IDLE", "BUSY", "READY", "ATTENTION", "ERROR" };

uint8_t state = S_IDLE;
uint8_t sceneIdx = 255;
unsigned long sceneStart = 0;
unsigned long attnShakeTimer = 0;
unsigned long readyStart = 0;
const unsigned long READY_TIMEOUT_MS = 10000;   // READY falls back to IDLE after this
unsigned long attnStart = 0;
const unsigned long ATTN_TIMEOUT_MS = 15000;    // ATTENTION falls back to IDLE after this

char msg[22] = "";                 // last message from the terminal/daemon
char lineBuf[28];
uint8_t lineLen = 0;

// Dirty-draw caches
char bubblePrev[22] = "\x01";
char userPrev[22] = "\x01";
char moodPrev[10] = "\x01";
uint16_t moodPrevCol = 0;
const char* actionPrev = nullptr;
uint16_t lastBorder = 1;

void resetFace() {
  eyes.setWidth(16, 16);
  eyes.setHeight(20, 20);
  eyes.setBorderradius(7, 7);      // tall rounded ovals, like the reference
  eyes.setSpacebetween(8);
  eyes.setMood(DEFAULT);
  eyes.setPosition(DEFAULT);
  eyes.setCuriosity(OFF);          // face is small; growing eyes overflow it
  eyes.setIdleMode(OFF);
  eyes.setAutoblinker(ON, 2, 3);
  eyes.open();
  mouth.setColor(C_CYAN);
}

void enterScene(uint8_t kind) {
  resetFace();
  switch (kind) {
    case SC_LOOK:      eyes.setIdleMode(ON, 1, 2);  mouth.setShape(MOUTH_SMILE); break;
    case SC_HAPPY:     eyes.setMood(HAPPY); eyes.anim_laugh(); mouth.setShape(MOUTH_GRIN); break;
    case SC_THINK:     eyes.setPosition(NE);        mouth.setShape(MOUTH_DOTS);  break;
    case SC_IMPATIENT: eyes.setMood(SUSPICIOUS);    mouth.setShape(MOUTH_FLAT);  break;
    case SC_SLEEPY:    eyes.setMood(TIRED); eyes.setPosition(S);
                       eyes.setAutoblinker(ON, 1, 1); mouth.setShape(MOUTH_FLAT); break;
    default:           mouth.setShape(MOUTH_SMILE); break;   // SC_WAIT
  }
}

void enterState() {
  resetFace();
  switch (state) {
    case S_BUSY:
      mouth.setShape(MOUTH_TALK);  mouth.setColor(C_AMBER);
      break;
    case S_READY:
      eyes.setMood(HAPPY); eyes.anim_laugh();
      mouth.setShape(MOUTH_GRIN);  mouth.setColor(C_GREEN);
      readyStart = millis();
      break;
    case S_ATTN:
      eyes.setWidth(20, 20); eyes.setHeight(24, 24);   // wide startled eyes
      eyes.anim_confused();
      mouth.setShape(MOUTH_O);     mouth.setColor(C_RED);
      attnShakeTimer = millis();
      attnStart = attnShakeTimer;
      break;
    case S_ERR:
      eyes.setMood(ANGRY); eyes.setAutoblinker(OFF);   // frozen glare
      mouth.setShape(MOUTH_ZIGZAG); mouth.setColor(C_RED);
      break;
    default:
      sceneIdx = 255;              // hand over to the idle scene machine
      break;
  }
}

void tickState(unsigned long now) {
  switch (state) {
    case S_IDLE:
      if (sceneIdx == 255 || now - sceneStart >= IDLE_SCENES[sceneIdx].dur) {
        sceneIdx = (sceneIdx == 255) ? 0 : (sceneIdx + 1) % SCENE_COUNT;
        sceneStart = now;
        enterScene(IDLE_SCENES[sceneIdx].kind);
      }
      if (IDLE_SCENES[sceneIdx].kind == SC_IMPATIENT) {
        eyes.setPosition(((now / 700) & 1) ? E : W);   // side glances (calm pace)
      }
      break;
    case S_BUSY:
      eyes.setPosition(((now / 900) & 1) ? E : W);     // scanning while working
      break;
    case S_READY:
      if (now - readyStart >= READY_TIMEOUT_MS) {      // back to idle, like first boot
        state = S_IDLE;
        msg[0] = '\0';
        enterState();
      }
      break;
    case S_ATTN:
      if (now - attnStart >= ATTN_TIMEOUT_MS) {        // back to idle, like first boot
        state = S_IDLE;
        msg[0] = '\0';
        enterState();
        break;
      }
      if (now - attnShakeTimer >= 3000) {              // gentle periodic nudge
        eyes.anim_confused();
        attnShakeTimer = now;
      }
      break;
    default:
      break;
  }
}

void handleLine(char* s) {
  uint8_t prevState = state;
  char cmd = s[0];
  char* m = s + 1;
  if (*m == ' ') m++;
  switch (cmd) {
    case '!': state = S_ATTN;  break;
    case '>': state = S_BUSY;  break;
    case '=': state = S_READY; break;
    case 'x': state = S_ERR;   break;
    case '.': state = S_IDLE;  break;
    default: return;
  }
  strncpy(msg, m, 21); msg[21] = '\0';
  if (state != prevState) enterState();
}

// Test button: every press feeds the next state through handleLine(),
// exactly as if it arrived over serial.
void pollButton(unsigned long now) {
  static const char TEST_CMD[5] = { '.', '>', '=', '!', 'x' };
  static const char* const TEST_MSG[5] =
    { "idle test", "busy test", "ready test", "attention test", "error test" };
  static uint8_t testIdx = 0;
  static bool prevDown = false;
  static unsigned long lastPress = 0;

  bool down = digitalRead(BTN_PIN) == LOW;
  if (down && !prevDown && now - lastPress > 250) {   // debounce
    lastPress = now;
    char line[24];
    line[0] = TEST_CMD[testIdx];
    line[1] = ' ';
    strcpy(line + 2, TEST_MSG[testIdx]);
    handleLine(line);
    testIdx = (testIdx + 1) % 5;
  }
  prevDown = down;
}

void pollSerial() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (lineLen > 0) { lineBuf[lineLen] = '\0'; handleLine(lineBuf); lineLen = 0; }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = ch;
    }
  }
}

//*********************************************************************
//  Card UI
//*********************************************************************

void drawStaticUI() {
  tft.fillScreen(C_BG);

  // Header
  tft.drawRoundRect(2, 2, 124, 15, 3, C_DIM);
  tft.setTextSize(1);
  tft.setTextColor(C_CYAN);
  tft.setCursor(8, 6);
  tft.print("CLABOT");

  // Play area: just stars — the face floats on the dark background
  static const uint8_t stars[6][2] = { {16,26}, {108,32}, {12,62}, {114,70}, {24,88}, {102,24} };
  for (uint8_t i = 0; i < 6; i++) tft.fillRect(stars[i][0], stars[i][1], 2, 2, C_DIM);

  // Emotion panel (big centered mood word)
  tft.drawRoundRect(2, 98, 124, 34, 3, C_DIM);

  // Speech bubble with a mini bot icon
  tft.drawRoundRect(2, 134, 124, 24, 4, C_DIM);
  tft.fillRoundRect(6, 139, 16, 14, 5, C_WHITE);
  tft.fillRect(10, 143, 2, 4, C_FACE);
  tft.fillRect(16, 143, 2, 4, C_FACE);
}

void drawHeaderAction() {
  const char* word = STATE_ACTION[state];
  if (word == actionPrev) return;
  uint16_t col;
  switch (state) {
    case S_BUSY:  col = C_AMBER; break;
    case S_READY: col = C_GREEN; break;
    case S_ATTN:
    case S_ERR:   col = C_RED;   break;
    default:      col = C_CYAN;  break;
  }
  tft.fillRect(56, 4, 68, 10, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(col);
  tft.setCursor(122 - 6 * (int)strlen(word), 6);   // right-aligned
  tft.print(word);
  actionPrev = word;
}

void drawMood() {
  const char* word;
  uint16_t col;
  switch (state) {
    case S_BUSY:  word = STATE_MOOD[0]; col = C_AMBER; break;
    case S_READY: word = STATE_MOOD[1]; col = C_GREEN; break;
    case S_ATTN:  word = STATE_MOOD[2]; col = C_RED;   break;
    case S_ERR:   word = STATE_MOOD[3]; col = C_RED;   break;
    default:      word = SCENE_MOOD[IDLE_SCENES[sceneIdx].kind]; col = C_CYAN; break;
  }
  if (strcmp(word, moodPrev) == 0 && col == moodPrevCol) return;
  tft.fillRect(6, 101, 116, 9, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(col);
  tft.setCursor(8, 102);
  tft.print(word);
  strcpy(moodPrev, word); moodPrevCol = col;
}

// The message you sent from the terminal, shown in the middle panel.
void drawUserMsg() {
  if (strcmp(msg, userPrev) == 0) return;
  tft.fillRect(6, 111, 116, 19, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_AMBER);
  tft.setCursor(8, 112);
  tft.print("> ");
  uint8_t len = strlen(msg);
  for (uint8_t i = 0; i < len && i < 17; i++) tft.print(msg[i]);
  if (len > 17) {
    tft.setCursor(20, 121);
    for (uint8_t i = 17; i < len; i++) tft.print(msg[i]);
  }
  strcpy(userPrev, msg);
}

// The bot's own per-expression phrase, always in the bottom bubble.
void drawBubble() {
  const char* text;
  if (state == S_IDLE) text = SCENE_MSG[IDLE_SCENES[sceneIdx].kind];
  else text = STATE_MSG[state - 1];
  if (strcmp(text, bubblePrev) == 0) return;
  tft.fillRect(24, 137, 100, 19, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  uint8_t len = strlen(text);
  tft.setCursor(26, 138);
  for (uint8_t i = 0; i < len && i < 16; i++) tft.print(text[i]);
  if (len > 16) {
    tft.setCursor(26, 148);
    for (uint8_t i = 16; i < len; i++) tft.print(text[i]);
  }
  strcpy(bubblePrev, text);
}

void drawBorder(unsigned long now) {
  uint16_t bc = C_BG;
  if (state == S_ATTN) bc = ((now / 300) & 1) ? C_RED : C_BG;
  if (bc != lastBorder) {
    tft.drawRect(0, 0, 128, 160, bc);
    lastBorder = bc;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);
  tft.initR(INITR_BLACKTAB);   // try INITR_GREENTAB if the screen looks wrong
  tft.setRotation(0);

  drawStaticUI();

  eyes.setDisplayColors(C_BG, C_CYAN);
  eyes.setOrigin(EYES_X, EYES_Y);
  eyes.begin(EYES_W, EYES_H, 50);
  mouth.begin(MOUTH_CX, MOUTH_CY, C_BG, C_CYAN);

  enterState();   // S_IDLE -> scene machine
  tickState(millis());
}

void loop() {
  pollSerial();
  unsigned long now = millis();
  pollButton(now);

  tickState(now);
  eyes.update();
  mouth.update();

  drawHeaderAction();
  drawMood();
  drawUserMsg();
  drawBubble();
  drawBorder(now);

  delay(10);
}
