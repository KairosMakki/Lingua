#include <lingua/format/snap/snap_parser.hpp>
#include <lingua/core/error.hpp>
namespace lingua::snap {
bool SnapParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 4) return false;
    return data[0]=='h' && data[1]=='s' && data[2]=='q' && data[3]=='s';
}
Result<FileInfo> SnapParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info; info.path=path; info.format=Format::Snap; info.file_size=data.size();
    info.metadata["payload_type"]="squashfs";
    info.warnings.push_back("Snap parsing is minimal");
    return info;
}
} // namespace lingua::snap