#include <catch2/catch_test_macros.hpp>
#include <lingua/core/mapped_file.hpp>
#include <lingua/core/error.hpp>

#include <filesystem>
#include <fstream>

using namespace lingua;

TEST_CASE("mapped file from buffer", "[core]") {
    std::vector<uint8_t> data = {1,2,3,4};
    auto m = MappedFile::from_buffer(std::move(data));
    REQUIRE(m.has_value());
    REQUIRE(m->size() == 4);
    REQUIRE(m->data()[0] == 1);
}

TEST_CASE("mapped file open missing", "[core]") {
    auto m = MappedFile::open("C:/nonexistent/file.bin");
    REQUIRE(!m);
}