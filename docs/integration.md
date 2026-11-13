# Integration Guide

## CMake (add_subdirectory)

```cmake
add_subdirectory(Lingua)
target_link_libraries(myapp PRIVATE lingua::lingua)
```

## find_package

```cmake
find_package(Lingua 0.1 REQUIRED)
target_link_libraries(myapp PRIVATE lingua::lingua)
```

## pkg-config

```bash
pkg-config --cflags --libs lingua
```

## Example

```cpp
#include <lingua/lingua.hpp>
auto file = lingua::open("app.exe");
auto info = lingua::inspect(*file);
```