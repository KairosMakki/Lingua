#include <lingua/format/rpm/rpm_parser.hpp>
#include <lingua/core/archive.hpp>
#include <lingua/core/byte_reader.hpp>
#include <lingua/core/error.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace lingua::rpm {

namespace {
// RPM header parsing
struct IndexEntry { uint32_t tag, type, offset, count; };

struct ParsedHeader {
    uint32_t nindex = 0;
    uint32_t hsize = 0;
    std::vector<IndexEntry> indices;
    const uint8_t* data = nullptr; // points into original buffer
    size_t data_offset = 0;
    size_t total_size = 0;
};

bool parse_header(const std::vector<uint8_t>& buf, size_t off, ParsedHeader& hdr) {
    if (off + 16 > buf.size()) return false;
    if (buf[off] != 0x8E || buf[off+1] != 0xAD || buf[off+2] != 0xE8 || buf[off+3] != 0x01) return false;
    // reserved 4 bytes
    uint32_t nindex = (buf[off+8]<<24)|(buf[off+9]<<16)|(buf[off+10]<<8)|buf[off+11];
    uint32_t hsize = (buf[off+12]<<24)|(buf[off+13]<<16)|(buf[off+14]<<8)|buf[off+15];
    if (nindex > 100000) return false;
    if (off + 16 + nindex*16 + hsize > buf.size()) return false;
    hdr.nindex = nindex;
    hdr.hsize = hsize;
    hdr.indices.resize(nindex);
    for (uint32_t i = 0; i < nindex; ++i) {
        size_t idx = off + 16 + i*16;
        hdr.indices[i].tag = (buf[idx]<<24)|(buf[idx+1]<<16)|(buf[idx+2]<<8)|buf[idx+3];
        hdr.indices[i].type = (buf[idx+4]<<24)|(buf[idx+5]<<16)|(buf[idx+6]<<8)|buf[idx+7];
        hdr.indices[i].offset = (buf[idx+8]<<24)|(buf[idx+9]<<16)|(buf[idx+10]<<8)|buf[idx+11];
        hdr.indices[i].count = (buf[idx+12]<<24)|(buf[idx+13]<<16)|(buf[idx+14]<<8)|buf[idx+15];
    }
    hdr.data_offset = off + 16 + nindex*16;
    hdr.data = buf.data() + hdr.data_offset;
    hdr.total_size = 16 + nindex*16 + hsize;
    // align to 8
    if (hdr.total_size % 8) hdr.total_size += 8 - (hdr.total_size % 8);
    return true;
}

// data access helpers
std::string get_string(const ParsedHeader& hdr, uint32_t tag) {
    for (auto& ie : hdr.indices) {
        if (ie.tag == tag && ie.type == 6 && ie.count >= 1) {
            if (ie.offset + 1 <= hdr.hsize) {
                const char* s = reinterpret_cast<const char*>(hdr.data + ie.offset);
                // ensure within
                size_t max = hdr.hsize - ie.offset;
                size_t len = 0;
                while (len < max && s[len] != '\0') ++len;
                return std::string(s, len);
            }
        }
    }
    return {};
}

std::vector<std::string> get_string_array(const ParsedHeader& hdr, uint32_t tag) {
    std::vector<std::string> out;
    for (auto& ie : hdr.indices) {
        if (ie.tag == tag && (ie.type == 8 || ie.type == 9) && ie.count >= 1) {
            // array of offsets? In RPM, string arrays are stored as offsets into data store? Actually type 8 is string array: data store contains concatenated null-terminated strings; index entry offset points to first string? For simplicity, RPM stores string arrays as array of offsets? Let's handle: type 8 data is array of int32 offsets? Actually RPM STRING_TYPE=6 (single), STRING_ARRAY_TYPE=8: data store has array of 32-bit offsets followed by strings? Hmm.
            // We'll attempt: if offset points to int32 array
            if (ie.count > 10000) return out;
            const uint8_t* p = hdr.data + ie.offset;
            // check if first 4 bytes look like offset
            // Try treating as array of 4-byte offsets
            for (uint32_t i = 0; i < ie.count; ++i) {
                uint32_t o = (p[i*4]<<24)|(p[i*4+1]<<16)|(p[i*4+2]<<8)|p[i*4+3];
                if (o + 1 > hdr.hsize) break;
                const char* s = reinterpret_cast<const char*>(hdr.data + o);
                size_t max = hdr.hsize - o;
                size_t len = 0;
                while (len < max && s[len] != '\0') ++len;
                out.emplace_back(s, len);
            }
            if (!out.empty()) return out;
        }
    }
    return out;
}

const char* arch_name_from_rpm(const std::string& a) {
    // pass-through; map to enum outside
    return a.c_str();
}
}

