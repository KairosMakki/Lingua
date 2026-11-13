#include <lingua/format/snap/snap_parser.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    lingua::snap::SnapParser p;
    std::span<const uint8_t> sp(data, size);
    (void)p.parse(sp, "fuzz");
    return 0;
}