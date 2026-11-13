#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace lingua {

enum class Format {
    Unknown = 0,
    PE,
    ELF,
    Deb,
    AppImage,
    MachO, // not yet implemented
    Rpm,   // not yet implemented
    Apk,   // not yet implemented
    Msi,   // not yet implemented
    Flatpak // not yet implemented
};

enum class Architecture {
    Unknown = 0,
    X86,
    X86_64,
    ARM,
    AArch64,
    MIPS,
    PowerPC,
    RISCV,
    Itanium,
};

enum class Endianness {
    Unknown = 0,
    Little,
    Big,
};

enum class HostOS {
    Unknown = 0,
    Windows,
    Linux,
    MacOS,
    FreeBSD,
};

struct Section {
    std::string name;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint64_t virtual_address = 0;
    std::string type;
};

struct Dependency {
    std::string name;
    std::string version_constraint;
    bool optional = false;
};

struct FileInfo {
    std::filesystem::path path;
    Format format = Format::Unknown;
    Architecture architecture = Architecture::Unknown;
    Endianness endianness = Endianness::Unknown;
    std::uint64_t file_size = 0;
    std::string sha256;
    std::map<std::string, std::string> metadata;
    std::vector<Section> sections;
    std::vector<Dependency> dependencies;
    std::vector<std::string> imports;
    std::vector<std::string> exports;
    std::vector<std::string> warnings;
};

inline const char* to_string(Format f) {
    switch (f) {
        case Format::Unknown: return "unknown";
        case Format::PE: return "pe";
        case Format::ELF: return "elf";
        case Format::Deb: return "deb";
        case Format::AppImage: return "appimage";
        case Format::MachO: return "macho";
        case Format::Rpm: return "rpm";
        case Format::Apk: return "apk";
        case Format::Msi: return "msi";
        case Format::Flatpak: return "flatpak";
    }
    return "unknown";
}

inline const char* to_string(Architecture a) {
    switch (a) {
        case Architecture::Unknown: return "unknown";
        case Architecture::X86: return "x86";
        case Architecture::X86_64: return "x86_64";
        case Architecture::ARM: return "arm";
        case Architecture::AArch64: return "aarch64";
        case Architecture::MIPS: return "mips";
        case Architecture::PowerPC: return "powerpc";
        case Architecture::RISCV: return "riscv";
        case Architecture::Itanium: return "itanium";
    }
    return "unknown";
}

} // namespace lingua