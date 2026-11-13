#pragma once

#include <lingua/core/types.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace lingua {

Format detect_format(std::span<const std::uint8_t> data);
std::string detect_mime(std::span<const std::uint8_t> data);
bool is_text(std::span<const std::uint8_t> data);

} // namespace lingua