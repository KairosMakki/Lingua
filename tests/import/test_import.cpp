#include <catch2/catch_test_macros.hpp>
#include <lingua/import/import.hpp>
#include <lingua/core/types.hpp>

#include <filesystem>
#include <fstream>

using namespace lingua;
using namespace lingua::importing;

TEST_CASE("path safety", "[import]") {
    REQUIRE(is_safe_member_path("foo/bar.txt"));
    REQUIRE(!is_safe_member_path("../etc/passwd"));
    REQUIRE(!is_safe_member_path("/abs/path"));
    REQUIRE(!is_safe_member_path("C:\\windows"));
}

TEST_CASE("import dry run", "[import]") {
    namespace fs = std::filesystem;
    fs::path tmp = fs::temp_directory_path() / "lingua-test-input.bin";
    {
        std::ofstream out(tmp, std::ios::binary);
        out << "hello";
    }
    FileInfo info;
    info.format = Format::Unknown;
    ImportOptions opts;
    opts.dry_run = true;
    opts.staging_dir = fs::temp_directory_path() / "lingua-test-staging";
    auto res = import_app(info, tmp, opts);
    REQUIRE(res.has_value());
    REQUIRE(res->success);
    fs::remove(tmp);
}