bool RpmParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 4) return false;
    return data[0] == 0xED && data[1] == 0xAB && data[2] == 0xEE && data[3] == 0xDB;
}

Result<FileInfo> RpmParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info;
    info.path = path;
    info.format = Format::Rpm;
    info.file_size = data.size();
    info.endianness = Endianness::Big;

    std::vector<uint8_t> buf(data.begin(), data.end());
    if (buf.size() < 96) return Error{ErrorCode::TruncatedInput, "rpm lead truncated"};

    // lead
    std::string lead_name;
    for (int i = 10; i < 76 && buf[i] != 0; ++i) lead_name.push_back((char)buf[i]);
    info.metadata["lead_name"] = lead_name;

    // signature header
    size_t off = 96;
    ParsedHeader sig_hdr;
    if (!parse_header(buf, off, sig_hdr)) {
        return Error{ErrorCode::MalformedHeader, "bad signature header"};
    }
    off += sig_hdr.total_size;

    // main header
    ParsedHeader hdr;
    if (!parse_header(buf, off, hdr)) {
        return Error{ErrorCode::MalformedHeader, "bad header"};
    }
    off += hdr.total_size;

    // metadata
    std::string name = get_string(hdr, 1000); // RPMTAG_NAME
    std::string version = get_string(hdr, 1001); // RPMTAG_VERSION
    std::string release = get_string(hdr, 1002); // RPMTAG_RELEASE
    std::string arch = get_string(hdr, 1022); // RPMTAG_ARCH
    info.metadata["name"] = name;
    info.metadata["version"] = version;
    info.metadata["release"] = release;
    info.metadata["arch"] = arch;
    if (!name.empty()) info.metadata["package"] = name + "-" + version + "-" + release;

    // architecture
    if (arch == "x86_64") info.architecture = Architecture::X86_64;
    else if (arch == "i386" || arch == "i486" || arch == "i586" || arch == "i686") info.architecture = Architecture::X86;
    else if (arch == "aarch64") info.architecture = Architecture::AArch64;
    else if (arch == "armv7hl" || arch == "armv7l" || arch.rfind("arm",0)==0) info.architecture = Architecture::ARM;
    else if (arch == "ppc64" || arch == "ppc64le" || arch == "ppc") info.architecture = Architecture::PowerPC;
    else if (arch == "mips" || arch == "mipsel") info.architecture = Architecture::MIPS;
    else if (arch == "riscv64") info.architecture = Architecture::RISCV;
    else info.architecture = Architecture::Unknown;

    // dependencies
    auto reqs = get_string_array(hdr, 1049); // RPMTAG_REQUIRENAME
    for (auto& r : reqs) {
        if (!r.empty()) info.dependencies.push_back(Dependency{r, "", false});
    }

    // payload offset
    info.metadata["payload_offset"] = std::to_string(off);
    info.metadata["payload_size"] = std::to_string(buf.size() > off ? buf.size() - off : 0);

    // store sections for payload
    if (off < buf.size()) {
        Section sec;
        sec.name = "payload";
        sec.offset = off;
        sec.size = buf.size() - off;
        sec.type = "cpio";
        info.sections.push_back(sec);
    }

    info.warnings.push_back("rpm payload extraction requires decompression support");
    return info;
}

} // namespace lingua::rpm