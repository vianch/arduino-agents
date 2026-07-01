# Frog Notifier

A pixel-art frog on a small TFT display, driven by an Arduino Nano, that shows the state of a Claude Code session running on your computer. Your machine pushes one-line status messages over USB serial; the frog changes behavior and color, flashing red when Claude Code needs your attention.

The board cannot poll the computer, so communication is one-directional: the computer writes, the firmware reads and reacts.

## Hardware

- Arduino Nano (ATmega328P) — an Uno works identically.
- ST7735S TFT, 1.8" 128x160, SPI.
- Jumper wires. Optionally a logic level shifter (see the Logic level section).
- USB cable to the computer.

## Wiring the display

The wiring is identical on the Arduino Uno and the Arduino Nano: the firmware drives the same pins on both boards. The only physical difference is where the `SCK` pin (D13) sits — see the per-board notes below.

**Arduino Uno**

![ST7735S to Arduino Uno wiring](assets/wiring-uno.svg)

**Arduino Nano**

![ST7735S to Arduino Nano wiring](assets/wiring-nano.svg)

### Connections

| ST7735S pin | Arduino pin (Uno & Nano) | Note |
|---|---|---|
| VCC | 5V (regulator board) or 3.3V (bare) | check your board |
| GND | GND | shared with the Arduino |
| CS | D10 | configurable in firmware |
| RESET / RST | D9 | configurable |
| A0 / DC / RS | D8 | configurable |
| SDA / MOSI | D11 | hardware SPI, fixed |
| SCK | D13 | hardware SPI, fixed |
| LED / BL | 3.3V | most boards have a series resistor onboard |

`SCK` and `MOSI` are fixed hardware-SPI pins. `CS`, `RESET`, and `A0/DC` are defined in the firmware and can be changed.

### Pin labels

Cheap boards silkscreen the same pins under different names:

- `SDA` = `MOSI` = `DIN` → MOSI
- `SCK` = `SCL` = `CLK` → SCK
- `A0` = `DC` = `RS` → Data/Command
- `RESET` = `RST`
- `CS` = `CE` → Chip Select
- `LED` = `BL` = `BLK` → Backlight
- `MISO` / `SDO` → leave unconnected

### Logic level

The ST7735S controller runs at **3.3V logic**; the Uno and Nano output **5V logic**.

