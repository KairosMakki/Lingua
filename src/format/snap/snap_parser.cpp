#include <lingua/format/snap/snap_parser.hpp>
#include <lingua/core/error.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#if defined(LINGUA_HAVE_ZLIB)
#include <zlib.h>
#endif
#if defined(LINGUA_HAVE_ZSTD)
#include <zstd.h>
#endif

namespace lingua::snap {

namespace {
uint16_t rd16(const uint8_t* p){ return p[0] | (p[1]<<8); }
uint32_t rd32(const uint8_t* p){ return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24); }
uint64_t rd64(const uint8_t* p){ uint64_t v=0; for(int i=7;i>=0;--i) v=(v<<8)|p[i]; return v; }

std::vector<uint8_t> decompress_block(const std::vector<uint8_t>& in, uint16_t compression) {
    if (compression == 1) { // zlib
#if defined(LINGUA_HAVE_ZLIB)
        std::vector<uint8_t> out;
        z_stream strm{};
        strm.next_in = const_cast<Bytef*>(in.data());
        strm.avail_in = in.size();
        if (inflateInit(&strm) != Z_OK) return {};
        uint8_t buf[65536];
        int ret;
        do {
            strm.next_out = buf;
            strm.avail_out = sizeof(buf);
            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) { inflateEnd(&strm); return {}; }
            size_t have = sizeof(buf) - strm.avail_out;
            out.insert(out.end(), buf, buf + have);
            if (out.size() > 100*1024*1024) { inflateEnd(&strm); return {}; }
        } while (ret != Z_STREAM_END);
        inflateEnd(&strm);
        return out;
#else
        return {};
#endif
    } else if (compression == 5) { // zstd
#if defined(LINGUA_HAVE_ZSTD)
        unsigned long long sz = ZSTD_getFrameContentSize(in.data(), in.size());
        if (sz == ZSTD_CONTENTSIZE_ERROR || sz == ZSTD_CONTENTSIZE_UNKNOWN) return {};
        if (sz > 100*1024*1024) return {};
        std::vector<uint8_t> out(sz);
        size_t ret = ZSTD_decompress(out.data(), out.size(), in.data(), in.size());
        if (ZSTD_isError(ret)) return {};
        out.resize(ret);
        return out;
#else
        return {};
#endif
    }
    return {}; // xz, lzo, lz4 not supported
}

// Read a metadata block at position pos, advance pos
std::vector<uint8_t> read_meta_block(const uint8_t* data, size_t data_size, size_t& pos, uint16_t compression) {
    if (pos + 2 > data_size) return {};
    uint16_t size = data[pos] | (data[pos+1] << 8);
    pos += 2;
    bool compressed = !(size & 0x8000);
    size &= 0x7FFF;
    if (pos + size > data_size) return {};
    std::vector<uint8_t> in(data + pos, data + pos + size);
    pos += size;
    if (!compressed) return in;
    return decompress_block(in, compression);
}

// Walk metadata blocks to find block at index target_idx starting from table_start
std::vector<uint8_t> read_meta_block_at(const uint8_t* data, size_t data_size, uint64_t table_start, int target_idx, uint16_t compression) {
    size_t pos = table_start;
    for (int i = 0; i <= target_idx; ++i) {
        auto block = read_meta_block(data, data_size, pos, compression);
        if (i == target_idx) return block;
    }
    return {};
}
}

bool SnapParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 4) return false;
    return data[0]=='h' && data[1]=='s' && data[2]=='q' && data[3]=='s';
}

