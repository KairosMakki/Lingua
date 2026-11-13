#pragma once

#include <lingua/core/result.hpp>
#include <lingua/core/types.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace lingua {

/// Common interface implemented by every format module.
class IFormatParser {
public:
    virtual ~IFormatParser() = default;

    /// Quick magic-number check.
    virtual bool detect(std::span<const std::uint8_t> data) const = 0;

    /// Full parse. Returns partial results plus warnings on partial failure.
    virtual Result<FileInfo> parse(std::span<const std::uint8_t> data,
                                  const std::filesystem::path& path = {}) const = 0;

    /// Human-readable format name.
    virtual const char* name() const noexcept = 0;
};

/// Registry of format parsers. Thread-safe for registration at startup.
class FormatRegistry {
public:
    static FormatRegistry& instance();

    void register_parser(std::shared_ptr<IFormatParser> parser);
    std::vector<std::shared_ptr<IFormatParser>> parsers() const;
    std::shared_ptr<IFormatParser> detect(std::span<const std::uint8_t> data) const;

private:
    FormatRegistry() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lingua