#include <lingua/format/appimage/appimage_parser.hpp>
#include <lingua/core/byte_reader.hpp>
#include <lingua/core/error.hpp>
#include <lingua/format/elf/elf_parser.hpp>

#include <algorithm>
#include <cstring>
#include <string>

namespace lingua::appimage {

bool AppImageParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 16) return false;
    if (!(data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F')) return false;
    for (size_t i = 0; i + 4 <= data.size(); ++i) {
        if (data[i] == 'h' && data[i+1] == 's' && data[i+2] == 'q' && data[i+3] == 's') return true;
        if (i + 5 <= data.size() && data[i] == 'C' && data[i+1] == 'D' && data[i+2] == '0' && data[i+3] == '0' && data[i+4] == '1') return true;
    }
    return false;
}

Result<FileInfo> AppImageParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info;
    info.path = path;
    info.format = Format::AppImage;
    info.file_size = data.size();

    // Parse ELF runtime part: find ELF end? We'll delegate architecture detection by scanning ELF header.
    if (data.size() >= 20) {
        uint8_t eclass = data[4];
        bool le = data[5] == 1;
        (void)le;
        // machine at offset 18
        uint16_t machine = data[18] | (data[19] << 8);
        switch (machine) {
            case 3: info.architecture = Architecture::X86; break;
            case 62: info.architecture = Architecture::X86_64; break;
            case 40: info.architecture = Architecture::ARM; break;
            case 183: info.architecture = Architecture::AArch64; break;
            default: info.architecture = Architecture::Unknown;
        }
        info.endianness = (data[5] == 1) ? Endianness::Little : Endianness::Big;
    }

    // Find payload
    size_t payload_off = 0;
    std::string payload_type;
    for (size_t i = 0; i + 4 <= data.size(); ++i) {
        if (data[i] == 'h' && data[i+1] == 's' && data[i+2] == 'q' && data[i+3] == 's') {
            payload_off = i; payload_type = "squashfs"; break;
        }
        if (i + 5 <= data.size() && data[i] == 'C' && data[i+1] == 'D' && data[i+2] == '0' && data[i+3] == '0' && data[i+4] == '1') {
            payload_off = i; payload_type = "iso9660"; break;
        }
    }
    if (payload_off == 0) {
        info.warnings.push_back("could not locate embedded payload");
    } else {
        info.metadata["payload_type"] = payload_type;
        info.metadata["payload_offset"] = std::to_string(payload_off);
        info.metadata["payload_size"] = std::to_string(data.size() - payload_off);
        Section sec;
        sec.name = "payload";
        sec.offset = payload_off;
        sec.size = data.size() - payload_off;
        sec.type = payload_type;
        info.sections.push_back(sec);
    }

    // AppImage type detection via ELF note? Heuristic: type 2 if squashfs, type1 if iso
    if (payload_type == "squashfs") info.metadata["appimage_type"] = "2";
    else if (payload_type == "iso9660") info.metadata["appimage_type"] = "1";
    else info.metadata["appimage_type"] = "unknown";

    info.metadata["extraction"] = "inspect-only";
    info.warnings.push_back("AppImage extraction is inspect-only in this build");
    return info;
}

} // namespace lingua::appimage