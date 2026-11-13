#pragma once

#include <lingua/core/format_parser.hpp>

namespace lingua::rpm {

class RpmParser final : public IFormatParser {
public:
    bool detect(std::span<const std::uint8_t> data) const override;
    Result<FileInfo> parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const override;
    const char* name() const noexcept override { return "rpm"; }
    Format format() const noexcept override { return Format::Rpm; }
};

} // namespace lingua::rpm