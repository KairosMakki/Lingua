#include <catch2/catch_test_macros.hpp>
#include <lingua/format/appimage/appimage_parser.hpp>
#include <lingua/core/types.hpp>

using namespace lingua;
using namespace lingua::appimage;

TEST_CASE("appimage detect", "[appimage]") {
    std::vector<uint8_t> data(256, 0);
    data[0]=0x7F; data[1]='E'; data[2]='L'; data[3]='F';
    // embed squashfs magic
    data[100]='h'; data[101]='s'; data[102]='q'; data[103]='s';
    AppImageParser p;
    REQUIRE(p.detect(data));
    auto res = p.parse(data, "t.AppImage");
    REQUIRE(res.has_value());
    REQUIRE(res->format == Format::AppImage);
    REQUIRE(res->metadata.count("payload_type"));
}