#include <lingua/format/msi/msi_parser.hpp>
#include <lingua/core/error.hpp>
#include <cstring>
namespace lingua::msi {
bool MsiParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 8) return false;
    return data[0]==0xD0 && data[1]==0xCF && data[2]==0x11 && data[3]==0xE0 && data[4]==0xA1 && data[5]==0xB1 && data[6]==0x1A && data[7]==0xE1;
}
Result<FileInfo> MsiParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info; info.path=path; info.format=Format::Msi; info.file_size=data.size();
    info.warnings.push_back("MSI parsing is minimal");
    return info;
}
} // namespace lingua::msi