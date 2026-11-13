#include <catch2/catch_test_macros.hpp>
#include <lingua/import/import.hpp>
#include <lingua/core/types.hpp>

#include <filesystem>
#include <fstream>

using namespace lingua;
using namespace lingua::importing;

TEST_CASE("path safety", "[import]") {
    REQUIRE(is_safe_member_path("foo/bar.txt"));
    REQUIRE(is_safe_member_path("a..b"));
    REQUIRE(!is_safe_member_path("../etc/passwd"));
    REQUIRE(!is_safe_member_path("/abs/path"));
    REQUIRE(!is_safe_member_path("C:\\windows"));
    REQUIRE(!is_safe_member_path("foo/../bar"));
}

TEST_CASE("safe join containment", "[import]") {
    namespace fs = std::filesystem;
    fs::path base = fs::temp_directory_path() / "lingua-stage";
    fs::create_directories(base);
    fs::path evil = fs::temp_directory_path() / "lingua-stage-evil";
    fs::create_directories(evil);
    REQUIRE(safe_join(base, "foo.txt") == base / "foo.txt");
    REQUIRE(safe_join(base, "../lingua-stage-evil/x").empty());
    fs::remove_all(base);
    fs::remove_all(evil);
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