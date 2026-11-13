#include <lingua/import/import.hpp>
#include <lingua/adapters/adapter.hpp>
#include <lingua/core/error.hpp>
#include <lingua/core/hash.hpp>
#include <lingua/core/logger.hpp>
#include <lingua/platform/platform.hpp>

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
std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
} // anon

bool is_safe_member_path(const std::string& member) {
    // delegate to anonymous implementation via redefinition
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
            result.adapter_argv = ar->argv;
            if (opts.sandbox && host == HostOS::Linux) {
                if (platform::tool_exists("bwrap")) {
                    std::vector<std::string> wrapped = {"bwrap", "--ro-bind", "/", "/"};
                    wrapped.insert(wrapped.end(), result.adapter_argv.begin(), result.adapter_argv.end());
                    result.adapter_argv = wrapped;
                } else if (platform::tool_exists("firejail")) {
                    std::vector<std::string> wrapped = {"firejail"};
                    wrapped.insert(wrapped.end(), result.adapter_argv.begin(), result.adapter_argv.end());
                    result.adapter_argv = wrapped;
                }
            }
            if (!ar->supported) result.warnings.push_back(ar->message);
        } else {
            result.warnings.push_back(ar.error().message);
        }
    } else {
        result.warnings.push_back("no adapter available for format/host");
    }

    std::ifstream hin(source, std::ios::binary);
    hin.seekg(0, std::ios::end);
    size_t hsz = (size_t)hin.tellg();
    hin.seekg(0, std::ios::beg);
    std::vector<uint8_t> hbuf(hsz);
    hin.read(reinterpret_cast<char*>(hbuf.data()), hsz);
    std::string hash = sha256(hbuf);

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