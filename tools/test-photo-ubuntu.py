#!/usr/bin/env python3
"""SkyTracker USB PHOTO_TEST with handshake, targeted USB ports and report."""
import sys
import time
from datetime import datetime
from pathlib import Path

import serial
from serial.tools import list_ports


def usb_ports():
    devices = []
    for item in list_ports.comports():
        # Ignore Linux's built-in 32 ttyS serial endpoints. They are not CYDs.
        if not (item.device.startswith("/dev/ttyUSB") or
                item.device.startswith("/dev/ttyACM")):
            continue
        devices.append(item)
    return sorted(devices, key=lambda x: x.device)


def read_until(ser, logfile, seconds, markers):
    seen = []
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        raw = ser.readline()
        if not raw:
            continue
        value = raw.decode("utf-8", errors="replace").rstrip("\r\n")
        print(value)
        logfile.write(value + "\n")
        logfile.flush()
        if any(marker in value for marker in markers):
            seen.append(value)
    return seen


def send(ser, value):
    raw = (value + "\n").encode("ascii")
    count = ser.write(raw)
    ser.flush()
    print(f"[HOST] Sent {value} ({count}/{len(raw)} bytes)")
    return count == len(raw)


def main():
    devices = usb_ports()
    if not devices:
        print("No USB/ACM serial ports. Check CYD USB data cable.")
        return 2
    print("Detected USB serial ports (ttyS0-ttyS31 hidden):")
    for i, item in enumerate(devices, 1):
        vendor = f" VID:PID={item.vid:04x}:{item.pid:04x}" if item.vid is not None and item.pid is not None else ""
        print(f"  {i}) {item.device} — {item.description}{vendor}")
    try:
        choice = input("Choose the CYD port number (q to cancel): ").strip()
        if choice.lower() == "q":
            return 0
        index = int(choice) - 1
        if not 0 <= index < len(devices):
            raise ValueError("out of range")
    except ValueError:
        print("Invalid selection.")
        return 2

    port = devices[index].device
    output = Path("SkyTracker-PHOTO-TEST-" +
                  datetime.now().strftime("%Y%m%d-%H%M%S") + ".txt")
    print(f"Opening {port} at 115200 baud.")
    try:
        with serial.Serial(port=port, baudrate=115200, timeout=0.3, write_timeout=3) as ser, output.open("w", encoding="utf-8") as log:
            log.write("Port: " + port + "\n")
            # Opening UART often resets ESP32; allow its boot output to settle.
            read_until(ser, log, 3, [])
            print("[HOST] Checking active firmware: VERSION")
            send(ser, "VERSION")
            version = read_until(ser, log, 10, ["[CMD] SkyTracker diagnostic"])
            if not version:
                print("[HOST] VERSION not acknowledged. Installed firmware may be older,")
                print("       or ESP32 may not be receiving commands from the USB port.")
            else:
                print("[HOST] VERSION acknowledged. Testing known B738 SD image.")
            send(ser, "PHOTO_TEST")
            lines = read_until(ser, log, 25, ["[PHOTO_TEST]", "[PHOTO]", "[CMD] Received PHOTO_TEST"])
            print("\n---- RESULT ----")
            if any("[PHOTO_TEST] SD RGB565 READY" in x for x in lines) and any("drawing: PASS" in x for x in lines):
                print("Firmware read the B738 photo and all 96 rows.")
                print("Check whether that picture was actually visible on the CYD screen.")
            elif any("[CMD] Received PHOTO_TEST" in x for x in lines) or any("[PHOTO_TEST]" in x for x in lines):
                print("Firmware accepted PHOTO_TEST. Review SD/credit/file/drawing messages above.")
            else:
                print("PHOTO_TEST WAS NOT ACKNOWLEDGED.")
                print("The photo reader has not been tested. Check that the diagnostic")
                print("firmware was flashed (VERSION must report PHOTO_TEST=YES).")
                print("If it WAS flashed, close other serial monitors and check USB RX.")
            print("Report:", output)
    except (serial.SerialException, OSError) as exc:
        print("USB serial error:", exc)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
