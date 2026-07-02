# Bot Notifier

A pixel-art robot on a small TFT display, driven by an Arduino Nano, that shows the state of a Claude Code session running on your computer. Your machine pushes one-line status messages over USB serial; the bot changes its facial expression and color, flashing red when Claude Code needs your attention.

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

## States and expressions

| Command | State | Face | Motion / extras |
|---|---|---|---|
| `!` | Attention — Claude needs you | Wide white eyes, surprised `O` mouth | Flashing red border, `ATTENTION!`, frantic bouncing |
| `>` | Busy — working | Eyes sweep left–right, flat mouth | Amber `WORKING...`, fast bounces |
| `=` | Ready — your turn | Happy `^ ^` eyes, big open grin | Green `YOUR TURN`, calm bounces |
| `x` | Error | Red `x x` eyes, frown | Red `ERROR`, completely still |
| `.` | Idle | Cyan eyes wandering slowly, small smile, blinks | Cleared, slow idle bob |

Each command is followed by optional text shown in the message bar at the bottom of the screen (max 21 characters).

## Files

- `bot_notifier/bot_notifier.ino` — Arduino firmware (Arduino IDE opens the folder as a sketch).
- `frogd.py` — serial daemon; holds the port open and forwards messages.
- `frog-hook.sh` — Claude Code hook script; translates hook events into bot commands.
- `assets/wiring-uno.svg`, `assets/wiring-nano.svg` — wiring diagrams shown above.
- `settings.json` — the hook block to merge into `~/.claude/settings.json`.

## Firmware installation

1. Install the display libraries. Arduino IDE → Tools → Manage Libraries → search `Adafruit ST7735` → install **Adafruit ST7735 and ST7789 Library**. Accept the prompt to install dependencies (**Adafruit GFX Library**, **Adafruit BusIO**). If no prompt appears, install `Adafruit GFX` and `Adafruit BusIO` manually. `Adafruit_GFX.h` lives in the GFX library — that is the header the compiler needs.
2. Confirm library location if a `No such file or directory` error persists: File → Preferences → "Sketchbook location" contains a `libraries` folder where `Adafruit_GFX_Library`, `Adafruit_ST7735_and_ST7789_Library`, and `Adafruit_BusIO` must appear as directories.
3. Open `bot_notifier/bot_notifier.ino`.
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

The Nano reboots whenever its serial port is opened (DTR toggles auto-reset). If every message opened the port, the board would reset on every Claude Code hook. The daemon opens the port **once** and stays connected; your terminal and the hooks write to a named pipe (FIFO) that the daemon forwards. No hardware modification is required. If the board is unplugged, the daemon waits and reconnects when it reappears.

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

With the daemon running, write lines to the pipe. All five commands, with the Claude Code hook that triggers each one automatically:

```bash
echo "! approve this?"   > /tmp/frog.pipe  # ATTENTION — wide eyes, O mouth, flashing red border  (hook: Notification)
echo "> building app"    > /tmp/frog.pipe  # BUSY — scanning eyes, flat mouth, amber WORKING...   (hook: UserPromptSubmit)
echo "= tests passed"    > /tmp/frog.pipe  # READY — happy ^ ^ eyes, big grin, green YOUR TURN    (hook: Stop / SubagentStop)
echo "x build failed"    > /tmp/frog.pipe  # ERROR — red x x eyes, frown, still                   (manual only)
echo "."                 > /tmp/frog.pipe  # IDLE — wandering eyes, small smile, blinks           (hook: SessionStart)
```

Text after the command character is optional and shows in the message bar (truncated to 21 characters).

Writing to the pipe from a plain shell when no reader is connected will block the terminal until a reader appears. That blocking is the signal that the daemon is not running. (The Claude Code hook script is immune: it opens the pipe non-blocking and drops the message silently if no daemon is listening.)

## Claude Code integration

Save the hook script as `~/bin/frog-hook.sh` and make it executable:

```bash
chmod +x ~/bin/frog-hook.sh
```

Add the hooks to `~/.claude/settings.json` (same content as `settings.json` in this repo):

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
- `UserPromptSubmit` marks the bot busy.
- `Stop` / `SubagentStop` mark your turn.
- `SessionStart` resets to idle.

Hook event names and the stdin JSON field names change between Claude Code releases. Verify them against your installed version; the `message` / `prompt` keys the script reads must match what your version emits.

## Startup order

1. Flash the firmware.
2. Start `frogd.py`.
3. Confirm a manual `echo "= hi" > /tmp/frog.pipe` makes the bot grin.
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
- **Bot does not react to Claude Code** — restart the Claude session after editing `settings.json`; verify hook event names for your version.

## Tuning (firmware)

- `SCALE` — bot size. `17 * SCALE` must stay ≤ 128.
- `BASE_TOP` — resting height in the grid.
- `BLINK_PERIOD` / `BLINK_DUR`, `GLANCE_PERIOD` (idle eye wander), `SCAN_STEP` (busy eye sweep), `BOUNCE_DUR`, `BOUNCE_H`, `BOB_MS` — timing of each behavior, independent so motion does not look mechanical.
- Per-state bounce speed is set in the `switch (state)` block in `loop()`; per-state faces in `applyFace()`.
- Body shape: edit the `BOT[]` rows (each exactly 17 characters). Legend: `.` background, `d` dark outline, `b` body, `s` face screen, `w` white. Colors live in `PAL[]`.
- Expressions: eyes are 3x3 bitmaps (`EYE_*`), mouths are 7x2 bitmaps (`MOUTH_*`), one byte per row, MSB = leftmost cell. Add a shape and map it to a state in `applyFace()`.
- Status/message colors: `C_RED`, `C_AMBER`, `C_GREEN`, `C_WHITE`, `C_CYAN`, `C_BG`.

RAM use: two 17x18 framebuffers, ~612 bytes; sprite art is stored in flash.
