#include <lingua/format/apk/apk_parser.hpp>
#include <lingua/core/archive.hpp>
#include <lingua/core/error.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace lingua::apk {

bool ApkParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 4) return false;
    if (!(data[0] == 0x50 && data[1] == 0x4B && data[2] == 0x03 && data[3] == 0x04)) return false;
    // look for apk markers
    std::string blob(data.begin(), data.end());
    if (blob.find("AndroidManifest.xml") != std::string::npos) return true;
    if (blob.find("classes.dex") != std::string::npos) return true;
    return false;
}

Result<FileInfo> ApkParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info;
    info.path = path;
    info.format = Format::Apk;
    info.file_size = data.size();
    info.endianness = Endianness::Little;

    std::vector<uint8_t> buf(data.begin(), data.end());
    auto entries = archive::parse_zip(buf);
    if (entries.empty()) {
        return Error{ErrorCode::MalformedHeader, "not a valid zip/apk"};
    }

    info.metadata["entry_count"] = std::to_string(entries.size());
    for (auto& e : entries) {
        if (e.is_dir) continue;
        Section sec;
        sec.name = e.name;
        sec.size = e.data.size();
        info.sections.push_back(sec);
        if (e.name.rfind("lib/", 0) == 0 && e.name.find(".so") != std::string::npos) {
            info.imports.push_back(e.name);
        }
    }
    // architecture guess from lib dirs
    for (auto& e : entries) {
        if (e.name.rfind("lib/arm64", 0) == 0) { info.architecture = Architecture::AArch64; break; }
        if (e.name.rfind("lib/armeabi", 0) == 0) { info.architecture = Architecture::ARM; break; }
        if (e.name.rfind("lib/x86_64", 0) == 0) { info.architecture = Architecture::X86_64; break; }
        if (e.name.rfind("lib/x86", 0) == 0) { info.architecture = Architecture::X86; break; }
    }
    info.warnings.push_back("apk metadata extraction is minimal");
    return info;
}

} // namespace lingua::apk