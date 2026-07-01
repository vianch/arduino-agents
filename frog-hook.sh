#!/usr/bin/env bash
# Hook script, save as ~/bin/frog-hook.sh, chmod +x it:
# frog-hook.sh <prefix>   — reads hook JSON on stdin, forwards to the frog
PIPE="${FROG_PIPE:-/tmp/frog.pipe}"
msg="$(cat | python3 -c 'import sys,json
try:
    d=json.load(sys.stdin); print(d.get("message") or d.get("prompt") or "")
except Exception:
    print("")' 2>/dev/null)"
[ -p "$PIPE" ] && printf '%s %s\n' "$1" "${msg:0:21}" > "$PIPE"
