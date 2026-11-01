#!/usr/bin/env bash
# SkyTracker: safe app-only flasher with numbered serial port selection.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
echo "SkyTracker firmware — dual-USB ILI9341 v2, HSPI, inversion ON"
echo "The script backs up firmware, leaves saved Wi-Fi and SD alone."
for f in firmware.bin partitions.bin SHA256SUMS.txt; do
  [[ -f "$f" ]] || { echo "Missing $f"; exit 1; }
done
sha256sum -c SHA256SUMS.txt || { echo "Checksum failure. Not flashing."; exit 1; }

declare -a ports=()
declare -A seen=()
for item in /dev/serial/by-id/* /dev/ttyUSB* /dev/ttyACM*; do
  [[ -e "$item" ]] || continue
  real="$(readlink -f "$item")"
  [[ -c "$real" ]] || continue
  if [[ -z "${seen[$real]:-}" ]]; then
    ports+=("$item")
    seen["$real"]=1
  fi
done
if (( ${#ports[@]} == 0 )); then
  echo "No USB serial ports found. Check the USB data cable."
  exit 1
fi
echo "Detected USB serial ports:"
for i in "${!ports[@]}"; do
  printf '  %d) %s\n' "$((i+1))" "${ports[$i]}"
done
while true; do
  read -rp "Choose port number (or q to cancel): " choice
  [[ "$choice" != q && "$choice" != Q ]] || exit 0
  if [[ "$choice" =~ ^[0-9]+$ ]] &&
     (( 10#$choice >= 1 && 10#$choice <= ${#ports[@]} )); then
    PORT="${ports[$((10#$choice-1))]}"
    break
  fi
  echo "Invalid number, try again."
done
echo "Selected: $PORT"

VENV="$HOME/.local/share/cyd-skytracker/esptool-venv"
if [[ ! -x "$VENV/bin/python" ]]; then
  python3 -m venv "$VENV" || { echo "sudo apt install python3-venv"; exit 1; }
fi
if ! "$VENV/bin/python" -c 'import esptool' >/dev/null 2>&1; then
  "$VENV/bin/python" -m pip install 'esptool==4.8.1'
fi
TOOL=("$VENV/bin/python" -m esptool --chip esp32 --port "$PORT" --baud 460800)
echo "Checking the partition layout..."
"${TOOL[@]}" read_flash 0x8000 0xC00 existing-partitions.bin
if ! cmp -s existing-partitions.bin partitions.bin; then
  echo "Partition mismatch: no firmware was flashed."
  exit 1
fi
BACKUP="SkyTracker-backup-$(date +%Y%m%d-%H%M%S).bin"
echo "Backing up current application to $BACKUP..."
"${TOOL[@]}" read_flash 0x10000 0x1E0000 "$BACKUP"
[[ -s "$BACKUP" ]] || { echo "Backup failed."; exit 1; }
read -rp "Type FLASH to install app firmware.bin: " confirm
[[ "$confirm" == FLASH ]] || { echo "Cancelled."; exit 0; }
"${TOOL[@]}" write_flash 0x10000 firmware.bin
echo "Done. Saved Wi-Fi settings and SD files were not erased."
