#!/usr/bin/env python3
"""tts_probe.py <port> — receive MLIT MPEG2-TTS(192) RTP and report structure.

Prints: pt=<payload type> units=<#192B units> sync=<#units with 0x47 at +4>.
A conformant TTS stream has pt=103, units>0, and sync==units (every 192-byte
unit is a 4-byte 27MHz timecode followed by a 188-byte TS packet whose first
byte is the 0x47 sync). rtpmp2tdepay can't be used here because it assumes
188-byte TS packets, so this checks the wrapping directly.
"""
import socket, sys

port = int(sys.argv[1])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('127.0.0.1', port))
s.settimeout(4)
pt = None; units = 0; sync = 0; n = 0
try:
    while n < 40:
        d, _ = s.recvfrom(4096)
        if len(d) < 12:
            continue
        pt = d[1] & 0x7f
        pay = d[12:]
        if pay and len(pay) % 192 == 0:
            u = len(pay) // 192
            for i in range(u):
                if pay[i * 192 + 4] == 0x47:
                    sync += 1
            units += u
        n += 1
except socket.timeout:
    pass
print(f"pt={pt} units={units} sync={sync}")