- Boards with an onboard AMS1117 regulator (most 1.8" red/black PCBs): `VCC` accepts 5V, but the SPI/control lines still feed the 3.3V controller directly. Driving them at 5V usually works but is out of spec.
- Bare boards without a regulator: `VCC` must be **3.3V only**.

Safe option on a 5V board: put a level shifter (or 1kΩ series + 2kΩ-to-GND dividers) on the lines going *into* the display — SCK, MOSI, CS, DC, RST. The display sends nothing back, so MISO is unused. Many people wire directly on a regulator board and accept the risk.

### Finding D13 (SCK)

- **Uno:** D13 is in the digital header alongside D0–D12, at the end nearest the USB connector.
- **Nano:** D13 is *not* next to D12. D2–D12 run down one long side; D13 sits at the top of the *other* side (the row with A0–A7 and 3V3), nearest the USB connector. It may be silkscreened `D13`, `13`, or `SCK`. The 2x3 ICSP header near the USB end also exposes SCK/MOSI.

SCK is fixed to D13 in hardware; it cannot be moved to D12 without switching to slower software SPI.

### Backlight

If there is no current-limiting resistor near the LED pin, add ~50–100Ω in series. Connecting LED to 3.3V is the safe default.

## States

| Command | State | Display |
|---|---|---|
| `!` | Attention — Claude needs you | Flashing red border, `ATTENTION!`, frantic hopping |
| `>` | Busy — working | Amber `WORKING...`, fast hops |
| `=` | Ready — your turn | Green `YOUR TURN`, calm hops, croaks |
| `x` | Error | Red `ERROR`, still |
| `.` | Idle | Cleared, slow idle bob |

Each command is followed by optional text shown in the message bar at the bottom of the screen (max 21 characters).

## Files

- `frog_notifier.ino` — Arduino firmware (full source in the appendix below).
- `frogd.py` — serial daemon; holds the port open and forwards messages.
- `frog-hook.sh` — Claude Code hook script; translates hook events into frog commands.
- `wiring-uno.svg`, `wiring-nano.svg` — wiring diagrams shown above.

## Firmware installation

1. Install the display libraries. Arduino IDE → Tools → Manage Libraries → search `Adafruit ST7735` → install **Adafruit ST7735 and ST7789 Library**. Accept the prompt to install dependencies (**Adafruit GFX Library**, **Adafruit BusIO**). If no prompt appears, install `Adafruit GFX` and `Adafruit BusIO` manually. `Adafruit_GFX.h` lives in the GFX library — that is the header the compiler needs.
2. Confirm library location if a `No such file or directory` error persists: File → Preferences → "Sketchbook location" contains a `libraries` folder where `Adafruit_GFX_Library`, `Adafruit_ST7735_and_ST7789_Library`, and `Adafruit_BusIO` must appear as directories.
3. Open `frog_notifier.ino`.
4. Tools → Board → **Arduino Nano** (or Arduino Uno). On older Nano clones, Tools → Processor → **ATmega328P (Old Bootloader)**.
5. Tools → Port → select the Nano's serial port.
6. Upload.

If the screen shows nothing or looks wrong after upload, see Troubleshooting.

## Protocol

One line per message. The first character is the command; the rest is the message text.

```
! approve rm -rf node_modules?
> building portal
= tests passed
x build failed
.
```

Serial runs at **115200 baud**. Text is truncated to 21 characters on screen.

## Daemon installation

The Nano reboots whenever its serial port is opened (DTR toggles auto-reset). If every message opened the port, the board would reset on every Claude Code hook. The daemon opens the port **once** and stays connected; your terminal and the hooks write to a named pipe (FIFO) that the daemon forwards. No hardware modification is required.

Install pyserial:

```bash
pip3 install pyserial
```

Run the daemon (leave it running):

```bash
python3 frogd.py &
```

It auto-detects common USB-serial device names. If it picks the wrong one, find the device with `ls /dev/cu.*` and set it explicitly:

```bash
FROG_PORT=/dev/cu.wchusbserial14230 python3 frogd.py &
```

The pipe path defaults to `/tmp/frog.pipe`; override with `FROG_PIPE` if needed.

## Terminal usage

With the daemon running, write lines to the pipe:

```bash
echo "> building portal"                 > /tmp/frog.pipe
echo "! approve rm -rf node_modules?"    > /tmp/frog.pipe
echo "= tests passed"                    > /tmp/frog.pipe
echo "."                                 > /tmp/frog.pipe
```

Writing to the pipe when no reader is connected will block the terminal until a reader appears. That blocking is the signal that the daemon is not running.

## Claude Code integration

Save the hook script as `~/bin/frog-hook.sh` and make it executable:

```bash
chmod +x ~/bin/frog-hook.sh
```

Add the hooks to `~/.claude/settings.json`:

```json
{
  "hooks": {
    "Notification":     [ { "hooks": [ { "type": "command", "command": "~/bin/frog-hook.sh '!'" } ] } ],
    "UserPromptSubmit": [ { "hooks": [ { "type": "command", "command": "~/bin/frog-hook.sh '>'" } ] } ],
    "Stop":             [ { "hooks": [ { "type": "command", "command": "~/bin/frog-hook.sh '='" } ] } ],
    "SubagentStop":     [ { "hooks": [ { "type": "command", "command": "~/bin/frog-hook.sh '='" } ] } ],
    "SessionStart":     [ { "hooks": [ { "type": "command", "command": "~/bin/frog-hook.sh '.'" } ] } ]
  }
}
```

- `Notification` fires when Claude Code needs permission for a tool or is idle waiting on you — this drives the flashing-red attention state.
- `UserPromptSubmit` marks the frog busy.
- `Stop` / `SubagentStop` mark your turn.
- `SessionStart` resets to idle.

Hook event names and the stdin JSON field names change between Claude Code releases. Verify them against your installed version; the `message` / `prompt` keys the script reads must match what your version emits.

## Startup order

1. Flash the firmware.
2. Start `frogd.py`.
3. Confirm a manual `echo "= hi" > /tmp/frog.pipe` moves the frog.
4. Restart your Claude Code session so it reloads `settings.json`.

## Stopping the daemon

Foreground (holding the terminal): `Ctrl-C`.

Backgrounded with `&` in the current shell: `kill %1` (check the job number with `jobs`).

From any other terminal, by name:

```bash
pkill -f frogd.py
```

`-f` matches the full command line because the process name is `python3`, not `frogd.py`. Confirm it is gone:

```bash
pgrep -f frogd.py    # no output means it is dead
```

If it ignores a normal kill:

```bash
pkill -9 -f frogd.py
```

The FIFO at `/tmp/frog.pipe` persists after the daemon exits and is harmless. Remove it if you want:

```bash
rm -f /tmp/frog.pipe
```

## Troubleshooting

- **`Adafruit_GFX.h: No such file or directory`** — GFX library not installed or installed outside the sketchbook `libraries` folder. See Firmware installation steps 1–2.
- **Blank white screen, backlight on** — wrong init constant. In `setup()`, replace `INITR_BLACKTAB` with `INITR_GREENTAB`, then `INITR_REDTAB`.
- **Colors inverted or red/blue swapped** — add `tft.invertDisplay(true);` after init, or change the tab constant.
- **Image shifted a few pixels at the edges** — green-vs-black tab mismatch; change the tab constant.
- **Random noise / garbage** — SCK or MOSI miswired, RST floating, or 5V feeding a bare 3.3V board.
- **Nothing at all, backlight off** — VCC or LED pin not powered; confirm GND is shared with the Nano.
- **Works intermittently** — long jumper wires degrade SPI; shorten them.
- **`frogd: no serial port found`** — set `FROG_PORT` explicitly (`ls /dev/cu.*`).
- **Terminal hangs on `echo ... > /tmp/frog.pipe`** — the daemon is not running; start it.
- **Frog does not react to Claude Code** — restart the Claude session after editing `settings.json`; verify hook event names for your version.

## Tuning (firmware)

- `SCALE` — frog size. `17 * SCALE` must stay ≤ 128.
- `BASE_TOP` — resting height in the grid.
- `BLINK_PERIOD` / `BLINK_DUR`, `CROAK_PERIOD` / `CROAK_DUR`, `HOP_DUR`, `HOP_H`, `BOB_MS` — timing of each behavior, independent so motion does not look mechanical.
- Per-state hop speed and croaking are set in the `switch (state)` block in `loop()`.
- Frog shape: edit the `FROG[]` rows (each exactly 17 characters). Legend: `.` background, `k` black, `g` green, `G` dark green, `w` white, `l` light green. Colors live in `PAL[]`.
- Status/message colors: `C_RED`, `C_AMBER`, `C_GREEN`, `C_WHITE`, `C_BG`.

RAM use: two 17x18 framebuffers, ~612 bytes; sprite art is stored in flash.

---

## Appendix A — `frog_notifier.ino`

```cpp
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <avr/pgmspace.h>

#define TFT_CS 10
#define TFT_RST 9
#define TFT_DC  8
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define SW 17
#define SH 14
#define GW 17
#define GH 18
#define SCALE 7
#define BASE_TOP 3

#define BLINK_PERIOD 2800
#define BLINK_DUR    140
#define CROAK_PERIOD 3600
#define CROAK_DUR    500
#define HOP_DUR      420
#define HOP_H        3
#define BOB_MS       500

#define STATUS_H 16
#define MSG_Y    146
#define MSG_H    14

#define C_BG    0x39AA
#define C_RED   0xF800
#define C_AMBER 0xFD20
#define C_GREEN 0x07E0
#define C_WHITE 0xFFFF

enum { S_IDLE, S_BUSY, S_READY, S_ATTN, S_ERR };

const uint16_t PAL[6] = { 0x39AA, 0x0000, 0x65AB, 0x3B66, 0xFFFF, 0x968F };

uint8_t ci(char ch) {
  switch (ch) {
    case 'k': return 1; case 'g': return 2; case 'G': return 3;
    case 'w': return 4; case 'l': return 5; default: return 0;
  }
}

const char FROG[SH][SW+1] PROGMEM = {
  "....ww.....ww....",
  "...wkkw...wkkw...",
  "..gwwwwgggwwwwg..",
  "..ggggggggggggg..",
  "..gkgggggggggkg..",
  "..ggkkkkkkkkkgg..",
  ".ggggglllllggggg.",
  ".ggggglllllggggg.",
  "..ggggglllggggg..",
  "...ggggggggggg...",
  ".gg..ggggggg..gg.",
  ".GG...ggggg...GG.",
  ".G.G..G.G.G..G.G.",
  "................."
};

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

void applyBlink(int top) {
  static const uint8_t lid[][2] = {
    {0,4},{0,5},{0,11},{0,12},
    {1,3},{1,4},{1,5},{1,6},{1,10},{1,11},{1,12},{1,13},
    {2,3},{2,4},{2,5},{2,6},{2,10},{2,11},{2,12},{2,13}
  };
  for (uint8_t i = 0; i < 20; i++) putCell(top + lid[i][0], lid[i][1], 2);
  for (uint8_t c = 3; c <= 6;  c++) putCell(top + 1, c, 1);
  for (uint8_t c = 10; c <= 13; c++) putCell(top + 1, c, 1);
}

void applyCroak(int top) {
  for (uint8_t c = 5; c <= 11; c++) { putCell(top+6,c,5); putCell(top+7,c,5); }
  for (uint8_t c = 6; c <= 10; c++) putCell(top+8,c,5);
  for (uint8_t c = 7; c <= 9;  c++) putCell(top+9,c,5);
}

void buildCanvas(int top, bool blink, bool croak) {
  for (uint8_t r = 0; r < GH; r++)
    for (uint8_t c = 0; c < GW; c++) canvas[r][c] = 0;
  for (uint8_t r = 0; r < SH; r++)
    for (uint8_t c = 0; c < SW; c++) {
      uint8_t v = ci((char)pgm_read_byte(&FROG[r][c]));
      if (v) putCell(top + r, c, v);
    }
  if (croak) applyCroak(top);
  if (blink) applyBlink(top);
}

void renderDiff() {
  for (uint8_t r = 0; r < GH; r++)
    for (uint8_t c = 0; c < GW; c++)
      if (canvas[r][c] != prevc[r][c]) {
        tft.fillRect(originX + c*SCALE, originY + r*SCALE, SCALE, SCALE, PAL[canvas[r][c]]);
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
      if (((ms/300)&1)==0) strcpy(stTxt, "ATTENTION!"); else stTxt[0]=0;
      stCol = C_RED; break;
    }
    case S_BUSY: {
      uint8_t d = (ms/400)%4;
      strcpy(stTxt, "WORKING");
      for (uint8_t i=0;i<d;i++) stTxt[7+i]='.';
      stTxt[7+d]=0; stCol = C_AMBER; break;
    }
    case S_READY: strcpy(stTxt,"YOUR TURN"); stCol=C_GREEN; break;
    case S_ERR:   strcpy(stTxt,"ERROR");     stCol=C_RED;   break;
    default:      stTxt[0]=0;                stCol=C_BG;    break;
  }
}

void drawStatus() {
  if (strcmp(stTxt, stPrev)!=0 || stCol!=stPrevCol) {
    tft.fillRect(0,0,128,STATUS_H,C_BG);
    if (stTxt[0]) {
      tft.setTextSize(1); tft.setTextColor(stCol);
      tft.setCursor((128 - (int)strlen(stTxt)*6)/2, 4);
      tft.print(stTxt);
    }
    strcpy(stPrev, stTxt); stPrevCol = stCol;
  }
}

void drawBorder(unsigned long ms) {
  uint16_t bc = C_BG;
  if (state == S_ATTN) bc = ((ms/300)&1) ? C_RED : C_BG;
  if (bc != lastBorder) {
    tft.drawRect(0,0,128,160,bc);
    tft.drawRect(1,1,126,158,bc);
    lastBorder = bc;
  }
}

void drawMsg() {
  if (strcmp(msg, lastMsg)!=0) {
    tft.fillRect(0,MSG_Y,128,MSG_H,C_BG);
    tft.setTextSize(1); tft.setTextColor(C_WHITE);
    tft.setCursor(2, MSG_Y+3);
    tft.print(msg);
    strcpy(lastMsg, msg);
  }
}

void setup() {
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);   // try INITR_GREENTAB if the screen looks wrong
  tft.setRotation(0);
  originX = (128 - GW*SCALE)/2;
  originY = 18;
  tft.fillScreen(C_BG);
  for (uint8_t r = 0; r < GH; r++)
    for (uint8_t c = 0; c < GW; c++) prevc[r][c] = 255;
}

void loop() {
  pollSerial();
  unsigned long ms = millis();

  unsigned long hopPeriod;
  bool allowCroak;
  switch (state) {
    case S_BUSY:  hopPeriod=1500; allowCroak=false; break;
    case S_READY: hopPeriod=3000; allowCroak=true;  break;
    case S_ATTN:  hopPeriod=900;  allowCroak=false; break;
    case S_ERR:   hopPeriod=0;    allowCroak=false; break;
    default:      hopPeriod=6000; allowCroak=true;  break;
  }

  unsigned long hp = hopPeriod ? ms % hopPeriod : 1;
  bool hopping = hopPeriod && hp < HOP_DUR;
  int lift = hopping ? (int)lround(HOP_H * sin(PI * (double)hp / HOP_DUR)) : 0;
  int bob  = (!hopping && hopPeriod) ? (int)((ms / BOB_MS) & 1) : 0;
  int top  = BASE_TOP + bob - lift;

  bool blink = (ms % BLINK_PERIOD) < BLINK_DUR;
  bool croak = allowCroak && (ms % CROAK_PERIOD) < CROAK_DUR;

  buildCanvas(top, blink, croak);
  renderDiff();
  computeStatus(ms);
  drawStatus();
  drawBorder(ms);
  drawMsg();

  delay(30);
}
```

## Appendix B — `frogd.py`

```python
#!/usr/bin/env python3
# frogd.py — run once, leave it running
import os, sys, time, glob, serial

PIPE = os.environ.get("FROG_PIPE", "/tmp/frog.pipe")
BAUD = 115200

def find_port():
    if os.environ.get("FROG_PORT"):
        return os.environ["FROG_PORT"]
    cands = sorted(glob.glob("/dev/cu.usbserial*") +
                   glob.glob("/dev/cu.wchusbserial*") +
                   glob.glob("/dev/cu.usbmodem*"))
    return cands[0] if cands else None

def main():
    port = find_port()
    if not port:
        sys.exit("frogd: no serial port found (set FROG_PORT)")
    if not os.path.exists(PIPE):
        os.mkfifo(PIPE)
    ser = serial.Serial(port, BAUD, timeout=1)
    time.sleep(2)  # board reboots when the port opens
    print(f"frogd: {port} <- {PIPE}", flush=True)
    while True:
        with open(PIPE, "r") as fifo:      # blocks until a writer connects
            for line in fifo:
                line = line.strip()
                if line:
                    ser.write((line + "\n").encode())
                    ser.flush()
        # all writers closed; reopen

if __name__ == "__main__":
    main()
```

## Appendix C — `frog-hook.sh`

```bash
#!/usr/bin/env bash
# frog-hook.sh <prefix>   — reads hook JSON on stdin, forwards to the frog
PIPE="${FROG_PIPE:-/tmp/frog.pipe}"
msg="$(cat | python3 -c 'import sys,json
try:
    d=json.load(sys.stdin); print(d.get("message") or d.get("prompt") or "")
except Exception:
    print("")' 2>/dev/null)"
[ -p "$PIPE" ] && printf '%s %s\n' "$1" "${msg:0:21}" > "$PIPE"
```
