#include <lingua/format/apk/apk_parser.hpp>
#include <lingua/core/archive.hpp>
#include <lingua/core/error.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace lingua::apk {

namespace {
uint16_t rd16(const uint8_t* p){ return p[0] | (p[1]<<8); }
uint32_t rd32(const uint8_t* p){ return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24); }

struct StringPool {
    std::vector<std::string> strings;
};

// Parse AXML string pool
bool parse_string_pool(const uint8_t* data, size_t size, size_t& pos, StringPool& pool) {
    // chunk header already consumed? We'll parse chunk at pos
    if (pos + 8 > size) return false;
    uint16_t type = rd16(data + pos);
    uint16_t hdrSize = rd16(data + pos + 2);
    uint32_t chunkSize = rd32(data + pos + 4);
    if (type != 0x00080001) return false;
    if (pos + chunkSize > size) return false;
    // string pool header fields at pos+8
    uint32_t stringCount = rd32(data + pos + 8);
    uint32_t styleCount = rd32(data + pos + 12);
    uint32_t flags = rd32(data + pos + 16);
    uint32_t stringsStart = rd32(data + pos + 20);
    uint32_t stylesStart = rd32(data + pos + 24);
    (void)styleCount; (void)stylesStart;
    pool.strings.resize(stringCount);
    bool utf8 = (flags & 0x00000100) != 0;
    const uint8_t* strData = data + pos + stringsStart;
    // offsets array at pos+28
    const uint32_t* offsets = reinterpret_cast<const uint32_t*>(data + pos + 28);
    for (uint32_t i = 0; i < stringCount; ++i) {
        uint32_t off = rd32(reinterpret_cast<const uint8_t*>(offsets + i));
        const uint8_t* sp = strData + off;
        if (utf8) {
            // utf8: length in chars (2 bytes? actually u8 length followed by byte length)
            size_t idx = 0;
            // skip char length (1 or 2 bytes)
            if (sp[idx] & 0x80) idx += 2; else idx += 1;
            uint32_t byteLen = sp[idx];
            if (byteLen & 0x80) { byteLen = ((byteLen & 0x7F) << 8) | sp[idx+1]; idx += 2; } else idx += 1;
            pool.strings[i].assign(reinterpret_cast<const char*>(sp + idx), byteLen);
        } else {
            uint32_t charLen = rd16(sp);
            size_t idx = 2;
            if (charLen & 0x8000) { charLen = ((charLen & 0x7FFF) << 16) | rd16(sp + 2); idx = 4; }
            // UTF-16
            std::string s;
            for (uint32_t c = 0; c < charLen; ++c) {
                uint16_t ch = rd16(sp + idx + c*2);
                if (ch < 0x80) s.push_back((char)ch);
                else if (ch < 0x800) { s.push_back(0xC0 | (ch>>6)); s.push_back(0x80 | (ch&0x3F)); }
                else { s.push_back(0xE0 | (ch>>12)); s.push_back(0x80 | ((ch>>6)&0x3F)); s.push_back(0x80 | (ch&0x3F)); }
            }
            pool.strings[i] = s;
        }
    }
    pos += chunkSize;
    return true;
}

std::string get_str(const StringPool& pool, int32_t idx) {
    if (idx < 0 || (size_t)idx >= pool.strings.size()) return {};
    return pool.strings[idx];
}

// typed value extraction
int32_t typed_int(const uint8_t* attr) {
    // attr typed value at offset 16? attr: ns(4) name(4) rawValue(4) typedValue(8): type(2?) Actually typed value: size(2)? Let's use last 4 bytes as data.
    return (int32_t)rd32(attr + 16);
}
}

