#include <lingua/format/flatpak/flatpak_parser.hpp>
#include <lingua/core/error.hpp>
#include <cstring>
#include <string>
namespace lingua::flatpak {
bool FlatpakParser::detect(std::span<const std::uint8_t> data) const {
    // Flatpak bundles are tar archives containing flatpak metadata
    if (data.size() < 262) return false;
    if (std::memcmp(data.data()+257, "ustar", 5) != 0) return false;
    std::string blob(data.begin(), data.end());
    return blob.find("org.flatpak") != std::string::npos || blob.find("flatpak") != std::string::npos;
}
Result<FileInfo> FlatpakParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info; info.path=path; info.format=Format::Flatpak; info.file_size=data.size();
    info.warnings.push_back("Flatpak parsing is minimal");
    return info;
}
} // namespace lingua::flatpak