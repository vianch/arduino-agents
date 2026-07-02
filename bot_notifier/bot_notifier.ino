// bot_notifier.ino — a robot "pet" that shows Claude Code session state.
// Hardware: Arduino Nano/Uno + ST7735S 128x160 SPI TFT. Protocol: README.md.
//
// Serial commands (one line each, 115200 baud):
//   ! msg  attention   > msg  busy   = msg  ready   x msg  error   . msg  idle

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <avr/pgmspace.h>

#define TFT_CS  10
#define TFT_RST  9
#define TFT_DC   8
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Sprite grid: the bot is SW x SH cells drawn inside a GW x GH grid so it can
// move vertically without repainting the whole screen. One cell = SCALE px.
#define SW 17
#define SH 14
#define GW 17
#define GH 18
#define SCALE 7        // GW*SCALE must stay <= 128
#define BASE_TOP 3     // resting row inside the grid

// Animation timing (ms). Periods are co-prime-ish so motion never syncs up.
#define BLINK_PERIOD  2800
#define BLINK_DUR      140
#define GLANCE_PERIOD 4200   // idle: eyes wander slowly
#define SCAN_STEP      600   // busy: eyes sweep left-right
#define BOUNCE_DUR     420
#define BOUNCE_H         3
#define BOB_MS         500

#define STATUS_H 16
#define MSG_Y   146
#define MSG_H    14

#define C_BG    0x39AA
#define C_RED   0xF800
#define C_AMBER 0xFD20
#define C_GREEN 0x07E0
#define C_WHITE 0xFFFF
#define C_CYAN  0x07FF

enum { S_IDLE, S_BUSY, S_READY, S_ATTN, S_ERR };

// Palette indices for grid cells.
enum { P_BG, P_DARK, P_BODY, P_SCREEN, P_WHITE, P_CYAN, P_RED };
const uint16_t PAL[7] = { C_BG, 0x2124, 0xBDD7, 0x0861, C_WHITE, C_CYAN, C_RED };

uint8_t ci(char ch) {
  switch (ch) {
    case 'd': return P_DARK;   case 'b': return P_BODY;
    case 's': return P_SCREEN; case 'w': return P_WHITE;
    default:  return P_BG;
  }
}

// Legend: . background, d dark outline, b body, s face screen, w white.
// The face (eyes + mouth) is drawn onto the 's' area every frame.
const char BOT[SH][SW + 1] PROGMEM = {
  "........w........",
  "........d........",
  "...ddddddddddd...",
  "..dbbbbbbbbbbbd..",
  ".dbsssssssssssbd.",
  "ddbsssssssssssbdd",
  "ddbsssssssssssbdd",
  ".dbsssssssssssbd.",
  ".dbsssssssssssbd.",
  ".dbsssssssssssbd.",
  "..dbbbbbbbbbbbd..",
  "...ddddddddddd...",
  "....dd.....dd....",
  "................."
};

// Face geometry, sprite-relative (inside the 's' screen: rows 4-9, cols 3-13).
#define EYE_ROW    4
#define EYE_L_COL  4
#define EYE_R_COL 10
#define MOUTH_ROW  7
#define MOUTH_COL  5

// Eyes: 3x3 cells, one byte per row, bit 2 = leftmost cell.
const uint8_t EYE_OPEN[3]   = { 0b111, 0b111, 0b111 };
const uint8_t EYE_HAPPY[3]  = { 0b010, 0b101, 0b000 };  // ^ ^
const uint8_t EYE_CLOSED[3] = { 0b000, 0b000, 0b111 };
const uint8_t EYE_X[3]      = { 0b101, 0b010, 0b101 };  // x x

// Mouths: 7x2 cells, one byte per row, bit 6 = leftmost cell.
const uint8_t MOUTH_SMILE[2] = { 0b1000001, 0b0111110 };
const uint8_t MOUTH_GRIN[2]  = { 0b1111111, 0b0111110 };  // open happy mouth
const uint8_t MOUTH_FLAT[2]  = { 0b0000000, 0b0111110 };
const uint8_t MOUTH_O[2]     = { 0b0011100, 0b0011100 };  // surprised
const uint8_t MOUTH_FROWN[2] = { 0b0111110, 0b1000001 };

uint8_t canvas[GH][GW];
uint8_t prevc[GH][GW];
int originX, originY;

uint8_t state = S_IDLE;
char msg[22] = "";
char lastMsg[22] = "\x01";
char stTxt[16] = "", stPrev[16] = "\x01";
uint16_t stCol = 0, stPrevCol = 1;
uint16_t lastBorder = 1;

char lineBuf[28];
uint8_t lineLen = 0;

inline void putCell(int r, int c, uint8_t v) {
  if ((unsigned)r < GH && (unsigned)c < GW) canvas[r][c] = v;
}

void drawEye(int top, int col, const uint8_t rows[3], uint8_t color) {
  for (uint8_t r = 0; r < 3; r++)
    for (uint8_t c = 0; c < 3; c++)
      if ((rows[r] >> (2 - c)) & 1) putCell(top + EYE_ROW + r, col + c, color);
}

void drawMouth(int top, const uint8_t rows[2], uint8_t color) {
  for (uint8_t r = 0; r < 2; r++)
    for (uint8_t c = 0; c < 7; c++)
      if ((rows[r] >> (6 - c)) & 1) putCell(top + MOUTH_ROW + r, MOUTH_COL + c, color);
}

