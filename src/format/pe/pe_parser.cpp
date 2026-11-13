#include <lingua/format/pe/pe_parser.hpp>
#include <lingua/core/byte_reader.hpp>
#include <lingua/core/error.hpp>
#include <lingua/detect/detector.hpp>

#include <algorithm>
#include <cstring>
#include <set>

namespace lingua::pe {

namespace {
bool detect_pe(std::span<const std::uint8_t> data) {
    if (data.size() < 64) return false;
    if (data[0] != 'M' || data[1] != 'Z') return false;
    // e_lfanew at 0x3C
    uint32_t e_lfanew = data[0x3C] | (data[0x3D] << 8) | (data[0x3E] << 16) | (data[0x3F] << 24);
    if (e_lfanew + 4 > data.size()) return false;
    if (data[e_lfanew] != 'P' || data[e_lfanew+1] != 'E' || data[e_lfanew+2] != 0 || data[e_lfanew+3] != 0) return false;
    return true;
}
}

bool PeParser::detect(std::span<const std::uint8_t> data) const {
    return detect_pe(data);
}

Result<FileInfo> PeParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info;
    info.path = path;
    info.format = Format::PE;
    info.file_size = data.size();

    if (data.size() < 64) {
        info.warnings.push_back("file too small");
        return info;
    }

    ByteReader r(data);
    // DOS header
    auto mz = r.read_u16_le();
    if (!mz || *mz != 0x5A4D) {
        return Error{ErrorCode::MalformedHeader, "not a PE (missing MZ)"};
    }
    r.set_offset(0x3C);
    auto e_lfanew_r = r.read_u32_le();
    if (!e_lfanew_r) return Error{ErrorCode::TruncatedInput, "truncated DOS header"};
    uint32_t e_lfanew = *e_lfanew_r;
    if (static_cast<size_t>(e_lfanew) + 24 > data.size()) {
        return Error{ErrorCode::TruncatedInput, "invalid e_lfanew"};
    }

    // NT signature
    if (data[e_lfanew] != 'P' || data[e_lfanew+1] != 'E' || data[e_lfanew+2] != 0 || data[e_lfanew+3] != 0) {
        return Error{ErrorCode::MalformedHeader, "missing PE signature"};
    }

    size_t nt = e_lfanew + 4;
    // COFF header
    if (nt + 20 > data.size()) return Error{ErrorCode::TruncatedInput, "truncated COFF header"};
    uint16_t machine = data[nt] | (data[nt+1] << 8);
    uint16_t num_sections = data[nt+2] | (data[nt+3] << 8);
    // uint32_t timestamp = ...
    uint16_t size_opt = data[nt+16] | (data[nt+17] << 8);
    uint16_t characteristics = data[nt+18] | (data[nt+19] << 8);

    // architecture
    switch (machine) {
        case 0x014c: info.architecture = Architecture::X86; break;
        case 0x8664: info.architecture = Architecture::X86_64; break;
        case 0x01c0: info.architecture = Architecture::ARM; break;
        case 0xaa64: info.architecture = Architecture::AArch64; break;
        case 0x0200: info.architecture = Architecture::Itanium; break;
        default: info.architecture = Architecture::Unknown; info.warnings.push_back("unknown machine type");
    }
    info.endianness = Endianness::Little;
    info.metadata["machine"] = std::to_string(machine);
    info.metadata["characteristics"] = std::to_string(characteristics);
    info.metadata["num_sections"] = std::to_string(num_sections);

    size_t opt_offset = nt + 20;
    if (opt_offset + size_opt > data.size()) {
        info.warnings.push_back("optional header truncated");
        return info;
    }

    // Optional header magic
    uint16_t opt_magic = 0;
    if (size_opt >= 2) opt_magic = data[opt_offset] | (data[opt_offset+1] << 8);
    bool is_pe32plus = (opt_magic == 0x20b);
    info.metadata["pe32plus"] = is_pe32plus ? "true" : "false";

    // Data directories: offset in optional header
    // PE32: data dir at opt+96, PE32+: opt+112
    size_t dd_offset = is_pe32plus ? opt_offset + 112 : opt_offset + 96;
    uint32_t import_rva = 0, import_size = 0;
    uint32_t export_rva = 0, export_size = 0;
    uint32_t security_rva = 0, security_size = 0;
    uint32_t clr_rva = 0, clr_size = 0;
    // Data directories
    auto read_dd = [&](int idx, uint32_t& rva, uint32_t& sz) {
        size_t entry = dd_offset + idx * 8;
        if (entry + 8 <= data.size()) {
            rva = data[entry] | (data[entry+1]<<8) | (data[entry+2]<<16) | (data[entry+3]<<24);
            sz = data[entry+4] | (data[entry+5]<<8) | (data[entry+6]<<16) | (data[entry+7]<<24);
        }
    };
    read_dd(0, export_rva, export_size);
    read_dd(1, import_rva, import_size);
    read_dd(4, security_rva, security_size);
    read_dd(14, clr_rva, clr_size);

