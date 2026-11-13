#include <lingua/core/byte_reader.hpp>

#include <algorithm>
#include <cstring>

namespace lingua {

ByteReader::ByteReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
ByteReader::ByteReader(std::span<const std::uint8_t> data) : data_(data.data()), size_(data.size()) {}

bool ByteReader::skip(std::size_t n) noexcept {
    if (n > remaining()) return false;
    offset_ += n;
    return true;
}

bool ByteReader::add_overflow(std::size_t a, std::size_t b, std::size_t& out) noexcept {
    if (a > (static_cast<std::size_t>(-1) - b)) return true;
    out = a + b;
    return false;
}

Result<std::uint8_t> ByteReader::read_u8() {
    if (remaining() < 1) return Error{ErrorCode::TruncatedInput, "truncated input"};
    std::uint8_t v = data_[offset_];
    ++offset_;
    return v;
}

Result<std::uint16_t> ByteReader::read_u16_le() {
    if (remaining() < 2) return Error{ErrorCode::TruncatedInput, "truncated input"};
    std::uint16_t v = static_cast<std::uint16_t>(data_[offset_]) |
                       (static_cast<std::uint16_t>(data_[offset_ + 1]) << 8);
    offset_ += 2;
    return v;
}
Result<std::uint16_t> ByteReader::read_u16_be() {
    if (remaining() < 2) return Error{ErrorCode::TruncatedInput, "truncated input"};
    std::uint16_t v = (static_cast<std::uint16_t>(data_[offset_]) << 8) |
                       static_cast<std::uint16_t>(data_[offset_ + 1]);
    offset_ += 2;
    return v;
}
Result<std::uint32_t> ByteReader::read_u32_le() {
    if (remaining() < 4) return Error{ErrorCode::TruncatedInput, "truncated input"};
    std::uint32_t v = static_cast<std::uint32_t>(data_[offset_]) |
                      (static_cast<std::uint32_t>(data_[offset_ + 1]) << 8) |
                      (static_cast<std::uint32_t>(data_[offset_ + 2]) << 16) |
                      (static_cast<std::uint32_t>(data_[offset_ + 3]) << 24);
    offset_ += 4;
    return v;
}
Result<std::uint32_t> ByteReader::read_u32_be() {
    if (remaining() < 4) return Error{ErrorCode::TruncatedInput, "truncated input"};
    std::uint32_t v = (static_cast<std::uint32_t>(data_[offset_]) << 24) |
                      (static_cast<std::uint32_t>(data_[offset_ + 1]) << 16) |
                      (static_cast<std::uint32_t>(data_[offset_ + 2]) << 8) |
                      static_cast<std::uint32_t>(data_[offset_ + 3]);
    offset_ += 4;
    return v;
}
Result<std::uint64_t> ByteReader::read_u64_le() {
    if (remaining() < 8) return Error{ErrorCode::TruncatedInput, "truncated input"};
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (static_cast<std::uint64_t>(data_[offset_ + i]) << (i * 8));
    offset_ += 8;
    return v;
}
Result<std::uint64_t> ByteReader::read_u64_be() {
    if (remaining() < 8) return Error{ErrorCode::TruncatedInput, "truncated input"};
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | data_[offset_ + i];
    offset_ += 8;
    return v;
}
Result<std::vector<std::uint8_t>> ByteReader::read_bytes(std::size_t n) {
    if (n > remaining()) return Error{ErrorCode::TruncatedInput, "truncated input"};
    std::vector<std::uint8_t> out(data_ + offset_, data_ + offset_ + n);
    offset_ += n;
    return out;
}
Result<std::string> ByteReader::read_cstring(std::size_t max_len) {
    std::string out;
    while (remaining() > 0 && out.size() < max_len) {
        char c = static_cast<char>(data_[offset_]);
        ++offset_;
        if (c == '\0') break;
        out.push_back(c);
    }
    return out;
}
Result<std::string> ByteReader::read_fixed_string(std::size_t n) {
    if (n > remaining()) return Error{ErrorCode::TruncatedInput, "truncated input"};
    std::string out(reinterpret_cast<const char*>(data_ + offset_), n);
    offset_ += n;
    // trim trailing nulls
    while (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

} // namespace lingua