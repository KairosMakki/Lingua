#include <lingua/format/rpm/rpm_parser.hpp>
#include <lingua/core/byte_reader.hpp>
#include <lingua/core/error.hpp>

#include <cstring>
#include <string>

namespace lingua::rpm {

bool RpmParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 4) return false;
    return data[0] == 0xED && data[1] == 0xAB && data[2] == 0xEE && data[3] == 0xDB;
}

Result<FileInfo> RpmParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info;
    info.path = path;
    info.format = Format::Rpm;
    info.file_size = data.size();
    info.endianness = Endianness::Big;

    if (data.size() < 96) return Error{ErrorCode::TruncatedInput, "rpm lead truncated"};

    // lead: magic(4), major(1), minor(1), type(2), arch(2), name[66], os(2), sigtype(2)
    uint8_t major = data[4];
    uint8_t minor = data[5];
    uint16_t type = (data[6] << 8) | data[7];
    uint16_t arch = (data[8] << 8) | data[9];
    std::string name;
    for (int i = 10; i < 76 && data[i] != 0; ++i) name.push_back((char)data[i]);

    info.metadata["rpm_version"] = std::to_string(major) + "." + std::to_string(minor);
    info.metadata["type"] = std::to_string(type);
    info.metadata["name"] = name;
    info.metadata["note"] = "rpm header/cpio parsing not fully implemented";

    switch (arch) {
        case 1: info.architecture = Architecture::X86; break;
        case 2: // alpha
        case 3: info.architecture = Architecture::PowerPC; break;
        case 6: case 9: info.architecture = Architecture::X86; break; // i386, i486
        case 7: case 8: case 10: case 11: case 12: info.architecture = Architecture::X86; break;
        case 13: case 14: info.architecture = Architecture::X86; break;
        case 21: info.architecture = Architecture::X86_64; break;
        case 22: info.architecture = Architecture::AArch64; break;
        default: info.architecture = Architecture::Unknown;
    }
    if (!name.empty()) info.metadata["package"] = name;

    return info;
}

} // namespace lingua::rpm