    // Section headers
    size_t section_offset = opt_offset + size_opt;
    // RVA to file offset mapping
    struct Sec { uint32_t va, vsz, raw, rsz; std::string name; };
    std::vector<Sec> secs;
    for (int i = 0; i < num_sections; ++i) {
        size_t sh = section_offset + i * 40;
        if (sh + 40 > data.size()) { info.warnings.push_back("truncated section header"); break; }
        Sec s;
        char namebuf[9] = {0};
        std::memcpy(namebuf, data.data() + sh, 8);
        s.name = namebuf;
        // virtual size at +8, virtual addr at +12, raw size at +16, raw ptr at +20
        s.vsz = data[sh+8] | (data[sh+9]<<8) | (data[sh+10]<<16) | (data[sh+11]<<24);
        s.va  = data[sh+12] | (data[sh+13]<<8) | (data[sh+14]<<16) | (data[sh+15]<<24);
        s.rsz = data[sh+16] | (data[sh+17]<<8) | (data[sh+18]<<16) | (data[sh+19]<<24);
        s.raw = data[sh+20] | (data[sh+21]<<8) | (data[sh+22]<<16) | (data[sh+23]<<24);
        secs.push_back(s);

        Section sec;
        sec.name = s.name;
        sec.virtual_address = s.va;
        sec.size = s.vsz ? s.vsz : s.rsz;
        sec.offset = s.raw;
        info.sections.push_back(sec);
    }

    auto rva_to_offset = [&](uint32_t rva) -> size_t {
        for (auto& s : secs) {
            if (rva >= s.va && rva < s.va + (s.vsz ? s.vsz : s.rsz)) {
                return s.raw + (rva - s.va);
            }
        }
        return 0;
    };

    // Imports
    if (import_rva != 0) {
        size_t imp_off = rva_to_offset(import_rva);
        if (imp_off != 0) {
            // iterate import descriptors (20 bytes each) until all zeros
            size_t cur = imp_off;
            std::set<std::string> dlls;
            while (cur + 20 <= data.size()) {
                uint32_t name_rva = data[cur+12] | (data[cur+13]<<8) | (data[cur+14]<<16) | (data[cur+15]<<24);
                if (name_rva == 0) break;
                size_t name_off = rva_to_offset(name_rva);
                if (name_off != 0) {
                    std::string dll;
                    while (name_off < data.size() && data[name_off] != 0) { dll.push_back((char)data[name_off]); ++name_off; }
                    if (!dll.empty()) {
                        dlls.insert(dll);
                        info.imports.push_back(dll);
                    }
                }
                cur += 20;
            }
            // dependencies
            for (auto& d : dlls) info.dependencies.push_back(Dependency{d, "", false});
        }
    }

    // Exports
    if (export_rva != 0) {
        size_t exp_off = rva_to_offset(export_rva);
        if (exp_off != 0 && exp_off + 40 <= data.size()) {
            uint32_t num_funcs = data[exp_off+20] | (data[exp_off+21]<<8) | (data[exp_off+22]<<16) | (data[exp_off+23]<<24);
            uint32_t num_names = data[exp_off+24] | (data[exp_off+25]<<8) | (data[exp_off+26]<<16) | (data[exp_off+27]<<24);
            uint32_t addr_funcs = data[exp_off+28] | (data[exp_off+29]<<8) | (data[exp_off+30]<<16) | (data[exp_off+31]<<24);
            uint32_t addr_names = data[exp_off+32] | (data[exp_off+33]<<8) | (data[exp_off+34]<<16) | (data[exp_off+35]<<24);
            (void)num_funcs;
            info.metadata["export_count"] = std::to_string(num_names);
            // read export names
            size_t funcs_off = rva_to_offset(addr_funcs);
            size_t names_off = rva_to_offset(addr_names);
            for (uint32_t i = 0; i < num_names && i < 10000; ++i) {
                if (names_off == 0 || names_off + i*4 + 4 > data.size()) break;
                uint32_t name_rva = data[names_off + i*4] | (data[names_off + i*4+1]<<8) | (data[names_off + i*4+2]<<16) | (data[names_off + i*4+3]<<24);
                size_t noff = rva_to_offset(name_rva);
                if (noff != 0) {
                    std::string en;
                    while (noff < data.size() && data[noff] != 0) { en.push_back((char)data[noff]); ++noff; }
                    if (!en.empty()) info.exports.push_back(en);
                }
            }
            (void)funcs_off;
        }
    }

    info.metadata["authenticode"] = (security_rva != 0) ? "present" : "absent";
    info.metadata["dotnet"] = (clr_rva != 0) ? "true" : "false";
    info.metadata["import_rva"] = std::to_string(import_rva);
    return info;
}

} // namespace lingua::pe