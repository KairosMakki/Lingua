#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace lingua {

/// Error codes returned by Lingua operations.
enum class ErrorCode : std::uint32_t {
    Ok = 0,
    IoError,
    NotFound,
    PermissionDenied,
    InvalidArgument,
    OutOfMemory,
    TruncatedInput,
    MalformedHeader,
    UnsupportedFormat,
    UnsupportedArchitecture,
    UnsupportedHost,
    SizeLimitExceeded,
    PathTraversal,
    DecompressionError,
    ChecksumMismatch,
    InternalError,
};

/// A structured error value. No exceptions cross the public API.
struct Error {
    ErrorCode code = ErrorCode::Ok;
    std::string message;
    std::string context;

    [[nodiscard]] bool ok() const noexcept { return code == ErrorCode::Ok; }
    [[nodiscard]] std::string to_string() const;
};

inline bool operator==(const Error& a, const Error& b) noexcept {
    return a.code == b.code && a.message == b.message;
}

} // namespace lingua