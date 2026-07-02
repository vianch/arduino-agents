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

def open_serial():
    # blocks until a board shows up (handles unplug/replug)
    while True:
        port = find_port()
        if port:
            try:
                ser = serial.Serial(port, BAUD, timeout=1)
                time.sleep(2)  # board reboots when the port opens
                print(f"frogd: connected {port}", flush=True)
                return ser
            except (serial.SerialException, OSError):
                pass
        time.sleep(2)  # ponytail: 2s poll; hotplug events not worth a dependency

def main():
    if not find_port():
        sys.exit("frogd: no serial port found (set FROG_PORT)")
    if not os.path.exists(PIPE):
        os.mkfifo(PIPE)
    ser = open_serial()
    print(f"frogd: {PIPE} -> serial", flush=True)
    while True:
        with open(PIPE, "r") as fifo:      # blocks until a writer connects
            for line in fifo:
                line = line.strip()
                if not line:
                    continue
                try:
                    ser.write((line + "\n").encode())
                    ser.flush()
                except (serial.SerialException, OSError):
                    ser.close()
                    ser = open_serial()    # board was unplugged; resend once
                    try:
                        ser.write((line + "\n").encode())
                        ser.flush()
                    except (serial.SerialException, OSError):
                        pass
        # all writers closed; reopen

if __name__ == "__main__":
    main()
