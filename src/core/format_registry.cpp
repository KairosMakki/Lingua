#include <lingua/core/format_parser.hpp>

#include <algorithm>
#include <mutex>
#include <vector>

namespace lingua {

struct FormatRegistry::Impl {
    std::vector<std::shared_ptr<IFormatParser>> parsers;
    std::mutex mtx;
};

FormatRegistry& FormatRegistry::instance() {
    static FormatRegistry r;
    if (!r.impl_) r.impl_ = std::make_unique<Impl>();
    return r;
}

void FormatRegistry::register_parser(std::shared_ptr<IFormatParser> parser) {
    if (!impl_) impl_ = std::make_unique<Impl>();
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->parsers.push_back(std::move(parser));
}

std::vector<std::shared_ptr<IFormatParser>> FormatRegistry::parsers() const {
    if (!impl_) return {};
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->parsers;
}

std::shared_ptr<IFormatParser> FormatRegistry::detect(std::span<const std::uint8_t> data) const {
    if (!impl_) return nullptr;
    std::lock_guard<std::mutex> lock(impl_->mtx);
    for (auto& p : impl_->parsers) {
        if (p && p->detect(data)) return p;
    }
    return nullptr;
}

} // namespace lingua