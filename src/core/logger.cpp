#include <lingua/core/logger.hpp>

#include <memory>
#include <mutex>

namespace lingua {

namespace {
std::shared_ptr<ILogger> g_logger = std::make_shared<NullLogger>();
std::mutex g_mutex;
}

ILogger& logger() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return *g_logger;
}
void set_logger(std::shared_ptr<ILogger> l) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (l) g_logger = std::move(l);
    else g_logger = std::make_shared<NullLogger>();
}

} // namespace lingua