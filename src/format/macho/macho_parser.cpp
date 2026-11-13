#include <lingua/format/macho/macho_parser.hpp>
#include <lingua/core/byte_reader.hpp>
#include <lingua/core/error.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace lingua::macho {

namespace {
uint32_t rd32(const uint8_t* p, bool le) {
    if (le) return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
    return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3];
}
uint64_t rd64(const uint8_t* p, bool le) {
    uint64_t v = 0;
    if (le) for (int i=7;i>=0;--i) v=(v<<8)|p[i];
    else for (int i=0;i<8;++i) v=(v<<8)|p[i];
    return v;
}
const char* arch_name(uint32_t cputype) {
    switch (cputype) {
        case 7: return "x86";
        case 0x01000007: return "x86_64";
        case 12: return "arm";
        case 0x0100000C: return "aarch64";
        case 18: return "ppc";
        default: return "unknown";
    }
}
}

bool MachOParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 4) return false;
    uint32_t magic = data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
    // fat
    if (magic == 0xBEBAFECA) return true;
    // macho magics
    if (magic == 0xFEEDFACE || magic == 0xFEEDFACF) return true;
    if (magic == 0xCEFAEDFE || magic == 0xCFFAEDFE) return true;
    return false;
}

Result<FileInfo> MachOParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info;
    info.path = path;
    info.format = Format::MachO;
    info.file_size = data.size();

    if (data.size() < 4) return Error{ErrorCode::TruncatedInput, "too small"};

    uint32_t magic0 = data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
    bool fat = (magic0 == 0xBEBAFECA);

    if (fat) {
        // big-endian fat header
        if (data.size() < 8) return Error{ErrorCode::TruncatedInput, "fat header truncated"};
        uint32_t nfat = rd32(data.data() + 4, false);
        info.metadata["fat"] = "true";
        info.metadata["nfat_arch"] = std::to_string(nfat);
        size_t pos = 8;
        for (uint32_t i = 0; i < nfat && pos + 20 <= data.size(); ++i) {
            uint32_t cputype = rd32(data.data() + pos, false);
            uint32_t cpusubtype = rd32(data.data() + pos + 4, false);
            uint32_t offset = rd32(data.data() + pos + 8, false);
            uint32_t size = rd32(data.data() + pos + 12, false);
            uint32_t align = rd32(data.data() + pos + 16, false);
            (void)cpusubtype; (void)size; (void)align;
            info.metadata["arch_" + std::to_string(i)] = arch_name(cputype);
            pos += 20;
        }
        // parse first arch slice if possible
        if (nfat > 0 && data.size() >= 12) {
            uint32_t off = rd32(data.data() + 8 + 8, false);
            if (off + 32 <= data.size()) {
                // treat as new data span? We'll just set architecture from first
                uint32_t ct = rd32(data.data() + 8, false);
                if (ct == 7 || ct == 0x01000007) info.architecture = (ct == 0x01000007) ? Architecture::X86_64 : Architecture::X86;
                else if (ct == 12 || ct == 0x0100000C) info.architecture = (ct == 0x0100000C) ? Architecture::AArch64 : Architecture::ARM;
                (void)off;
            }
        }
        return info;
    }

    // single arch
    bool le = true;
    bool is64 = false;
    if (magic0 == 0xFEEDFACF || magic0 == 0xCFFAEDFE) is64 = true;
    if (magic0 == 0xCEFAEDFE || magic0 == 0xCFFAEDFE) le = false;
    info.endianness = le ? Endianness::Little : Endianness::Big;

    const uint8_t* h = data.data();
    size_t hdr_size = is64 ? 32 : 28;
    if (data.size() < hdr_size) return Error{ErrorCode::TruncatedInput, "mach header truncated"};

    uint32_t cputype = rd32(h + 4, le);
    uint32_t cpusubtype = rd32(h + 8, le);
    uint32_t filetype = rd32(h + 12, le);
    uint32_t ncmds = rd32(h + 16, le);
    uint32_t sizeofcmds = rd32(h + 20, le);
    (void)cpusubtype; (void)filetype; (void)sizeofcmds;

    switch (cputype) {
        case 7: info.architecture = Architecture::X86; break;
        case 0x01000007: info.architecture = Architecture::X86_64; break;
        case 12: info.architecture = Architecture::ARM; break;
        case 0x0100000C: info.architecture = Architecture::AArch64; break;
        case 18: info.architecture = Architecture::PowerPC; break;
        default: info.architecture = Architecture::Unknown;
    }

    info.metadata["cputype"] = std::to_string(cputype);
    info.metadata["filetype"] = std::to_string(filetype);
    info.metadata["ncmds"] = std::to_string(ncmds);

    // walk load commands
    size_t pos = hdr_size;
    for (uint32_t i = 0; i < ncmds && pos + 8 <= data.size(); ++i) {
        uint32_t cmd = rd32(h + pos, le);
        uint32_t cmdsize = rd32(h + pos + 4, le);
        if (cmdsize == 0 || pos + cmdsize > data.size()) break;
        // LC_LOAD_DYLIB = 0x0C (also 0x0D, 0x0E, 0x0F)
        if (cmd == 0x0C || cmd == 0x0D || cmd == 0x0E || cmd == 0x0F) {
            if (cmdsize >= 24) {
                uint32_t nameoff = rd32(h + pos + 8, le);
                if (nameoff < cmdsize) {
                    std::string name;
                    size_t idx = pos + nameoff;
                    while (idx < data.size() && data[idx] != 0) { name.push_back((char)data[idx]); ++idx; }
                    if (!name.empty()) {
                        info.imports.push_back(name);
                        info.dependencies.push_back(Dependency{name, "", false});
                    }
                }
            }
        }
        // LC_RPATH = 0x8000001c
        if (cmd == 0x8000001c) {
            if (cmdsize >= 16) {
                uint32_t pathoff = rd32(h + pos + 8, le);
                if (pathoff < cmdsize) {
                    std::string p;
                    size_t idx = pos + pathoff;
                    while (idx < data.size() && data[idx] != 0) { p.push_back((char)data[idx]); ++idx; }
                    if (!p.empty()) info.metadata["rpath"] = p;
                }
            }
        }
        pos += cmdsize;
    }

    return info;
}

} // namespace lingua::macho