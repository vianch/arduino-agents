#!/usr/bin/env bash
# Hook script, save as ~/bin/demon.sh, chmod +x it:
# demon.sh <prefix>   — reads hook JSON on stdin, forwards to the bot.
# Opens the pipe O_NONBLOCK so a dead daemon can never hang a Claude Code hook.
python3 -c '
import json, os, sys
pipe = os.environ.get("DEMON_PIPE", "/tmp/demon.pipe")
try:
    data = json.load(sys.stdin)
    text = (data.get("message") or data.get("prompt") or "")[:21]
except Exception:
    text = ""
try:
    fd = os.open(pipe, os.O_WRONLY | os.O_NONBLOCK)  # ENXIO if no daemon reading
except OSError:
    sys.exit(0)  # no pipe or no daemon: drop silently
os.write(fd, f"{sys.argv[1]} {text}\n".encode())
os.close(fd)
' "$1" 2>/dev/null
exit 0
