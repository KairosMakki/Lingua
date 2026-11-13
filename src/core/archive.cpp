#include <lingua/core/archive.hpp>
#include <lingua/core/error.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#if defined(LINGUA_HAVE_ZLIB)
#include <zlib.h>
#endif
#if defined(LINGUA_HAVE_ZSTD)
#include <zstd.h>
#endif

namespace lingua::archive {

// ---------- ar ----------
std::vector<ArMember> parse_ar(const std::vector<std::uint8_t>& data) {
    std::vector<ArMember> members;
    if (data.size() < 8) return members;
    if (std::memcmp(data.data(), "!<arch>\n", 8) != 0) return members;
    size_t pos = 8;
    std::string ext_names;
    while (pos + 60 <= data.size()) {
        char name[17] = {0}; std::memcpy(name, data.data() + pos, 16);
        char sizebuf[11] = {0}; std::memcpy(sizebuf, data.data() + pos + 48, 10);
        size_t sz = 0;
        try { sz = std::stoull(sizebuf); } catch (...) { break; }
        std::string member_name = name;
        while (!member_name.empty() && member_name.back() == ' ') member_name.pop_back();
        size_t data_start = pos + 60;
        if (data_start + sz > data.size()) break;
        if (member_name == "//") {
            ext_names.assign(reinterpret_cast<const char*>(data.data() + data_start), sz);
        } else if (member_name != "/") {
            std::string actual = member_name;
            if (!member_name.empty() && member_name[0] == '/') {
                size_t off = 0;
                try { off = std::stoull(member_name.substr(1)); } catch (...) { off = 0; }
                if (off < ext_names.size()) {
                    size_t end = ext_names.find('/', off);
                    if (end == std::string::npos) end = ext_names.size();
                    actual = ext_names.substr(off, end - off);
                }
            }
            if (!actual.empty() && actual.back() == '/') actual.pop_back();
            ArMember m;
            m.name = actual;
            m.data.assign(data.data() + data_start, data.data() + data_start + sz);
            members.push_back(m);
        }
        pos = data_start + sz;
        if (sz % 2 == 1) ++pos;
    }
    return members;
}

// ---------- tar ----------
std::vector<TarEntry> parse_tar(const std::vector<std::uint8_t>& data) {
    std::vector<TarEntry> entries;
    size_t pos = 0;
    while (pos + 512 <= data.size()) {
        bool allzero = true;
        for (int i = 0; i < 512; ++i) if (data[pos + i] != 0) { allzero = false; break; }
        if (allzero) { pos += 512; continue; }
        const uint8_t* h = data.data() + pos;
        char name[101] = {0}; std::memcpy(name, h, 100);
        char sizebuf[13] = {0}; std::memcpy(sizebuf, h + 124, 12);
        size_t sz = 0;
        for (int i = 0; i < 12; ++i) {
            if (sizebuf[i] == ' ' || sizebuf[i] == '\0') break;
            if (sizebuf[i] < '0' || sizebuf[i] > '7') { sz = 0; break; }
            sz = sz * 8 + (sizebuf[i] - '0');
        }
        char typeflag = (char)h[156];
        std::string full_name = name;
        char prefix[156] = {0}; std::memcpy(prefix, h + 345, 155);
        if (prefix[0]) full_name = std::string(prefix) + "/" + full_name;
        if (full_name.rfind("./", 0) == 0) full_name = full_name.substr(2);
        size_t data_start = pos + 512;
        if (data_start + sz > data.size()) break;
        if (typeflag == '0' || typeflag == '\0' || typeflag == '7') {
            TarEntry e; e.name = full_name;
            e.data.assign(data.data() + data_start, data.data() + data_start + sz);
            e.is_dir = false; entries.push_back(e);
        } else if (typeflag == '5') {
            TarEntry e; e.name = full_name; e.is_dir = true; entries.push_back(e);
        }
        size_t advance = 512 + ((sz + 511) / 512) * 512;
        pos += advance;
        if (pos == 0) break;
    }
    return entries;
}

// ---------- decompress ----------
std::vector<std::uint8_t> decompress(const std::vector<std::uint8_t>& in) {
    if (in.size() >= 2 && in[0] == 0x1F && in[1] == 0x8B) {
#if defined(LINGUA_HAVE_ZLIB)
        z_stream strm{};
        strm.next_in = const_cast<Bytef*>(in.data());
        strm.avail_in = in.size();
        if (inflateInit2(&strm, 16 + 15) != Z_OK) return {};
        std::vector<std::uint8_t> out;
        uint8_t buf[65536];
        int ret;
        do {
            strm.next_out = buf;
            strm.avail_out = sizeof(buf);
            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) { inflateEnd(&strm); return {}; }
            size_t have = sizeof(buf) - strm.avail_out;
            out.insert(out.end(), buf, buf + have);
            if (out.size() > 100 * 1024 * 1024) { inflateEnd(&strm); return {}; }
        } while (ret != Z_STREAM_END);
        inflateEnd(&strm);
        return out;
#else
        return {};
#endif
    }
    if (in.size() >= 4 && in[0] == 0x28 && in[1] == 0xB5 && in[2] == 0x2F && in[3] == 0xFD) {
#if defined(LINGUA_HAVE_ZSTD)
        unsigned long long sz = ZSTD_getFrameContentSize(in.data(), in.size());
        if (sz == ZSTD_CONTENTSIZE_ERROR || sz == ZSTD_CONTENTSIZE_UNKNOWN) return {};
        if (sz > 100 * 1024 * 1024) return {};
        std::vector<std::uint8_t> out(sz);
        size_t ret = ZSTD_decompress(out.data(), out.size(), in.data(), in.size());
        if (ZSTD_isError(ret)) return {};
        out.resize(ret);
        return out;
#else
        return {};
#endif
    }
    return in;
}

