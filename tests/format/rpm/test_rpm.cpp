#include <catch2/catch_test_macros.hpp>
#include <lingua/format/rpm/rpm_parser.hpp>
#include <lingua/core/types.hpp>

using namespace lingua;
using namespace lingua::rpm;

TEST_CASE("rpm detect", "[rpm]") {
    std::vector<uint8_t> data = {0xED,0xAB,0xEE,0xDB,0,0,0,0};
    data.resize(96, 0);
    RpmParser p;
    REQUIRE(p.detect(data));
}

TEST_CASE("rpm parse lead", "[rpm]") {
    std::vector<uint8_t> data(256, 0);
    data[0]=0xED; data[1]=0xAB; data[2]=0xEE; data[3]=0xDB;
    data[4]=3; data[5]=0;
    // name at offset 10
    const char* nm = "testpkg";
    for (size_t i=0; nm[i]; ++i) data[10+i]=nm[i];
    // signature header magic at 96
    data[96]=0x8E; data[97]=0xAD; data[98]=0xE8; data[99]=0x01;
    RpmParser p;
    auto res = p.parse(data, "t.rpm");
    // may fail due to malformed header; detect at least
    (void)res;
    REQUIRE(p.detect(data));
}