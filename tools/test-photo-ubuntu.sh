#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
VENV="$HOME/.local/share/cyd-skytracker/esptool-venv"
if [[ ! -x "$VENV/bin/python" ]]; then
  python3 -m venv "$VENV" || { echo "sudo apt install python3-venv"; exit 1; }
fi
if ! "$VENV/bin/python" -c 'import serial' >/dev/null 2>&1; then
  "$VENV/bin/python" -m pip install 'pyserial>=3.5,<4'
fi
exec "$VENV/bin/python" test-photo-ubuntu.py
