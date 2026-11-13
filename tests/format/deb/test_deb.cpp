#include <catch2/catch_test_macros.hpp>
#include <lingua/format/deb/deb_parser.hpp>
#include <lingua/core/types.hpp>
#include <cstring>

using namespace lingua;
using namespace lingua::deb;

static std::vector<uint8_t> make_ar() {
    std::string out = "!<arch>\n";
    // debian-binary member
    std::string content = "2.0\n";
    // pad to even
    std::string name = "debian-binary";
    // ar header 60 bytes
    char hdr[60];
    std::memset(hdr, ' ', 60);
    std::memcpy(hdr, name.c_str(), name.size());
    // size at offset 48
    std::string sz = std::to_string(content.size());
    std::memcpy(hdr+48, sz.c_str(), sz.size());
    hdr[58] = '`';
    hdr[59] = '\n';
    out.append(hdr, 60);
    out += content;
    if (content.size() % 2 == 1) out += '\n';
    return std::vector<uint8_t>(out.begin(), out.end());
}

TEST_CASE("deb detect", "[deb]") {
    DebParser p;
    auto data = make_ar();
    REQUIRE(p.detect(data));
    auto res = p.parse(data, "t.deb");
    REQUIRE(res.has_value());
    REQUIRE(res->format == Format::Deb);
}