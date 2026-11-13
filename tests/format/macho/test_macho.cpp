#include <catch2/catch_test_macros.hpp>
#include <lingua/format/macho/macho_parser.hpp>
#include <lingua/core/types.hpp>
#include <cstring>

using namespace lingua;
using namespace lingua::macho;

TEST_CASE("macho detect", "[macho]") {
    std::vector<uint8_t> data(64, 0);
    data[0]=0xFE; data[1]=0xED; data[2]=0xFA; data[3]=0xCE;
    MachOParser p;
    REQUIRE(p.detect(data));
}

TEST_CASE("macho parse", "[macho]") {
    std::vector<uint8_t> data(64, 0);
    data[0]=0xFE; data[1]=0xED; data[2]=0xFA; data[3]=0xCE;
    // cputype x86_64 at offset 4: 0x01000007 little
    data[4]=0x07; data[5]=0x00; data[6]=0x00; data[7]=0x01;
    MachOParser p;
    auto res = p.parse(data, "t.macho");
    REQUIRE(res.has_value());
    REQUIRE(res->format == Format::MachO);
}