#include <lingua/format/deb/deb_parser.hpp>
#include <lingua/core/archive.hpp>
#include <lingua/core/byte_reader.hpp>
#include <lingua/core/error.hpp>

#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace lingua::deb {

namespace {
std::map<std::string,std::string> parse_control(const std::string& text) {
    std::map<std::string,std::string> fields;
    std::istringstream in(text);
    std::string line;
    std::string current_key;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == ' ' || line[0] == '\t') {
            if (!current_key.empty()) fields[current_key] += "\n" + line;
            continue;
        }
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon+1);
        while (!val.empty() && (val[0] == ' ')) val.erase(val.begin());
        fields[key] = val;
        current_key = key;
    }
    return fields;
}
}

bool DebParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 8) return false;
    return std::memcmp(data.data(), "!<arch>\n", 8) == 0;
}

Result<FileInfo> DebParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info;
    info.path = path;
    info.format = Format::Deb;
    info.file_size = data.size();
    info.endianness = Endianness::Little;

    std::vector<uint8_t> buf(data.begin(), data.end());
    auto members = archive::parse_ar(buf);
    if (members.empty()) {
        return Error{ErrorCode::MalformedHeader, "not a valid ar archive"};
    }

    std::vector<uint8_t> control_tar, data_tar;
    std::string debian_binary;
    for (auto& m : members) {
        if (m.name == "debian-binary") {
            debian_binary.assign(reinterpret_cast<const char*>(m.data.data()), m.data.size());
        } else if (m.name.rfind("control.tar", 0) == 0) {
            control_tar = archive::decompress(m.data);
            if (control_tar.empty()) control_tar = m.data;
        } else if (m.name.rfind("data.tar", 0) == 0) {
            data_tar = archive::decompress(m.data);
            if (data_tar.empty()) data_tar = m.data;
        }
    }

    info.metadata["debian_binary"] = debian_binary;

    if (!control_tar.empty()) {
        auto entries = archive::parse_tar(control_tar);
        for (auto& e : entries) {
            if (e.name == "./control" || e.name == "control" || e.name.ends_with("/control")) {
                std::string text(reinterpret_cast<const char*>(e.data.data()), e.data.size());
                auto fields = parse_control(text);
                for (auto& [k,v] : fields) info.metadata[k] = v;
                auto it = fields.find("Depends");
                if (it != fields.end()) {
                    std::string s = it->second;
                    size_t start = 0;
                    while (true) {
                        size_t comma = s.find(',', start);
                        std::string part = (comma == std::string::npos) ? s.substr(start) : s.substr(start, comma-start);
                        while (!part.empty() && part.front() == ' ') part.erase(part.begin());
                        while (!part.empty() && part.back() == ' ') part.pop_back();
                        if (!part.empty()) {
                            std::string name = part;
                            size_t sp = name.find(' ');
                            if (sp != std::string::npos) name = name.substr(0, sp);
                            info.dependencies.push_back(Dependency{name, part, false});
                        }
                        if (comma == std::string::npos) break;
                        start = comma + 1;
                    }
                }
            }
        }
    }

    if (!data_tar.empty()) {
        auto entries = archive::parse_tar(data_tar);
        for (auto& e : entries) {
            if (e.is_dir) continue;
            Section sec;
            sec.name = e.name;
            sec.size = e.data.size();
            sec.offset = 0;
            info.sections.push_back(sec);
        }
    }

    auto ait = info.metadata.find("Architecture");
    if (ait != info.metadata.end()) {
        std::string a = ait->second;
        if (a == "amd64") info.architecture = Architecture::X86_64;
        else if (a == "i386") info.architecture = Architecture::X86;
        else if (a == "arm64") info.architecture = Architecture::AArch64;
        else if (a == "armhf" || a == "armel") info.architecture = Architecture::ARM;
    }

    return info;
}

} // namespace lingua::deb