bool ApkParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 4) return false;
    if (!(data[0] == 0x50 && data[1] == 0x4B && data[2] == 0x03 && data[3] == 0x04)) return false;
    // search span directly
    const char* needle1 = "AndroidManifest.xml";
    const char* needle2 = "classes.dex";
    auto search = [&](const char* n) {
        size_t ln = std::strlen(n);
        for (size_t i = 0; i + ln <= data.size(); ++i) {
            if (std::memcmp(data.data() + i, n, ln) == 0) return true;
        }
        return false;
    };
    return search(needle1) || search(needle2);
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
    for (auto& e : entries) {
        if (e.name.rfind("lib/arm64", 0) == 0) { info.architecture = Architecture::AArch64; break; }
        if (e.name.rfind("lib/armeabi", 0) == 0) { info.architecture = Architecture::ARM; break; }
        if (e.name.rfind("lib/x86_64", 0) == 0) { info.architecture = Architecture::X86_64; break; }
        if (e.name.rfind("lib/x86", 0) == 0) { info.architecture = Architecture::X86; break; }
    }

    // Decode AndroidManifest.xml
    for (auto& e : entries) {
        if (e.name == "AndroidManifest.xml") {
            // parse AXML
            const uint8_t* d = e.data.data();
            size_t size = e.data.size();
            size_t pos = 0;
            if (size < 8) break;
            // file magic
            if (rd32(d) != 0x00080003) break;
            // skip file header (8 bytes) +? Actually AXML file header is 8 bytes: magic(4), size(4)
            pos = 8;
            StringPool pool;
            // iterate chunks
            std::string packageName, versionName;
            int versionCode = 0, minSdk = 0;
            std::vector<std::string> permissions;
            while (pos + 8 <= size) {
                uint16_t type = rd16(d + pos);
                uint16_t hdrSize = rd16(d + pos + 2);
                uint32_t chunkSize = rd32(d + pos + 4);
                if (chunkSize == 0 || pos + chunkSize > size) break;
                if (type == 0x00080001) {
                    parse_string_pool(d, size, pos, pool);
                    continue; // pos advanced
                }
                if (type == 0x00080006) { // start element
                    // ns at pos+8? element chunk: ns(4), name(4), attrStart? Let's parse according to AXML spec:
                    // chunk: type(2), headerSize(2), size(4), ns(4), name(4), attrStart(2), attrSize(2), attrCount(2), idIndex(2), classIndex(2), styleIndex(2)
                    uint32_t nsIdx = rd32(d + pos + 8);
                    uint32_t nameIdx = rd32(d + pos + 12);
                    uint16_t attrStart = rd16(d + pos + 16);
                    uint16_t attrSize = rd16(d + pos + 18);
                    uint16_t attrCount = rd16(d + pos + 20);
                    (void)nsIdx; (void)attrStart; (void)attrSize;
                    std::string elemName = get_str(pool, (int32_t)nameIdx);
                    size_t attrPos = pos + 28; // attributes start? headerSize usually 28
                    // Actually attributes start at pos + headerSize? headerSize=28, attrStart is offset from start of element chunk? Use pos + 28.
                    for (int a = 0; a < attrCount; ++a) {
                        const uint8_t* ap = d + pos + 28 + a * 20;
                        if (ap + 20 > d + pos + chunkSize) break;
                        int32_t aNs = (int32_t)rd32(ap);
                        int32_t aName = (int32_t)rd32(ap + 4);
                        int32_t rawVal = (int32_t)rd32(ap + 8);
                        uint32_t typedVal = rd32(ap + 16);
                        (void)aNs;
                        std::string attrName = get_str(pool, aName);
                        if (elemName == "manifest") {
                            if (attrName == "package") {
                                packageName = get_str(pool, rawVal);
                            } else if (attrName == "versionCode") {
                                versionCode = (int32_t)typedVal;
                            } else if (attrName == "versionName") {
                                versionName = get_str(pool, rawVal);
                            }
                        } else if (elemName == "uses-sdk") {
                            if (attrName == "minSdkVersion") {
                                minSdk = (int32_t)typedVal;
                            }
                        } else if (elemName == "uses-permission") {
                            if (attrName == "name") {
                                std::string perm = get_str(pool, rawVal);
                                if (!perm.empty()) permissions.push_back(perm);
                            }
                        }
                    }
                }
                pos += chunkSize;
            }
            if (!packageName.empty()) info.metadata["package"] = packageName;
            if (versionCode != 0) info.metadata["version_code"] = std::to_string(versionCode);
            if (!versionName.empty()) info.metadata["version_name"] = versionName;
            if (minSdk != 0) info.metadata["min_sdk"] = std::to_string(minSdk);
            if (!permissions.empty()) {
                std::string p;
                for (size_t i = 0; i < permissions.size(); ++i) {
                    if (i) p += ", ";
                    p += permissions[i];
                }
                info.metadata["permissions"] = p;
            }
            break;
        }
    }

    if (info.metadata.count("package")) {
        info.warnings.clear();
    } else {
        info.warnings.push_back("apk metadata extraction is minimal");
    }
    return info;
}

} // namespace lingua::apk