void applyFace(int top, unsigned long ms) {
  const uint8_t* eye   = EYE_OPEN;
  const uint8_t* mouth = MOUTH_SMILE;
  uint8_t color = P_CYAN;
  int8_t look = 0;                              // eye x-offset: -1, 0, +1
  static const int8_t SWEEP[4] = { -1, 0, 1, 0 };

  switch (state) {
    case S_BUSY:  look = SWEEP[(ms / SCAN_STEP) & 3];     mouth = MOUTH_FLAT;  break;
    case S_READY: eye = EYE_HAPPY;                        mouth = MOUTH_GRIN;  break;
    case S_ATTN:  color = P_WHITE;                        mouth = MOUTH_O;     break;
    case S_ERR:   eye = EYE_X; color = P_RED;             mouth = MOUTH_FROWN; break;
    default:      look = SWEEP[(ms / GLANCE_PERIOD) & 3];                      break;
  }

  // Only round open eyes blink; attention eyes stay wide, X/happy never blink.
  if (eye == EYE_OPEN && state != S_ATTN && (ms % BLINK_PERIOD) < BLINK_DUR)
    eye = EYE_CLOSED;

  drawEye(top, EYE_L_COL + look, eye, color);
  drawEye(top, EYE_R_COL + look, eye, color);
  drawMouth(top, mouth, color);
}

void buildCanvas(int top, unsigned long ms) {
  memset(canvas, P_BG, sizeof(canvas));
  for (uint8_t r = 0; r < SH; r++)
    for (uint8_t c = 0; c < SW; c++) {
      uint8_t v = ci((char)pgm_read_byte(&BOT[r][c]));
      if (v) putCell(top + r, c, v);
    }
  applyFace(top, ms);
}

void renderDiff() {
  for (uint8_t r = 0; r < GH; r++)
    for (uint8_t c = 0; c < GW; c++)
      if (canvas[r][c] != prevc[r][c]) {
        tft.fillRect(originX + c * SCALE, originY + r * SCALE, SCALE, SCALE, PAL[canvas[r][c]]);
        prevc[r][c] = canvas[r][c];
      }
}

void handleLine(char* s) {
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

void computeStatus(unsigned long ms) {
  switch (state) {
    case S_ATTN: {
      if (((ms / 300) & 1) == 0) strcpy(stTxt, "ATTENTION!"); else stTxt[0] = 0;
      stCol = C_RED; break;
    }
    case S_BUSY: {
      uint8_t dots = (ms / 400) % 4;
      strcpy(stTxt, "WORKING");
      for (uint8_t i = 0; i < dots; i++) stTxt[7 + i] = '.';
      stTxt[7 + dots] = 0; stCol = C_AMBER; break;
    }
    case S_READY: strcpy(stTxt, "YOUR TURN"); stCol = C_GREEN; break;
    case S_ERR:   strcpy(stTxt, "ERROR");     stCol = C_RED;   break;
    default:      stTxt[0] = 0;               stCol = C_BG;    break;
  }
}

void drawStatus() {
  if (strcmp(stTxt, stPrev) != 0 || stCol != stPrevCol) {
    tft.fillRect(0, 0, 128, STATUS_H, C_BG);
    if (stTxt[0]) {
      tft.setTextSize(1); tft.setTextColor(stCol);
      tft.setCursor((128 - (int)strlen(stTxt) * 6) / 2, 4);
      tft.print(stTxt);
    }
    strcpy(stPrev, stTxt); stPrevCol = stCol;
  }
}

void drawBorder(unsigned long ms) {
  uint16_t bc = C_BG;
  if (state == S_ATTN) bc = ((ms / 300) & 1) ? C_RED : C_BG;
  if (bc != lastBorder) {
    tft.drawRect(0, 0, 128, 160, bc);
    tft.drawRect(1, 1, 126, 158, bc);
    lastBorder = bc;
  }
}

void drawMsg() {
  if (strcmp(msg, lastMsg) != 0) {
    tft.fillRect(0, MSG_Y, 128, MSG_H, C_BG);
    tft.setTextSize(1); tft.setTextColor(C_WHITE);
    tft.setCursor(2, MSG_Y + 3);
    tft.print(msg);
    strcpy(lastMsg, msg);
  }
}

void setup() {
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);   // try INITR_GREENTAB if the screen looks wrong
  tft.setRotation(0);
  originX = (128 - GW * SCALE) / 2;
  originY = 18;
  tft.fillScreen(C_BG);
  memset(prevc, 255, sizeof(prevc));   // force full first paint
}

void loop() {
  pollSerial();
  unsigned long ms = millis();

  unsigned int bouncePeriod;   // 0 = still
  switch (state) {
    case S_BUSY:  bouncePeriod = 1500; break;
    case S_READY: bouncePeriod = 3000; break;
    case S_ATTN:  bouncePeriod = 900;  break;
    case S_ERR:   bouncePeriod = 0;    break;
    default:      bouncePeriod = 6000; break;
  }

  unsigned long phase = bouncePeriod ? ms % bouncePeriod : BOUNCE_DUR;
  int lift = 0;
  if (phase < BOUNCE_DUR) {
    // integer triangle wave 0 -> BOUNCE_H -> 0 (no float sin needed)
    unsigned int half = BOUNCE_DUR / 2;
    lift = (phase < half) ? (int)(BOUNCE_H * phase / half)
                          : (int)(BOUNCE_H * (BOUNCE_DUR - phase) / half);
  }
  int bob = (lift == 0 && bouncePeriod) ? (int)((ms / BOB_MS) & 1) : 0;
  int top = BASE_TOP + bob - lift;

  buildCanvas(top, ms);
  renderDiff();
  computeStatus(ms);
  drawStatus();
  drawBorder(ms);
  drawMsg();

  delay(30);
}
