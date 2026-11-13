#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace lingua {

enum class LogLevel : std::uint8_t { Trace, Debug, Info, Warn, Error, Off };

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(LogLevel level, const std::string& message) = 0;
};

class NullLogger final : public ILogger {
public:
    void log(LogLevel, const std::string&) override {}
};

/// Global logger accessor. Defaults to a null logger.
ILogger& logger();
void set_logger(std::shared_ptr<ILogger> logger);

} // namespace lingua