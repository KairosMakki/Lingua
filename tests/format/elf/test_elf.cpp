#include <catch2/catch_test_macros.hpp>
#include <lingua/format/elf/elf_parser.hpp>
#include <lingua/core/types.hpp>

using namespace lingua;
using namespace lingua::elf;

static std::vector<uint8_t> make_elf32_le() {
    std::vector<uint8_t> data(64, 0);
    data[0]=0x7F; data[1]='E'; data[2]='L'; data[3]='F';
    data[4]=1; // 32-bit
    data[5]=1; // little
    data[6]=1;
    // e_type at 16: ET_EXEC 2
    data[16]=2; data[17]=0;
    // e_machine at 18: x86 3
    data[18]=3; data[19]=0;
    // e_version at 20
    data[20]=1;
    return data;
}

TEST_CASE("elf parse 32", "[elf]") {
    ElfParser p;
    auto data = make_elf32_le();
    REQUIRE(p.detect(data));
    auto res = p.parse(data, "t.elf");
    REQUIRE(res.has_value());
    REQUIRE(res->architecture == Architecture::X86);
    REQUIRE(res->endianness == Endianness::Little);
}

TEST_CASE("elf parse 64 be", "[elf]") {
    std::vector<uint8_t> data(64,0);
    data[0]=0x7F; data[1]='E'; data[2]='L'; data[3]='F';
    data[4]=2; data[5]=2; data[6]=1;
    // e_type at 16
    data[16]=0; data[17]=2; // big-endian ET_EXEC? Actually bytes: 0 2 => 2? rd16 be = data[16]<<8 | data[17] = 2 yes
    // e_machine at 18: x86_64 62 = 0x003E -> be bytes 0 62
    data[18]=0; data[19]=62;
    ElfParser p;
    REQUIRE(p.detect(data));
    auto res = p.parse(data, "t.elf");
    REQUIRE(res.has_value());
    REQUIRE(res->architecture == Architecture::X86_64);
    REQUIRE(res->endianness == Endianness::Big);
}