#include <lingua/format/deb/deb_parser.hpp>
#include <lingua/core/byte_reader.hpp>
#include <lingua/core/error.hpp>

#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#if defined(LINGUA_HAVE_ZLIB)
#include <zlib.h>
#endif
#if defined(LINGUA_HAVE_ZSTD)
#include <zstd.h>
#endif

namespace lingua::deb {

namespace {
struct ArMember { std::string name; std::vector<uint8_t> data; };

std::vector<ArMember> parse_ar(const std::vector<uint8_t>& data) {
    std::vector<ArMember> members;
    if (data.size() < 8) return members;
    if (std::memcmp(data.data(), "!<arch>\n", 8) != 0) return members;
    size_t pos = 8;
    // extended name table
    std::string ext_names;
    while (pos + 60 <= data.size()) {
        // header
        char name[17] = {0}; std::memcpy(name, data.data()+pos, 16);
        char sizebuf[11] = {0}; std::memcpy(sizebuf, data.data()+pos+48, 10);
        size_t sz = 0;
        try { sz = std::stoull(sizebuf); } catch (...) { break; }
        // magic check
        if (data[pos+58] != '`' || data[pos+59] != '\n') {
            // maybe padded; break
        }
        std::string member_name = name;
        // trim trailing spaces
        while (!member_name.empty() && member_name.back() == ' ') member_name.pop_back();
        size_t data_start = pos + 60;
        if (data_start + sz > data.size()) break;

        if (member_name == "/" || member_name == "//") {
            // extended names table if "//"
            if (member_name == "//") {
                ext_names.assign(reinterpret_cast<const char*>(data.data()+data_start), sz);
            }
        } else {
            // handle extended name reference /nnn
            std::string actual = member_name;
            if (!member_name.empty() && member_name[0] == '/') {
                // parse offset
                size_t off = 0;
                try { off = std::stoull(member_name.substr(1)); } catch (...) { off = 0; }
                if (off < ext_names.size()) {
                    size_t end = ext_names.find('/', off);
                    if (end == std::string::npos) end = ext_names.size();
                    actual = ext_names.substr(off, end - off);
                }
            }
            // strip trailing slash
            if (!actual.empty() && actual.back() == '/') actual.pop_back();

            ArMember m;
            m.name = actual;
            m.data.assign(data.data()+data_start, data.data()+data_start+sz);
            members.push_back(m);
        }

        pos = data_start + sz;
        if (sz % 2 == 1) ++pos; // padding
    }
    return members;
}

std::vector<uint8_t> decompress(const std::vector<uint8_t>& in) {
    // gzip
    if (in.size() >= 2 && in[0] == 0x1F && in[1] == 0x8B) {
#if defined(LINGUA_HAVE_ZLIB)
        z_stream strm{};
        strm.next_in = const_cast<Bytef*>(in.data());
        strm.avail_in = in.size();
        if (inflateInit2(&strm, 16 + 15) != Z_OK) return {};
        std::vector<uint8_t> out;
        uint8_t buf[65536];
        int ret;
        do {
            strm.next_out = buf;
            strm.avail_out = sizeof(buf);
            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) { inflateEnd(&strm); return {}; }
            size_t have = sizeof(buf) - strm.avail_out;
            out.insert(out.end(), buf, buf + have);
            if (out.size() > 100 * 1024 * 1024) { inflateEnd(&strm); return {}; } // bomb guard
        } while (ret != Z_STREAM_END);
        inflateEnd(&strm);
        return out;
#else
        return {};
#endif
    }
    // zstd
    if (in.size() >= 4 && in[0] == 0x28 && in[1] == 0xB5 && in[2] == 0x2F && in[3] == 0xFD) {
#if defined(LINGUA_HAVE_ZSTD)
        unsigned long long sz = ZSTD_getFrameContentSize(in.data(), in.size());
        if (sz == ZSTD_CONTENTSIZE_ERROR || sz == ZSTD_CONTENTSIZE_UNKNOWN) return {};
        if (sz > 100 * 1024 * 1024) return {};
        std::vector<uint8_t> out(sz);
        size_t ret = ZSTD_decompress(out.data(), out.size(), in.data(), in.size());
        if (ZSTD_isError(ret)) return {};
        out.resize(ret);
        return out;
#else
        return {};
#endif
    }
    // xz not supported
    return in; // assume uncompressed
}

struct TarEntry { std::string name; std::vector<uint8_t> data; bool is_dir; };

std::vector<TarEntry> parse_tar(const std::vector<uint8_t>& data) {
    std::vector<TarEntry> entries;
    size_t pos = 0;
    while (pos + 512 <= data.size()) {
        // check zero block
        bool allzero = true;
        for (int i = 0; i < 512; ++i) if (data[pos+i] != 0) { allzero = false; break; }
        if (allzero) { pos += 512; continue; }
        if (pos + 512 > data.size()) break;
        const uint8_t* h = data.data() + pos;
        char name[101] = {0}; std::memcpy(name, h, 100);
        char sizebuf[13] = {0}; std::memcpy(sizebuf, h+124, 12);
        // octal size
        size_t sz = 0;
        for (int i = 0; i < 12; ++i) {
            if (sizebuf[i] == ' ' || sizebuf[i] == '\0') break;
            if (sizebuf[i] < '0' || sizebuf[i] > '7') { sz = 0; break; }
            sz = sz * 8 + (sizebuf[i] - '0');
        }
        char typeflag = (char)h[156];
        std::string full_name = name;
        // handle prefix
        char prefix[156] = {0}; std::memcpy(prefix, h+345, 155);
        if (prefix[0]) {
            full_name = std::string(prefix) + "/" + full_name;
        }
        // strip leading ./
        if (full_name.rfind("./", 0) == 0) full_name = full_name.substr(2);

        size_t data_start = pos + 512;
        if (data_start + sz > data.size()) break;

        if (typeflag == '0' || typeflag == '\0' || typeflag == '7') {
            TarEntry e;
            e.name = full_name;
            e.data.assign(data.data()+data_start, data.data()+data_start+sz);
            e.is_dir = false;
            entries.push_back(e);
        } else if (typeflag == '5') {
            TarEntry e;
            e.name = full_name;
            e.is_dir = true;
            entries.push_back(e);
        }

        size_t advance = 512 + ((sz + 511) / 512) * 512;
        pos += advance;
        if (pos == 0) break;
    }
    return entries;
}

std::map<std::string,std::string> parse_control(const std::string& text) {
    std::map<std::string,std::string> fields;
    std::istringstream in(text);
    std::string line;
    std::string current_key;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == ' ' || line[0] == '\t') {
            // continuation
            if (!current_key.empty()) fields[current_key] += "\n" + line;
            continue;
        }
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon+1);
        // trim leading space
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
    auto members = parse_ar(buf);
    if (members.empty()) {
        return Error{ErrorCode::MalformedHeader, "not a valid ar archive"};
    }

    // find control and data tarballs
    std::vector<uint8_t> control_tar, data_tar;
    std::string debian_binary;
    for (auto& m : members) {
        if (m.name == "debian-binary") {
            debian_binary.assign(reinterpret_cast<const char*>(m.data.data()), m.data.size());
        } else if (m.name.rfind("control.tar", 0) == 0) {
            control_tar = decompress(m.data);
            if (control_tar.empty()) control_tar = m.data;
        } else if (m.name.rfind("data.tar", 0) == 0) {
            data_tar = decompress(m.data);
            if (data_tar.empty()) data_tar = m.data;
        }
    }

    info.metadata["debian_binary"] = debian_binary;

    // parse control tar
    if (!control_tar.empty()) {
        auto entries = parse_tar(control_tar);
        for (auto& e : entries) {
            if (e.name == "./control" || e.name == "control" || e.name.ends_with("/control")) {
                std::string text(reinterpret_cast<const char*>(e.data.data()), e.data.size());
                auto fields = parse_control(text);
                for (auto& [k,v] : fields) {
                    info.metadata[k] = v;
                }
                // dependencies
                auto it = fields.find("Depends");
                if (it != fields.end()) {
                    // split by comma
                    std::string s = it->second;
                    size_t start = 0;
                    while (true) {
                        size_t comma = s.find(',', start);
                        std::string part = (comma == std::string::npos) ? s.substr(start) : s.substr(start, comma-start);
                        // trim
                        while (!part.empty() && part.front() == ' ') part.erase(part.begin());
                        while (!part.empty() && part.back() == ' ') part.pop_back();
                        if (!part.empty()) {
                            // name may include version in parens
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

    // data tar entries -> sections (file list)
    if (!data_tar.empty()) {
        auto entries = parse_tar(data_tar);
        for (auto& e : entries) {
            if (e.is_dir) continue;
            Section sec;
            sec.name = e.name;
            sec.size = e.data.size();
            sec.offset = 0;
            info.sections.push_back(sec);
        }
    }

    // architecture
    auto ait = info.metadata.find("Architecture");
    if (ait != info.metadata.end()) {
        std::string a = ait->second;
        if (a == "amd64") info.architecture = Architecture::X86_64;
        else if (a == "i386") info.architecture = Architecture::X86;
        else if (a == "arm64") info.architecture = Architecture::AArch64;
        else if (a == "armhf" || a == "armel") info.architecture = Architecture::ARM;
        else info.architecture = Architecture::Unknown;
    }

    return info;
}

} // namespace lingua::deb