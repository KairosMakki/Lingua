#include <catch2/catch_test_macros.hpp>
#include <lingua/detect/detector.hpp>
#include <lingua/core/types.hpp>

using namespace lingua;

TEST_CASE("detect pe", "[detect]") {
    std::vector<uint8_t> data(128, 0);
    data[0]='M'; data[1]='Z';
    // e_lfanew
    data[0x3C]=0x40;
    data[0x40]='P'; data[0x41]='E'; data[0x42]=0; data[0x43]=0;
    REQUIRE(detect_format(data) == Format::PE);
}

TEST_CASE("detect elf", "[detect]") {
    std::vector<uint8_t> data = {0x7F,'E','L','F',1,1,1,0};
    data.resize(64,0);
    REQUIRE(detect_format(data) == Format::ELF);
}

TEST_CASE("detect deb", "[detect]") {
    std::string s = "!<arch>\n";
    std::vector<uint8_t> data(s.begin(), s.end());
    REQUIRE(detect_format(data) == Format::Deb);
}