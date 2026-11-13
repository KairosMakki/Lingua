# Lingua

[![CI](https://github.com/KairosMakki/Lingua/actions/workflows/ci.yml/badge.svg)](https://github.com/KairosMakki/Lingua/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Latest Release](https://img.shields.io/github/v/release/KairosMakki/Lingua)](https://github.com/KairosMakki/Lingua/releases)

Lingua is a modern C++20 library for reading, inspecting, and importing cross-platform application formats. It lets any operating system or application understand software packages and executables from other platforms without reimplementing a loader.

## Features

- PE (Windows executables and DLLs, 32/64-bit)
- ELF (Linux/Unix executables and shared objects, 32/64-bit, both endiannesses)
- Mach-O (macOS executables, including universal binaries)
- Debian packages (.deb)
- RPM packages (.rpm)
- Android packages (.apk)
- AppImage (Type 1 and Type 2)
- MSI, Flatpak, Snap
- Magic-number format detection on files and streams
- Host adapters that delegate to existing tools (Wine, dpkg, native loader)
- Import pipeline with staging, dry-run, and audit logging
- Optional local web import interface (localhost-only by default)
- Command-line tool

## Quick start

```bash
git clone https://github.com/KairosMakki/Lingua.git
cd Lingua
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

```cpp
#include <lingua/lingua.hpp>

auto file = lingua::open("app.exe");
if (!file) { /* handle error */ }

auto info = lingua::inspect(*file);
// info->format == lingua::Format::PE

lingua::ImportOptions opts;
opts.dry_run = true;
auto result = lingua::import_app(*file, opts);
```

## Documentation

See the `docs/` directory: architecture, integration guide, format notes, web interface, contributing.

## License

MIT. See [LICENSE](LICENSE).