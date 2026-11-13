#include <catch2/catch_test_macros.hpp>
#include <lingua/format/apk/apk_parser.hpp>
#include <lingua/core/types.hpp>

using namespace lingua;
using namespace lingua::apk;

TEST_CASE("apk detect", "[apk]") {
    // zip magic + marker
    std::string s = "PK\x03\x04" + std::string(10,0) + "AndroidManifest.xml";
    std::vector<uint8_t> data(s.begin(), s.end());
    ApkParser p;
    REQUIRE(p.detect(data));
}