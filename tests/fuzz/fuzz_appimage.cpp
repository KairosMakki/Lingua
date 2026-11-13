#include <lingua/format/appimage/appimage_parser.hpp>
#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    lingua::appimage::AppImageParser p;
    std::span<const uint8_t> sp(data, size);
    (void)p.parse(sp, "fuzz");
    return 0;
}