// ---------- zip ----------
static uint32_t rd32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

std::vector<ZipEntry> parse_zip(const std::vector<std::uint8_t>& data) {
    std::vector<ZipEntry> entries;
    // find EOCD
    size_t eocd = std::string::npos;
    for (size_t i = data.size(); i >= 22 && i-- > 0; ) {
        if (i + 4 <= data.size() && rd32(data.data() + i) == 0x06054b50) { eocd = i; break; }
        if (i == 0) break;
    }
    if (eocd == std::string::npos) return entries;
    uint32_t cd_offset = rd32(data.data() + eocd + 16);
    uint16_t num_entries = data[eocd + 10] | (data[eocd + 11] << 8);
    size_t pos = cd_offset;
    for (int n = 0; n < num_entries; ++n) {
        if (pos + 46 > data.size()) break;
        if (rd32(data.data() + pos) != 0x02014b50) break;
        uint16_t method = data[pos + 10] | (data[pos + 11] << 8);
        uint32_t csize = rd32(data.data() + pos + 20);
        uint32_t usize = rd32(data.data() + pos + 24);
        uint16_t fnlen = data[pos + 28] | (data[pos + 29] << 8);
        uint16_t eflen = data[pos + 30] | (data[pos + 31] << 8);
        uint16_t cmtlen = data[pos + 32] | (data[pos + 33] << 8);
        uint32_t lho = rd32(data.data() + pos + 42);
        std::string name(reinterpret_cast<const char*>(data.data() + pos + 46), fnlen);
        pos += 46 + fnlen + eflen + cmtlen;

        // read local header
        if (lho + 30 > data.size()) continue;
        if (rd32(data.data() + lho) != 0x04034b50) continue;
        uint16_t lfnlen = data[lho + 26] | (data[lho + 27] << 8);
        uint16_t leflen = data[lho + 28] | (data[lho + 29] << 8);
        size_t data_start = lho + 30 + lfnlen + leflen;
        if (data_start + csize > data.size()) continue;

        ZipEntry e;
        e.name = name;
        e.is_dir = (!name.empty() && name.back() == '/');
        if (e.is_dir) { entries.push_back(e); continue; }
        std::vector<uint8_t> raw(data.data() + data_start, data.data() + data_start + csize);
        if (method == 0) {
            e.data = raw;
        } else if (method == 8) {
#if defined(LINGUA_HAVE_ZLIB)
            z_stream strm{};
            strm.next_in = const_cast<Bytef*>(raw.data());
            strm.avail_in = raw.size();
            inflateInit2(&strm, -15);
            std::vector<uint8_t> out;
            uint8_t buf[65536];
            int ret;
            do {
                strm.next_out = buf; strm.avail_out = sizeof(buf);
                ret = inflate(&strm, Z_NO_FLUSH);
                if (ret != Z_OK && ret != Z_STREAM_END) { inflateEnd(&strm); out.clear(); break; }
                size_t have = sizeof(buf) - strm.avail_out;
                out.insert(out.end(), buf, buf + have);
                if (out.size() > 100 * 1024 * 1024) { inflateEnd(&strm); out.clear(); break; }
            } while (ret != Z_STREAM_END);
            inflateEnd(&strm);
            if (out.size() == usize) e.data = out;
            else e.data = raw;
#else
            e.data = raw;
#endif
        } else {
            e.data = raw;
        }
        entries.push_back(e);
    }
    return entries;
}

// ---------- cpio (newc) ----------
static uint32_t parse_hex(const char* p, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; ++i) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
    }
    return v;
}

std::vector<TarEntry> parse_cpio(const std::vector<std::uint8_t>& data) {
    std::vector<TarEntry> entries;
    size_t pos = 0;
    while (pos + 110 <= data.size()) {
        if (std::memcmp(data.data() + pos, "070701", 6) != 0 && std::memcmp(data.data() + pos, "070702", 6) != 0) break;
        // fields start at pos+6, 13*8
        const char* f = reinterpret_cast<const char*>(data.data() + pos + 6);
        uint32_t mode = parse_hex(f + 3*8, 8);
        uint32_t filesize = parse_hex(f + 6*8, 8);
        uint32_t namesize = parse_hex(f + 11*8, 8);
        // name
        size_t name_off = pos + 6 + 13*8;
        if (name_off + namesize > data.size()) break;
        std::string name(reinterpret_cast<const char*>(data.data() + name_off), namesize);
        // strip trailing null
        while (!name.empty() && name.back() == '\0') name.pop_back();
        if (name == "TRAILER!!!") break;
        size_t data_off = name_off + namesize;
        // pad to 4
        while (data_off % 4) ++data_off;
        if (data_off + filesize > data.size()) break;
        TarEntry e;
        e.name = name;
        e.is_dir = ((mode & 0xF000) == 0x4000);
        if (!e.is_dir) e.data.assign(data.data() + data_off, data.data() + data_off + filesize);
        entries.push_back(e);
        size_t next = data_off + filesize;
        while (next % 4) ++next;
        pos = next;
    }
    return entries;
}

} // namespace lingua::archive