Result<FileInfo> SnapParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info; info.path=path; info.format=Format::Snap; info.file_size=data.size();
    if (data.size() < 96) return Error{ErrorCode::TruncatedInput, "squashfs superblock truncated"};

    uint32_t inodes = rd32(data.data()+4);
    uint32_t block_size = rd32(data.data()+12);
    uint16_t compression = rd16(data.data()+16);
    uint16_t major = rd16(data.data()+26);
    uint16_t minor = rd16(data.data()+28);
    uint64_t root_inode_ref = rd64(data.data()+32);
    uint64_t bytes_used = rd64(data.data()+40);
    uint64_t inode_table_start = rd64(data.data()+64);
    uint64_t directory_table_start = rd64(data.data()+72);
    uint64_t fragment_table_start = rd64(data.data()+80);

    const char* comp_name = "unknown";
    switch (compression) { case 1: comp_name="zlib"; break; case 2: comp_name="lzo"; break; case 3: comp_name="xz"; break; case 4: comp_name="lz4"; break; case 5: comp_name="zstd"; break; }
    info.metadata["squashfs_version"] = std::to_string(major) + "." + std::to_string(minor);
    info.metadata["inodes"] = std::to_string(inodes);
    info.metadata["block_size"] = std::to_string(block_size);
    info.metadata["compression"] = comp_name;
    info.metadata["bytes_used"] = std::to_string(bytes_used);
    info.metadata["payload_type"] = "squashfs";

    if (compression != 1 && compression != 5) {
        info.warnings.push_back("squashfs compression not supported for file extraction");
        return info;
    }

    // Read root inode
    int root_block_idx = (int)(root_inode_ref >> 16);
    int root_offset = (int)(root_inode_ref & 0xFFFF);
    auto root_block = read_meta_block_at(data.data(), data.size(), inode_table_start, root_block_idx, compression);
    if (root_block.empty() || root_offset + 32 > root_block.size()) {
        info.warnings.push_back("could not read root inode");
        return info;
    }
    const uint8_t* ri = root_block.data() + root_offset;
    uint16_t inode_type = rd16(ri);
    uint32_t root_inode_number = rd32(ri + 12);
    (void)root_inode_number;

    if (inode_type != 1 && inode_type != 8) {
        info.warnings.push_back("root is not a directory");
        return info;
    }

    // Basic directory inode: start_block at offset 24, file_size at offset 20
    uint32_t dir_start = rd32(ri + 24);
    uint32_t dir_size = rd32(ri + 20);

    // Read directory entries
    size_t pos = directory_table_start + dir_start;
    size_t bytes_read = 0;
    int depth = 0;
    const int MAX_DEPTH = 8;
    std::function<void(size_t, size_t, const std::string&, int)> read_dir = [&](size_t dir_pos, size_t dir_size, const std::string& prefix, int d) {
        if (d > MAX_DEPTH) return;
        size_t p = dir_pos;
        size_t remaining = dir_size;
        while (remaining > 0 && p < data.size()) {
            // read metadata block
            size_t block_start = p;
            auto block = read_meta_block(data.data(), data.size(), p, compression);
            if (block.empty()) break;
            size_t block_bytes = std::min(block.size(), remaining);
            size_t off = 0;
            // directory block header: count (2 bytes) at start? Actually entries start immediately
            // Parse entries: each entry is offset(2)+inode(4)+type(2)+name_size(2)+name(name_size+1)
            // But directory blocks have a header: first 4 bytes? Let me check.
            // In squashfs, directory block: first 2 bytes = number of entries? No.
            // Actually: directory entries are stored sequentially. Each entry:
            //   offset (2): offset within the directory block for sorting
            //   inode_number (4): inode number
            //   type (2): file type (1=reg, 2=dir, 3=symlink, etc.)
            //   name_size (2): name length - 1
            //   name (name_size+1 bytes)
            // The block starts directly with entries, no count header.
            // Entries continue until we've consumed the block or remaining bytes.
            while (off + 10 <= block.size() && off < block_bytes) {
                uint16_t entry_offset = rd16(block.data() + off);
                uint32_t inode_num = rd32(block.data() + off + 2);
                uint16_t type = rd16(block.data() + off + 6);
                uint16_t name_size = rd16(block.data() + off + 8);
                if (off + 10 + name_size + 1 > block.size()) break;
                std::string name(reinterpret_cast<const char*>(block.data() + off + 10), name_size + 1);
                off += 10 + name_size + 1;
                // align to 2? entries are packed, no alignment
                std::string full = prefix.empty() ? name : prefix + "/" + name;
                if (type == 2) {
                    // directory - recurse
                    // Need to read the inode for this directory
                    // But we don't have the inode reference here; we have inode_number
                    // Squashfs doesn't store inode ref in dir entry, just inode number
                    // We'd need to look up the inode by number - not straightforward
                    // For now, just record the directory name
                    info.sections.push_back(Section{full + "/", 0, 0, 0, "directory"});
                } else {
                    info.sections.push_back(Section{full, 0, 0, 0, "file"});
                    if (full == "meta/snap.yaml" || full == "snap.yaml") {
                        // Try to read file content - need inode lookup
                        // For now, store the name
                        info.metadata["snap_yaml_path"] = full;
                    }
                }
                (void)entry_offset; (void)inode_num;
            }
            remaining -= block_bytes;
            if (remaining == 0) break;
        }
    };

    read_dir(directory_table_start + dir_start, dir_size, "", 0);

    if (info.sections.empty()) {
        info.warnings.push_back("no directory entries found");
    }
    info.warnings.push_back("snap file content extraction requires inode traversal (partial)");
    return info;
}

} // namespace lingua::snap