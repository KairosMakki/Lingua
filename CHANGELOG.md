# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog, and this project adheres to Semantic Versioning.

## [0.1.0] - 2026-11-12

### Added
- Mach-O, RPM, and APK format support.
- PE exports, Authenticode presence, .NET detection.
- ELF security hardening metadata and RPATH/RUNPATH.
- Extract command for archives and sections.
- Web interface is localhost-only by default.
- Core module with byte reader, memory-mapped files, error handling, format registry.
- Format parsers: PE, ELF, Debian packages, AppImage.
- Format detection for files and streams.
- Host adapters for PE, ELF, deb, AppImage.
- Import pipeline with staging, dry-run, audit log.
- Optional local web import interface.
- Command-line tool with detect, inspect, extract, import commands.
- Documentation, CI workflows, and test suite.