#include <lingua/format/elf/elf_parser.hpp>
#include <lingua/core/byte_reader.hpp>
#include <lingua/core/error.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace lingua::elf {

namespace {
struct ElfCtx {
    bool is64;
    bool little;
    // header fields
    uint8_t ei_class;
    uint8_t ei_data;
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

uint16_t rd16(const uint8_t* p, bool le) {
    if (le) return p[0] | (p[1] << 8);
    return (p[0] << 8) | p[1];
}
uint32_t rd32(const uint8_t* p, bool le) {
    if (le) return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
    return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3];
}
uint64_t rd64(const uint8_t* p, bool le) {
    uint64_t v = 0;
    if (le) for (int i=7;i>=0;--i) v = (v<<8) | p[i];
    else for (int i=0;i<8;++i) v = (v<<8) | p[i];
    return v;
}
}

bool ElfParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 16) return false;
    return data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F';
}

Result<FileInfo> ElfParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info;
    info.path = path;
    info.format = Format::ELF;
    info.file_size = data.size();

    if (data.size() < 52) {
        return Error{ErrorCode::TruncatedInput, "ELF header truncated"};
    }

    ElfCtx ctx{};
    ctx.ei_class = data[4];
    ctx.ei_data = data[5];
    if (ctx.ei_class != 1 && ctx.ei_class != 2) {
        return Error{ErrorCode::MalformedHeader, "bad ELF class"};
    }
    if (ctx.ei_data != 1 && ctx.ei_data != 2) {
        return Error{ErrorCode::MalformedHeader, "bad ELF data encoding"};
    }
    ctx.is64 = (ctx.ei_class == 2);
    ctx.little = (ctx.ei_data == 1);
    info.endianness = ctx.little ? Endianness::Little : Endianness::Big;

    const uint8_t* h = data.data();
    if (ctx.is64) {
        if (data.size() < 64) return Error{ErrorCode::TruncatedInput, "ELF64 header truncated"};
        ctx.e_type = rd16(h+16, ctx.little);
        ctx.e_machine = rd16(h+18, ctx.little);
        ctx.e_version = rd32(h+20, ctx.little);
        ctx.e_entry = rd64(h+24, ctx.little);
        ctx.e_phoff = rd64(h+32, ctx.little);
        ctx.e_shoff = rd64(h+40, ctx.little);
        ctx.e_flags = rd32(h+48, ctx.little);
        ctx.e_ehsize = rd16(h+52, ctx.little);
        ctx.e_phentsize = rd16(h+54, ctx.little);
        ctx.e_phnum = rd16(h+56, ctx.little);
        ctx.e_shentsize = rd16(h+58, ctx.little);
        ctx.e_shnum = rd16(h+60, ctx.little);
        ctx.e_shstrndx = rd16(h+62, ctx.little);
    } else {
        ctx.e_type = rd16(h+16, ctx.little);
        ctx.e_machine = rd16(h+18, ctx.little);
        ctx.e_version = rd32(h+20, ctx.little);
        ctx.e_entry = rd32(h+24, ctx.little);
        ctx.e_phoff = rd32(h+28, ctx.little);
        ctx.e_shoff = rd32(h+32, ctx.little);
        ctx.e_flags = rd32(h+36, ctx.little);
        ctx.e_ehsize = rd16(h+40, ctx.little);
        ctx.e_phentsize = rd16(h+42, ctx.little);
        ctx.e_phnum = rd16(h+44, ctx.little);
        ctx.e_shentsize = rd16(h+46, ctx.little);
        ctx.e_shnum = rd16(h+48, ctx.little);
        ctx.e_shstrndx = rd16(h+50, ctx.little);
    }

    // architecture
    switch (ctx.e_machine) {
        case 3: info.architecture = Architecture::X86; break;
        case 62: info.architecture = Architecture::X86_64; break;
        case 40: info.architecture = Architecture::ARM; break;
        case 183: info.architecture = Architecture::AArch64; break;
        case 8: info.architecture = Architecture::MIPS; break;
        case 20: info.architecture = Architecture::PowerPC; break;
        case 243: info.architecture = Architecture::RISCV; break;
        default: info.architecture = Architecture::Unknown; info.warnings.push_back("unknown machine");
    }
    info.metadata["e_type"] = std::to_string(ctx.e_type);
    info.metadata["e_machine"] = std::to_string(ctx.e_machine);
    info.metadata["class"] = ctx.is64 ? "64" : "32";

    uint32_t shnum = ctx.e_shnum;
    uint32_t shstrndx = ctx.e_shstrndx;
    uint64_t shoff = ctx.e_shoff;
    (void)shoff;
    // extended section count handling omitted for simplicity

    // Section names string table
    std::vector<char> shstrtab;
    size_t shstr_offset = 0, shstr_size = 0;

    // First pass: find shstrtab section header
    if (ctx.e_shoff != 0 && shnum > 0 && ctx.e_shentsize > 0) {
        // read shstrndx entry
        size_t idx_off = static_cast<size_t>(ctx.e_shoff) + static_cast<size_t>(shstrndx) * ctx.e_shentsize;
        if (ctx.is64) {
            if (idx_off + 64 <= data.size()) {
                uint64_t sh_offset = rd64(data.data()+idx_off+24, ctx.little);
                uint64_t sh_size = rd64(data.data()+idx_off+32, ctx.little);
                if (sh_offset + sh_size <= data.size()) {
                    shstr_offset = static_cast<size_t>(sh_offset);
                    shstr_size = static_cast<size_t>(sh_size);
                }
            }
        } else {
            if (idx_off + 40 <= data.size()) {
                uint32_t sh_offset = rd32(data.data()+idx_off+16, ctx.little);
                uint32_t sh_size = rd32(data.data()+idx_off+20, ctx.little);
                if ((size_t)sh_offset + sh_size <= data.size()) {
                    shstr_offset = sh_offset;
                    shstr_size = sh_size;
                }
            }
        }
    }

    auto get_name = [&](uint32_t name_off) -> std::string {
        if (shstr_offset == 0 || name_off >= shstr_size) return "";
        std::string s;
        size_t i = shstr_offset + name_off;
        while (i < data.size() && i < shstr_offset + shstr_size && data[i] != 0) { s.push_back((char)data[i]); ++i; }
        return s;
    };

    // Sections
    for (uint32_t i = 0; i < shnum; ++i) {
        size_t off = static_cast<size_t>(ctx.e_shoff) + i * ctx.e_shentsize;
        if (ctx.is64) {
            if (off + 64 > data.size()) break;
            uint32_t sh_name = rd32(data.data()+off, ctx.little);
            uint32_t sh_type = rd32(data.data()+off+4, ctx.little);
            uint64_t sh_addr = rd64(data.data()+off+16, ctx.little);
            uint64_t sh_offset = rd64(data.data()+off+24, ctx.little);
            uint64_t sh_size = rd64(data.data()+off+32, ctx.little);
            Section sec;
            sec.name = get_name(sh_name);
            sec.offset = sh_offset;
            sec.size = sh_size;
            sec.virtual_address = sh_addr;
            sec.type = std::to_string(sh_type);
            info.sections.push_back(sec);
        } else {
            if (off + 40 > data.size()) break;
            uint32_t sh_name = rd32(data.data()+off, ctx.little);
            uint32_t sh_type = rd32(data.data()+off+4, ctx.little);
            uint32_t sh_addr = rd32(data.data()+off+12, ctx.little);
            uint32_t sh_offset = rd32(data.data()+off+16, ctx.little);
            uint32_t sh_size = rd32(data.data()+off+20, ctx.little);
            Section sec;
            sec.name = get_name(sh_name);
            sec.offset = sh_offset;
            sec.size = sh_size;
            sec.virtual_address = sh_addr;
            sec.type = std::to_string(sh_type);
            info.sections.push_back(sec);
        }
    }

    // Dynamic section: find PT_DYNAMIC program header or section type 6
    // Search sections named .dynamic or type 6
    int dynamic_section_index = -1;
    for (size_t i = 0; i < info.sections.size(); ++i) {
        // We stored type as string of sh_type; check original? We'll just name match
        if (info.sections[i].name == ".dynamic") dynamic_section_index = (int)i;
    }

    // Parse dynamic entries if found by scanning sections for type 6 (we didn't keep type int). Let's re-scan.
    // Simpler: iterate program headers for PT_DYNAMIC
    uint64_t dyn_off = 0, dyn_sz = 0;
    if (ctx.e_phoff != 0 && ctx.e_phnum > 0) {
        for (uint32_t i = 0; i < ctx.e_phnum; ++i) {
            size_t ph = static_cast<size_t>(ctx.e_phoff) + i * ctx.e_phentsize;
            if (ctx.is64) {
                if (ph + 56 > data.size()) break;
                uint32_t p_type = rd32(data.data()+ph, ctx.little);
                if (p_type == 2) { // PT_DYNAMIC
                    dyn_off = rd64(data.data()+ph+8, ctx.little); // offset? for 64: p_offset at +8
                    dyn_sz = rd64(data.data()+ph+32, ctx.little); // p_filesz at +32? Actually p_filesz offset 32
                    // ELF64 Phdr: type(0) flags(4) offset(8) vaddr(16) paddr(24) filesz(32) memsz(40)
                }
            } else {
                if (ph + 32 > data.size()) break;
                uint32_t p_type = rd32(data.data()+ph, ctx.little);
                if (p_type == 2) {
                    dyn_off = rd32(data.data()+ph+4, ctx.little); // offset at +4
                    dyn_sz = rd32(data.data()+ph+16, ctx.little); // filesz at +16
                }
            }
        }
    }

    // Need convert virtual offset? dyn_off is file offset for PT_DYNAMIC? In ELF, p_offset is file offset. Good.
    if (dyn_off != 0 && dyn_sz != 0 && dyn_off + dyn_sz <= data.size()) {
        size_t entry_size = ctx.is64 ? 16 : 8;
        size_t num = dyn_sz / entry_size;
        // string table for dynamic: find DT_STRTAB; but we need offset. We'll collect NEEDED names using a second pass:
        // First gather entries
        std::vector<std::pair<uint64_t,uint64_t>> entries;
        for (size_t i = 0; i < num; ++i) {
            size_t base = static_cast<size_t>(dyn_off) + i*entry_size;
            if (ctx.is64) {
                uint64_t tag = rd64(data.data()+base, ctx.little);
                uint64_t val = rd64(data.data()+base+8, ctx.little);
                entries.push_back({tag,val});
            } else {
                uint32_t tag = rd32(data.data()+base, ctx.little);
                uint32_t val = rd32(data.data()+base+4, ctx.little);
                entries.push_back({tag,val});
            }
        }
        // Find strtab address and convert to file offset. Need map virtual->file. We'll build from sections: for a virtual addr, find section containing it.
        auto va_to_off = [&](uint64_t va) -> size_t {
            for (auto& sec : info.sections) {
                if (va >= sec.virtual_address && va < sec.virtual_address + sec.size) {
                    return sec.offset + (va - sec.virtual_address);
                }
            }
            return 0;
        };
        uint64_t strtab_va = 0;
        for (auto& e : entries) {
            if (e.first == 5) { strtab_va = e.second; break; } // DT_STRTAB
        }
        size_t strtab_off = va_to_off(strtab_va);
        for (auto& e : entries) {
            if (e.first == 1) { // DT_NEEDED
                if (strtab_off != 0) {
                    std::string name;
                    size_t i = strtab_off + e.second;
                    while (i < data.size() && data[i] != 0) { name.push_back((char)data[i]); ++i; }
                    if (!name.empty()) {
                        info.imports.push_back(name);
                        info.dependencies.push_back(Dependency{name, "", false});
                    }
                }
            }
        }
    }

    return info;
}

} // namespace lingua::elf