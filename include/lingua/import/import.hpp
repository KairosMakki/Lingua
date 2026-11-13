#pragma once

#include <lingua/core/result.hpp>
#include <lingua/core/types.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace lingua::importing {

struct ImportOptions {
    bool dry_run = false;
    bool sandbox = false;
    std::filesystem::path staging_dir;
    std::uint64_t max_file_size = 2ULL * 1024 * 1024 * 1024;
    bool allow_remote = false;
    std::string audit_log_path;
};

struct ImportResult {
    bool success = false;
    std::filesystem::path staged_path;
    std::string adapter_command;
    std::vector<std::string> warnings;
    std::string audit_entry;
};

Result<ImportResult> import_app(const FileInfo& info, const std::filesystem::path& source, const ImportOptions& opts);

// path safety
bool is_safe_member_path(const std::string& member);
std::filesystem::path safe_join(const std::filesystem::path& base, const std::string& member);

} // namespace lingua::importing