#include <catch2/catch_test_macros.hpp>
#include <lingua/core/byte_reader.hpp>

using namespace lingua;

TEST_CASE("byte reader basic", "[core]") {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    ByteReader r(data);
    REQUIRE(r.read_u8().value() == 0x01);
    REQUIRE(r.read_u16_le().value() == 0x0302);
}

TEST_CASE("byte reader truncation", "[core]") {
    std::vector<uint8_t> data = {0x01};
    ByteReader r(data);
    REQUIRE(!r.read_u32_le());
    REQUIRE(r.read_u8().value() == 0x01);
    REQUIRE(!r.read_u8());
}

TEST_CASE("byte reader cstring", "[core]") {
    std::vector<uint8_t> data = {'h','i',0,'x'};
    ByteReader r(data);
    REQUIRE(r.read_cstring().value() == "hi");
}

TEST_CASE("endian swap", "[core]") {
    REQUIRE(swap16(0x1234) == 0x3412);
    REQUIRE(swap32(0x12345678) == 0x78563412);
}