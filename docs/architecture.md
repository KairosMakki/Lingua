# Architecture

Lingua is organized into layered modules, each a separate CMake target with its own namespace.

## Modules

- `lingua-core`: shared types, error handling, byte reader, memory-mapped files, logging, format registry.
- `lingua-detect`: magic-number and heuristic format detection.
- Format modules: `lingua-format-pe`, `lingua-format-elf`, `lingua-format-deb`, `lingua-format-appimage`.
- `lingua-adapters`: host OS adapters that delegate to existing tools.
- `lingua-import`: import pipeline.
- `lingua-web`: optional local web server and UI.
- `lingua-cli`: command-line tool.

## Design decisions

- No exceptions across the public API; results use `std::expected`-style `Result<T>`.
- All input treated as hostile: bounds-checked reads, overflow guards.
- Parsers degrade gracefully and return warnings.
- Dependencies isolated behind interfaces.
- Platform-specific code confined to a `platform/` abstraction.