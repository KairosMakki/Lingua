#include <lingua/format/msi/msi_parser.hpp>
#include <lingua/core/error.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace lingua::msi {

namespace {
const uint32_t FREESECT = 0xFFFFFFFF;
const uint32_t ENDOFCHAIN = 0xFFFFFFFE;
const uint32_t FATSECT = 0xFFFFFFFD;
const uint32_t DIFSECT = 0xFFFFFFFC;

uint32_t rd32(const uint8_t* p){ return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24); }
uint16_t rd16(const uint8_t* p){ return p[0] | (p[1]<<8); }
uint64_t rd64(const uint8_t* p){ uint64_t v=0; for(int i=7;i>=0;--i) v=(v<<8)|p[i]; return v; }

struct CFB {
    int sectorShift = 9;
    int miniSectorShift = 6;
    uint32_t miniCutoff = 4096;
    uint32_t firstDirSector = 0;
    uint32_t firstMiniFatSector = 0;
    uint32_t miniFatSectors = 0;
    uint32_t firstFatSector = 0; // not used
    std::vector<uint32_t> fat;
    std::vector<uint32_t> miniFat;
    std::vector<uint8_t> miniStream;
    const std::vector<uint8_t>* data = nullptr;
    size_t sectorSize() const { return 1u << sectorShift; }
    size_t miniSectorSize() const { return 1u << miniSectorShift; }

    bool load(const std::vector<uint8_t>& d) {
        data = &d;
        if (d.size() < 512) return false;
        if (d[0]!=0xD0||d[1]!=0xCF||d[2]!=0x11||d[3]!=0xE0||d[4]!=0xA1||d[5]!=0xB1||d[6]!=0x1A||d[7]!=0xE1) return false;
        sectorShift = rd16(d.data()+30);
        miniSectorShift = rd16(d.data()+32);
        miniCutoff = rd32(d.data()+56);
        uint32_t fatSectors = rd32(d.data()+44);
        firstDirSector = rd32(d.data()+48);
        firstMiniFatSector = rd32(d.data()+60);
        miniFatSectors = rd32(d.data()+64);
        // DIFAT
        std::vector<uint32_t> difat;
        for (int i=0;i<109;++i) difat.push_back(rd32(d.data()+76+i*4));
        // first DIFAT sector at offset 0x44
        uint32_t nextDifat = rd32(d.data()+0x44);
        uint32_t difatSectors = rd32(d.data()+0x48);
        // traverse DIFAT sectors
        uint32_t cur = nextDifat;
        for (uint32_t s=0; s<difatSectors && cur != ENDOFCHAIN; ++s) {
            size_t off = (cur+1)*sectorSize();
            if (off + sectorSize() > d.size()) break;
            for (size_t i=0; i<sectorSize()/4 - 1; ++i) difat.push_back(rd32(d.data()+off+i*4));
            cur = rd32(d.data()+off+sectorSize()-4);
        }
        // Build FAT
        size_t totalSectors = d.size() / sectorSize() - 1; // approximate
        (void)totalSectors;
        fat.assign(d.size()/sectorSize() + 10, FREESECT);
        for (uint32_t ds : difat) {
            if (ds == FREESECT || ds == ENDOFCHAIN) continue;
            size_t off = (ds+1)*sectorSize();
            if (off + sectorSize() > d.size()) continue;
            for (size_t i=0; i<sectorSize()/4; ++i) {
                uint32_t val = rd32(d.data()+off+i*4);
                size_t idx = ds* (sectorSize()/4) + i;
                if (idx < fat.size()) fat[idx] = val;
            }
        }
        // mini FAT
        if (firstMiniFatSector != ENDOFCHAIN) {
            // traverse mini FAT chain
            std::vector<uint32_t> chain;
            uint32_t c = firstMiniFatSector;
            while (c != ENDOFCHAIN && c < fat.size()) { chain.push_back(c); c = fat[c]; if (chain.size()>100000) break; }
            for (uint32_t sec : chain) {
                size_t off = (sec+1)*sectorSize();
                if (off + sectorSize() > d.size()) break;
                for (size_t i=0; i<sectorSize()/4; ++i) miniFat.push_back(rd32(d.data()+off+i*4));
            }
        }
        // mini stream container: root entry first sector chain (entry index 0)
        // We'll read later when needed; store root first sector? directory entry 0
        return true;
    }

