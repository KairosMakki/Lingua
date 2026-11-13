# Contributing

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DLINGUA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## Coding style

Follow `.clang-format`. Use Conventional Commits.

## Adding a new format module

1. Create `lingua-format-<name>` target under `src/format/<name>/`.
2. Implement `IFormatParser`.
3. Register in detection and API.
4. Add tests and docs.