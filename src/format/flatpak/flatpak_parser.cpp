#include <lingua/format/flatpak/flatpak_parser.hpp>
#include <lingua/core/archive.hpp>
#include <lingua/core/error.hpp>

#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace lingua::flatpak {

bool FlatpakParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 262) return false;
    if (std::memcmp(data.data()+257, "ustar", 5) != 0) return false;
    // search span directly
    const char* n1 = "flatpak";
    const char* n2 = "org.flatpak";
    auto find = [&](const char* n) {
        size_t ln = std::strlen(n);
        for (size_t i = 0; i + ln <= data.size(); ++i) {
            if (std::memcmp(data.data() + i, n, ln) == 0) return true;
        }
        return false;
    };
    return find(n1) || find(n2);
}

Result<FileInfo> FlatpakParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info; info.path=path; info.format=Format::Flatpak; info.file_size=data.size();
    std::vector<uint8_t> buf(data.begin(), data.end());
    auto entries = archive::parse_tar(buf);
    info.metadata["entry_count"] = std::to_string(entries.size());
    for (auto& e : entries) {
        if (e.is_dir) continue;
        Section sec; sec.name=e.name; sec.size=e.data.size(); info.sections.push_back(sec);
        if (e.name == "metadata") {
            std::string text(e.data.begin(), e.data.end());
            // parse key=value lines
            std::istringstream in(text);
            std::string line;
            while (std::getline(in, line)) {
                size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string k = line.substr(0, eq);
                    std::string v = line.substr(eq+1);
                    info.metadata[k] = v;
                }
            }
        }
    }
    info.warnings.push_back("Flatpak extraction uses tar entries");
    return info;
}

} // namespace lingua::flatpak