#include <lingua/detect/detector.hpp>
#include <lingua/core/byte_reader.hpp>

#include <algorithm>
#include <cstring>
#include <cstdio>

namespace lingua {

Format detect_format(std::span<const std::uint8_t> data) {
    if (data.size() < 4) return Format::Unknown;
    // PE
    if (data.size() >= 2 && data[0] == 'M' && data[1] == 'Z') return Format::PE;
    // ELF
    if (data.size() >= 4 && data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
        for (size_t i = 0; i + 4 <= data.size(); ++i) {
            if (data[i] == 'h' && data[i+1] == 's' && data[i+2] == 'q' && data[i+3] == 's') return Format::AppImage;
            if (i + 5 <= data.size() && data[i] == 'C' && data[i+1] == 'D' && data[i+2] == '0' && data[i+3] == '0' && data[i+4] == '1') return Format::AppImage;
        }
        return Format::ELF;
    }
    // deb: ar archive
    if (data.size() >= 8 && std::memcmp(data.data(), "!<arch>\n", 8) == 0) return Format::Deb;
    // Mach-O
    if (data.size() >= 4) {
        uint32_t magic = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        if (magic == 0xFEEDFACE || magic == 0xFEEDFACF || magic == 0xCEFAEDFE || magic == 0xCFFAEDFE) return Format::MachO;
        if (magic == 0xBEBAFECA) return Format::MachO; // fat
    }
    // RPM
    if (data.size() >= 4 && data[0] == 0xED && data[1] == 0xAB && data[2] == 0xEE && data[3] == 0xDB) return Format::Rpm;
    // APK (zip with apk markers)
    if (data.size() >= 4 && data[0] == 0x50 && data[1] == 0x4B && data[2] == 0x03 && data[3] == 0x04) {
        std::string blob(data.begin(), data.end());
        if (blob.find("AndroidManifest.xml") != std::string::npos || blob.find("classes.dex") != std::string::npos) return Format::Apk;
    }
    return Format::Unknown;
}

std::string detect_mime(std::span<const std::uint8_t> data) {
    Format f = detect_format(data);
    switch (f) {
        case Format::PE: return "application/x-msdownload";
        case Format::ELF: return "application/x-executable";
        case Format::Deb: return "application/vnd.debian.binary-package";
        case Format::AppImage: return "application/x-appimage";
        default: return "application/octet-stream";
    }
}

bool is_text(std::span<const std::uint8_t> data) {
    for (auto b : data) {
        if (b == 0) return false;
        if (b < 0x09) return false;
        if (b > 0x7E && b < 0x20) return false;
    }
    return true;
}

} // namespace lingua