    std::vector<uint8_t> readChain(uint32_t firstSector, bool mini) {
        std::vector<uint8_t> out;
        if (firstSector == ENDOFCHAIN) return out;
        if (mini) {
            // read from mini stream
            uint32_t c = firstSector;
            size_t ms = miniSectorSize();
            while (c != ENDOFCHAIN && c < miniFat.size()) {
                size_t off = c * ms; // mini stream is contiguous in miniStream
                if (off + ms > miniStream.size()) break;
                out.insert(out.end(), miniStream.begin()+off, miniStream.begin()+off+ms);
                c = miniFat[c];
                if (out.size() > 100*1024*1024) break;
            }
        } else {
            uint32_t c = firstSector;
            size_t ss = sectorSize();
            while (c != ENDOFCHAIN && c < fat.size()) {
                size_t off = (c+1)*ss;
                if (off + ss > data->size()) break;
                out.insert(out.end(), data->begin()+off, data->begin()+off+ss);
                c = fat[c];
                if (out.size() > 100*1024*1024) break;
            }
        }
        return out;
    }
};

struct DirEntry {
    std::string name;
    uint8_t type = 0;
    uint32_t firstSector = 0;
    uint64_t size = 0;
    uint32_t child = 0;
    uint32_t left = 0;
    uint32_t right = 0;
};

std::vector<DirEntry> readDirectory(CFB& cfb) {
    std::vector<DirEntry> entries;
    // read directory chain
    std::vector<uint8_t> dirData = cfb.readChain(cfb.firstDirSector, false);
    // parse entries (128 bytes each)
    for (size_t off = 0; off + 128 <= dirData.size(); off += 128) {
        const uint8_t* e = dirData.data() + off;
        // name: 64 bytes UTF-16LE, length in bytes at offset 64 (name length)
        uint16_t nameLen = rd16(e+64); // in characters? actually length in characters including null
        std::string name;
        for (int i=0; i+1 < nameLen && i*2+1 < 64; ++i) {
            uint16_t ch = rd16(e + i*2);
            if (ch == 0) break;
            if (ch < 0x80) name.push_back((char)ch);
            else name.push_back('?');
        }
        DirEntry de;
        de.name = name;
        de.type = e[66];
        de.left = rd32(e+68);
        de.right = rd32(e+72);
        de.child = rd32(e+76);
        de.firstSector = rd32(e+116);
        de.size = rd64(e+120);
        entries.push_back(de);
    }
    // read mini stream from root entry (index 0)
    if (!entries.empty() && entries[0].type == 5) {
        // root entry's stream contains mini stream
        auto miniData = cfb.readChain(entries[0].firstSector, false);
        cfb.miniStream = miniData;
    }
    return entries;
}
}

bool MsiParser::detect(std::span<const std::uint8_t> data) const {
    if (data.size() < 8) return false;
    return data[0]==0xD0 && data[1]==0xCF && data[2]==0x11 && data[3]==0xE0 && data[4]==0xA1 && data[5]==0xB1 && data[6]==0x1A && data[7]==0xE1;
}

Result<FileInfo> MsiParser::parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const {
    FileInfo info; info.path=path; info.format=Format::Msi; info.file_size=data.size();
    std::vector<uint8_t> buf(data.begin(), data.end());
    CFB cfb;
    if (!cfb.load(buf)) return Error{ErrorCode::MalformedHeader, "bad CFB"};
    auto entries = readDirectory(cfb);
    // collect streams
    for (auto& de : entries) {
        if (de.type != 2) continue; // stream
        if (de.name.empty()) continue;
        Section sec; sec.name = de.name; sec.size = de.size; info.sections.push_back(sec);
        // read stream data
        bool mini = de.size < cfb.miniCutoff;
        auto streamData = cfb.readChain(de.firstSector, mini);
        // Property table? Try to parse if name == "Property"
        if (de.name == "Property") {
            // MSI Property table: rows of (Property, Value) strings. The stream is a table.
            // We'll scan for UTF-16LE strings and pair them? Instead, parse table header.
            // MSI table format: first 4 bytes? Let's just extract printable ASCII runs.
            std::string text;
            for (size_t i=0; i+1 < streamData.size(); i+=2) {
                uint16_t ch = streamData[i] | (streamData[i+1]<<8);
                if (ch == 0) { text.push_back('\n'); }
                else if (ch < 0x80 && ch >= 0x20) text.push_back((char)ch);
            }
            info.metadata["property_table_raw"] = text.substr(0, 2000);
        }
    }
    info.metadata["stream_count"] = std::to_string(info.sections.size());
    info.warnings.push_back("MSI database table parsing is partial");
    return info;
}

} // namespace lingua::msi