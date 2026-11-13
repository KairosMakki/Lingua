#include <catch2/catch_test_macros.hpp>
#include <lingua/format/pe/pe_parser.hpp>
#include <lingua/core/types.hpp>

using namespace lingua;
using namespace lingua::pe;

TEST_CASE("pe detect empty", "[pe]") {
    PeParser p;
    std::vector<uint8_t> data(128,0);
    REQUIRE(!p.detect(data));
}

TEST_CASE("pe parse minimal", "[pe]") {
    // Minimal DOS+PE header
    std::vector<uint8_t> data(512, 0);
    data[0]='M'; data[1]='Z';
    // e_lfanew = 0x80
    data[0x3C]=0x80;
    // PE signature
    data[0x80]='P'; data[0x81]='E'; data[0x82]=0; data[0x83]=0;
    // COFF: machine 0x8664 at 0x84
    data[0x84]=0x64; data[0x85]=0x86;
    // num sections 0 at 0x86
    // size optional header 0 at 0x94? offset 0x80+20=0x94
    PeParser p;
    REQUIRE(p.detect(data));
    auto res = p.parse(data, "minimal.exe");
    REQUIRE(res.has_value());
    REQUIRE(res->format == Format::PE);
    REQUIRE(res->architecture == Architecture::X86_64);
}