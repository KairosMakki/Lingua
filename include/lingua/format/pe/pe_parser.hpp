#pragma once

#include <lingua/core/format_parser.hpp>

namespace lingua::pe {

class PeParser final : public IFormatParser {
public:
    bool detect(std::span<const std::uint8_t> data) const override;
    Result<FileInfo> parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const override;
    const char* name() const noexcept override { return "pe"; }
    Format format() const noexcept override { return Format::PE; }
};

} // namespace lingua::pe