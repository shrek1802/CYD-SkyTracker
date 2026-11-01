#!/usr/bin/env python3
"""Verify the firmware's trusted PEM roots against Postcodes.io.

This is a network integration smoke executed separately from unittest discover.
No user postcode is ever used: SW1A1AA is a public example.
"""
import json
import re
import ssl
import subprocess
import urllib.request
from pathlib import Path

def test_live_api():
    raw=Path("src/postcode_ca.h").read_text(encoding="utf-8")
    certs=re.findall(r"-----BEGIN CERTIFICATE-----[\s\S]*?-----END CERTIFICATE-----",raw)
    assert certs, "Missing firmware trust anchors"
    context=ssl.create_default_context(cadata="\n".join(certs))
    req=urllib.request.Request(
        "https://api.postcodes.io/postcodes/SW1A1AA",
        headers={"User-Agent":"CYD-SkyTracker-Postcode-Smoke/1.0"})
    try:
        with urllib.request.urlopen(req,timeout=15,context=context) as rsp:
            body=json.load(rsp)
    except Exception:
        # Diagnostic chain comes straight from the real TLS endpoint.
        p=subprocess.run(["openssl","s_client","-connect","api.postcodes.io:443",
                "-servername","api.postcodes.io","-showcerts"],
                input="",capture_output=True,text=True,timeout=15)
        print("Certificate chain summary:",p.stderr[-1200:])
        chain=re.findall(r"-----BEGIN CERTIFICATE-----[\s\S]*?-----END CERTIFICATE-----",p.stdout)
        for i,pem in enumerate(chain[:4]):
            result=subprocess.run(["openssl","x509","-noout","-issuer","-subject"],
                input=pem,capture_output=True,text=True)
            print("Chain",i,result.stdout.strip())
        raise
    assert body["status"]==200
    assert 51.3 < body["result"]["latitude"] < 51.7
    assert -0.4 < body["result"]["longitude"] < 0.3
    print("Postcodes.io HTTPS root CA and API contract verified")

if __name__=="__main__":
    test_live_api()
