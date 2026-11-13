#pragma once

#include <lingua/core/error.hpp>
#include <lingua/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace lingua {

/// A bounds-checked view over a byte buffer. All reads return Result and never
/// throw. Integer overflow is guarded.
class ByteReader {
public:
    ByteReader(const std::uint8_t* data, std::size_t size);
    explicit ByteReader(std::span<const std::uint8_t> data);

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
    [[nodiscard]] bool eof() const noexcept { return offset_ >= size_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return (offset_ >= size_) ? 0 : (size_ - offset_); }

    void set_offset(std::size_t o) noexcept { offset_ = o; }
    bool skip(std::size_t n) noexcept;

    Result<std::uint8_t> read_u8();
    Result<std::uint16_t> read_u16_le();
    Result<std::uint16_t> read_u16_be();
    Result<std::uint32_t> read_u32_le();
    Result<std::uint32_t> read_u32_be();
    Result<std::uint64_t> read_u64_le();
    Result<std::uint64_t> read_u64_be();
    Result<std::vector<std::uint8_t>> read_bytes(std::size_t n);
    Result<std::string> read_cstring(std::size_t max_len = 4096);
    Result<std::string> read_fixed_string(std::size_t n);

    [[nodiscard]] const std::uint8_t* data() const noexcept { return data_; }

    static bool add_overflow(std::size_t a, std::size_t b, std::size_t& out) noexcept;

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t offset_ = 0;
};

/// Endianness helpers.
[[nodiscard]] inline std::uint16_t swap16(std::uint16_t v) noexcept {
    return static_cast<std::uint16_t>((v >> 8) | (v << 8));
}
[[nodiscard]] inline std::uint32_t swap32(std::uint32_t v) noexcept {
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
}
[[nodiscard]] inline std::uint64_t swap64(std::uint64_t v) noexcept {
    return ((v & 0x00000000000000FFull) << 56) | ((v & 0x000000000000FF00ull) << 40) |
           ((v & 0x0000000000FF0000ull) << 24) | ((v & 0x00000000FF000000ull) << 8) |
           ((v & 0x000000FF00000000ull) >> 8) | ((v & 0x0000FF0000000000ull) >> 24) |
           ((v & 0x00FF000000000000ull) >> 40) | ((v & 0xFF00000000000000ull) >> 56);
}

} // namespace lingua