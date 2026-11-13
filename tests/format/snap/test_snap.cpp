#include <catch2/catch_test_macros.hpp>
#include <lingua/format/snap/snap_parser.hpp>
#include <lingua/core/types.hpp>
using namespace lingua; using namespace lingua::snap;
TEST_CASE("snap detect", "[snap]") {
    std::vector<uint8_t> data(96, 0);
    data[0]='h'; data[1]='s'; data[2]='q'; data[3]='s';
    SnapParser p; REQUIRE(p.detect(data));
}
TEST_CASE("snap parse", "[snap]") {
    std::vector<uint8_t> data(96, 0);
    data[0]='h'; data[1]='s'; data[2]='q'; data[3]='s';
    data[12]=0; data[13]=0; data[14]=0x01; // block size 65536? not needed
    SnapParser p; auto res = p.parse(data, "t.snap"); REQUIRE(res.has_value()); REQUIRE(res->format == Format::Snap);
}