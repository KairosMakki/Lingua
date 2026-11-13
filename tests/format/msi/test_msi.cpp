#include <catch2/catch_test_macros.hpp>
#include <lingua/format/msi/msi_parser.hpp>
#include <lingua/core/types.hpp>
using namespace lingua; using namespace lingua::msi;
TEST_CASE("msi detect", "[msi]") {
    std::vector<uint8_t> data(512, 0);
    data[0]=0xD0; data[1]=0xCF; data[2]=0x11; data[3]=0xE0; data[4]=0xA1; data[5]=0xB1; data[6]=0x1A; data[7]=0xE1;
    MsiParser p; REQUIRE(p.detect(data));
}
TEST_CASE("msi parse", "[msi]") {
    std::vector<uint8_t> data(512, 0);
    data[0]=0xD0; data[1]=0xCF; data[2]=0x11; data[3]=0xE0; data[4]=0xA1; data[5]=0xB1; data[6]=0x1A; data[7]=0xE1;
    MsiParser p; auto res = p.parse(data, "t.msi"); REQUIRE(res.has_value()); REQUIRE(res->format == Format::Msi);
}