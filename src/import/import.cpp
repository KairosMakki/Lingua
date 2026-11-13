#include <lingua/import/import.hpp>
#include <lingua/adapters/adapter.hpp>
#include <lingua/core/error.hpp>
#include <lingua/core/logger.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace lingua::importing {

namespace fs = std::filesystem;

namespace {
// Minimal SHA-256
static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
uint32_t rotr(uint32_t x, uint32_t n){ return (x>>n)|(x<<(32-n)); }
std::string sha256_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    in.seekg(0, std::ios::end);
    size_t sz = (size_t)in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(sz);
    in.read(reinterpret_cast<char*>(data.data()), sz);
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint64_t bitlen = data.size() * 8;
    data.push_back(0x80);
    while ((data.size() % 64) != 56) data.push_back(0);
    for (int i = 7; i >= 0; --i) data.push_back((bitlen >> (i*8)) & 0xFF);
    for (size_t off = 0; off < data.size(); off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) w[i] = (data[off+i*4]<<24)|(data[off+i*4+1]<<16)|(data[off+i*4+2]<<8)|data[off+i*4+3];
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);
            uint32_t s1 = rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e,6)^rotr(e,11)^rotr(e,25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t temp1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a,2)^rotr(a,13)^rotr(a,22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;
            hh=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) oss << std::setw(8) << h[i];
    return oss.str();
}

std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
} // anon

bool is_safe_member_path(const std::string& member) {
    if (member.empty()) return false;
    if (member[0] == '/') return false;
    if (member.find('\\') != std::string::npos) return false;
    if (member.size() >= 2 && member[1] == ':') return false;
    size_t start = 0;
    while (true) {
        size_t slash = member.find('/', start);
        std::string comp = (slash == std::string::npos) ? member.substr(start) : member.substr(start, slash - start);
        if (comp == "..") return false;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

fs::path safe_join(const fs::path& base, const std::string& member) {
    if (!is_safe_member_path(member)) return {};
    fs::path resolved = base / member;
    std::error_code ec;
    auto canon_base = fs::weakly_canonical(base, ec);
    if (ec) canon_base = base;
    auto canon_resolved = fs::weakly_canonical(resolved, ec);
    if (ec) canon_resolved = resolved;
    auto it_base = canon_base.begin();
    auto it_res = canon_resolved.begin();
    for (; it_base != canon_base.end(); ++it_base, ++it_res) {
        if (it_res == canon_resolved.end() || *it_res != *it_base) return {};
    }
    return resolved;
}

Result<ImportResult> import_app(const FileInfo& info, const fs::path& source, const ImportOptions& opts) {
    ImportResult result;
    if (source.empty()) return Error{ErrorCode::InvalidArgument, "empty source path"};

    std::error_code ec;
    if (!fs::exists(source, ec) || !fs::is_regular_file(source, ec)) {
        return Error{ErrorCode::NotFound, "source file not found"};
    }
    auto fsize = fs::file_size(source, ec);
    if (ec) return Error{ErrorCode::IoError, "cannot stat source"};
    if (fsize > opts.max_file_size) {
        return Error{ErrorCode::SizeLimitExceeded, "file exceeds size limit"};
    }

    fs::path staging = opts.staging_dir;
    if (staging.empty()) {
        staging = fs::temp_directory_path() / "lingua-staging";
    }
    fs::create_directories(staging, ec);
    if (ec) return Error{ErrorCode::IoError, "cannot create staging dir"};

#if defined(__unix__) || defined(__APPLE__)
    fs::permissions(staging, fs::perms::owner_all, fs::perm_options::replace, ec);
#endif

    std::string safe_name = source.filename().string();
    for (char& c : safe_name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
    }
    fs::path staged = staging / safe_name;

    if (!opts.dry_run) {
        fs::copy_file(source, staged, fs::copy_options::overwrite_existing, ec);
        if (ec) return Error{ErrorCode::IoError, "cannot stage file: " + ec.message()};
#if defined(__unix__) || defined(__APPLE__)
        fs::permissions(staged, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ec);
#endif
    }
    result.staged_path = staged;

    HostOS host = adapters::detect_host_os();
    auto adapter = adapters::find_adapter(info.format, host);
    if (adapter) {
        auto ar = adapter->prepare(staged, host);
        if (ar) {
            result.adapter_command = ar->command;
            if (opts.sandbox && host == HostOS::Linux) {
                // wrap with bubblewrap/firejail if available
                if (std::system("command -v bwrap >/dev/null 2>&1") == 0) {
                    result.adapter_command = "bwrap --ro-bind / / " + result.adapter_command;
                } else if (std::system("command -v firejail >/dev/null 2>&1") == 0) {
                    result.adapter_command = "firejail " + result.adapter_command;
                }
            }
            if (!ar->supported) result.warnings.push_back(ar->message);
        } else {
            result.warnings.push_back(ar.error().message);
        }
    } else {
        result.warnings.push_back("no adapter available for format/host");
    }

    std::string hash = sha256_file(source);

    std::ostringstream audit;
    audit << timestamp() << " format=" << to_string(info.format)
          << " arch=" << to_string(info.architecture)
          << " sha256=" << hash
          << " source=" << source.string()
          << " staged=" << staged.string()
          << " dry_run=" << (opts.dry_run ? "true" : "false")
          << " sandbox=" << (opts.sandbox ? "true" : "false")
          << " host=" << static_cast<int>(host);
    result.audit_entry = audit.str();

    if (!opts.audit_log_path.empty()) {
        std::ofstream log(opts.audit_log_path, std::ios::app);
        if (log) log << result.audit_entry << "\n";
    }

    result.success = true;
    for (auto& w : info.warnings) result.warnings.push_back(w);
    return result;
}

} // namespace lingua::importing