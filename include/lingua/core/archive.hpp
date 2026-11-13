#pragma once

#include <lingua/core/error.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lingua::archive {

struct ArMember {
    std::string name;
    std::vector<std::uint8_t> data;
};

struct TarEntry {
    std::string name;
    std::vector<std::uint8_t> data;
    bool is_dir = false;
};

struct ZipEntry {
    std::string name;
    std::vector<std::uint8_t> data;
    bool is_dir = false;
};

std::vector<ArMember> parse_ar(const std::vector<std::uint8_t>& data);
std::vector<TarEntry> parse_tar(const std::vector<std::uint8_t>& data);
std::vector<ZipEntry> parse_zip(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> decompress(const std::vector<std::uint8_t>& data);

} // namespace lingua::archive