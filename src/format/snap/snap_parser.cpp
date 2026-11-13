#include <lingua/format/snap/snap_parser.hpp>
#include <lingua/core/error.hpp>
namespace lingua::snap {
bool SnapParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 4) return false;
    return data[0]=='h' && data[1]=='s' && data[2]=='q' && data[3]=='s';
}
Result<FileInfo> SnapParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info; info.path=path; info.format=Format::Snap; info.file_size=data.size();
    if (data.size() < 96) return Error{ErrorCode::TruncatedInput, "squashfs superblock truncated"};
    // squashfs superblock fields (little-endian)
    auto rd = [&](size_t off, size_t n) -> uint64_t {
        uint64_t v=0;
        for (size_t i=0;i<n;++i) if (off+i < data.size()) v |= (uint64_t)data[off+i] << (i*8);
        return v;
    };
    uint32_t inodes = (uint32_t)rd(4,4);
    uint32_t block_size = (uint32_t)rd(12,4);
    uint16_t compression = (uint16_t)rd(16,2);
    uint16_t major = (uint16_t)rd(20,2);
    uint16_t minor = (uint16_t)rd(22,2);
    uint64_t bytes_used = rd(40,8);
    const char* comp_name = "unknown";
    switch (compression) { case 1: comp_name="zlib"; break; case 2: comp_name="lzo"; break; case 3: comp_name="xz"; break; case 4: comp_name="lz4"; break; case 5: comp_name="zstd"; break; }
    info.metadata["squashfs_version"] = std::to_string(major) + "." + std::to_string(minor);
    info.metadata["inodes"] = std::to_string(inodes);
    info.metadata["block_size"] = std::to_string(block_size);
    info.metadata["compression"] = comp_name;
    info.metadata["bytes_used"] = std::to_string(bytes_used);
    info.metadata["payload_type"]="squashfs";
    info.warnings.push_back("snap file extraction requires unsquashfs");
    return info;
}
} // namespace lingua::snap