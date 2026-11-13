#include <catch2/catch_test_macros.hpp>
#include <lingua/core/archive.hpp>
#include <vector>
#include <string>
using namespace lingua::archive;

TEST_CASE("parse zip empty", "[archive]") {
    std::vector<uint8_t> data;
    auto e = parse_zip(data);
    REQUIRE(e.empty());
}

TEST_CASE("parse ar bad", "[archive]") {
    std::vector<uint8_t> data = {1,2,3};
    auto e = parse_ar(data);
    REQUIRE(e.empty());
}