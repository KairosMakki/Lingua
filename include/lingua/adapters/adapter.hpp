#pragma once

#include <lingua/core/result.hpp>
#include <lingua/core/types.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace lingua::adapters {

struct AdapterResult {
    bool supported = false;
    std::string tool;                 // delegate tool name
    std::vector<std::string> argv;    // command as argv vector
    std::string message;
};

class IAdapter {
public:
    virtual ~IAdapter() = default;
    virtual std::string name() const = 0;
    virtual AdapterResult can_handle(Format format, HostOS host) const = 0;
    virtual Result<AdapterResult> prepare(const std::filesystem::path& staged, HostOS host) const = 0;
};

HostOS detect_host_os();
std::vector<std::shared_ptr<IAdapter>> default_adapters();
std::shared_ptr<IAdapter> find_adapter(Format format, HostOS host);

} // namespace lingua::adapters