#pragma once
#include <lingua/core/format_parser.hpp>
namespace lingua::snap {
class SnapParser final : public IFormatParser {
public:
    bool detect(std::span<const std::uint8_t> data) const override;
    Result<FileInfo> parse(std::span<const std::uint8_t> data, const std::filesystem::path& path) const override;
    const char* name() const noexcept override { return "snap"; }
    Format format() const noexcept override { return Format::Snap; }
};
} // namespace lingua::snap