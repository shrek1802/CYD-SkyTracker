#!/usr/bin/env python3
"""Integration smoke: firmware-trusted root must validate the public UK API.

Uses a public example postcode only; never transmits the user's location,
and failure prevents releasing an unusable postcode setup.
"""
import json
import re
import ssl
import urllib.request
from pathlib import Path
pem=Path("src/postcode_ca.h").read_text(encoding="utf-8")
match=re.search(r"-----BEGIN CERTIFICATE-----[\s\S]*?-----END CERTIFICATE-----",pem)
assert match, "Firmware root certificate missing"
context=ssl.create_default_context(cadata=match.group())
req=urllib.request.Request(
    "https://api.postcodes.io/postcodes/SW1A1AA",
    headers={"User-Agent":"CYD-SkyTracker-Postcode-Smoke/1.0"})
with urllib.request.urlopen(req,timeout=15,context=context) as rsp:
    body=json.load(rsp)
assert body["status"]==200
assert 51.3 < body["result"]["latitude"] < 51.7
assert -0.4 < body["result"]["longitude"] < 0.3
print("Postcodes.io HTTPS CA and API contract verified")
