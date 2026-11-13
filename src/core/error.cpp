#include <lingua/core/error.hpp>

namespace lingua {

std::string Error::to_string() const {
    return std::to_string(static_cast<int>(code)) + ": " + message + (context.empty() ? "" : " (" + context + ")");
}

} // namespace lingua