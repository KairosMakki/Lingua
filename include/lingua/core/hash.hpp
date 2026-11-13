#pragma once
#include <lingua/core/types.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace lingua {
std::string sha256(std::span<const std::uint8_t> data);
} // namespace lingua