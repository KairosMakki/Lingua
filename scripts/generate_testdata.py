#!/usr/bin/env python3
"""Generate small, legally clean test fixtures for Lingua."""
import os
import struct
import zlib

ROOT = os.path.join(os.path.dirname(__file__), "..", "testdata")
os.makedirs(ROOT, exist_ok=True)

def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    print("wrote", path)

# ELF 64-bit little-endian minimal
elf = bytearray(64)
elf[0:4] = b"\x7fELF"
elf[4] = 2; elf[5] = 1; elf[6] = 1
struct.pack_into("<H", elf, 16, 2)  # ET_EXEC
struct.pack_into("<H", elf, 18, 62) # x86_64
write(os.path.join(ROOT, "elf", "minimal.elf"), bytes(elf))

# PE minimal
pe = bytearray(512)
pe[0:2] = b"MZ"
struct.pack_into("<I", pe, 0x3C, 0x80)
pe[0x80:0x84] = b"PE\0\0"
struct.pack_into("<H", pe, 0x84, 0x8664)  # machine
write(os.path.join(ROOT, "pe", "minimal.exe"), bytes(pe))

# deb minimal
ar = b"!<arch>\n"
content = b"2.0\n"
name = b"debian-binary"
hdr = bytearray(60)
hdr[:len(name)] = name
hdr[48:48+len(str(len(content)).encode())] = str(len(content)).encode()
hdr[58:60] = b"`\n"
# pad spaces
for i in range(60):
    if hdr[i] == 0:
        hdr[i] = ord(" ")
ar += bytes(hdr) + content
if len(content) % 2:
    ar += b"\n"
write(os.path.join(ROOT, "deb", "minimal.deb"), ar)

# AppImage minimal: ELF + squashfs magic
app = bytearray(256)
app[0:4] = b"\x7fELF"
app[4] = 2; app[5] = 1; app[6] = 1
struct.pack_into("<H", app, 18, 62)
app[100:104] = b"hsqs"
write(os.path.join(ROOT, "appimage", "minimal.AppImage"), bytes(app))

# Malformed
write(os.path.join(ROOT, "malformed", "truncated.elf"), b"\x7fEL")
write(os.path.join(ROOT, "malformed", "bad_pe.exe"), b"MZ" + b"\x00"*10)
print("done")