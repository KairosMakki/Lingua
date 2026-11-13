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

# PE minimal
pe = bytearray(512)
pe[0:2] = b"MZ"
struct.pack_into("<I", pe, 0x3C, 0x80)
pe[0x80:0x84] = b"PE\\x00\\x00"
struct.pack_into("<H", pe, 0x84, 0x8664)
write(os.path.join(ROOT, "pe", "minimal.exe"), bytes(pe))

# Mach-O minimal
macho = bytearray(64)
macho[0:4] = b"\\xfe\\xed\\xfa\\xce"
struct.pack_into("<I", macho, 4, 0x01000007)
write(os.path.join(ROOT, "macho", "minimal.macho"), bytes(macho))

# RPM minimal
rpm = bytearray(256)
rpm[0:4] = b"\\xed\\xab\\xee\\xdb"
rpm[4] = 3; rpm[5] = 0
name = b"testpkg"
for i,c in enumerate(name): rpm[10+i] = c
# signature header at 96
rpm[96:100] = b"\\x8e\\xad\\xe8\\x01"
# main header at 96+256? We'll just write magic lead
write(os.path.join(ROOT, "rpm", "minimal.rpm"), bytes(rpm))

# APK minimal: zip magic + marker
apk = b"PK\\x03\\x04" + b"\\x00"*10 + b"AndroidManifest.xml"
write(os.path.join(ROOT, "apk", "minimal.apk"), apk)

# MSI minimal
msi = bytearray(512)
msi[0:8] = b"\\xd0\\xcf\\x11\\xe0\\xa1\\xb1\\x1a\\xe1"
write(os.path.join(ROOT, "msi", "minimal.msi"), bytes(msi))

# Flatpak minimal tar
flatpak = b"ustar" + b"\\x00"*252 + b"flatpak" + b"\\x00"*100
write(os.path.join(ROOT, "flatpak", "minimal.flatpak"), flatpak)

# Snap minimal
snap = bytearray(96)
snap[0:4] = b"hsqs"
write(os.path.join(ROOT, "snap", "minimal.snap"), bytes(snap))

# Malformed
write(os.path.join(ROOT, "malformed", "truncated.elf"), b"\\x7fEL")
write(os.path.join(ROOT, "malformed", "bad_pe.exe"), b"MZ" + b"\\x00"*10)
print("done")