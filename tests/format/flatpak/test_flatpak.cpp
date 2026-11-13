#include <catch2/catch_test_macros.hpp>
#include <lingua/format/flatpak/flatpak_parser.hpp>
#include <lingua/core/types.hpp>
using namespace lingua; using namespace lingua::flatpak;
TEST_CASE("flatpak detect", "[flatpak]") {
    std::vector<uint8_t> data(300, 0);
    // ustar at 257
    const char* u = "ustar";
    for (int i=0; u[i]; ++i) data[257+i]=u[i];
    std::string marker = "flatpak";
    for (size_t i=0;i<marker.size();++i) data[300-marker.size()+i]=marker[i];
    FlatpakParser p; REQUIRE(p.detect(data));
}