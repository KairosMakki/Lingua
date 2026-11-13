#pragma once

#include <lingua/core/result.hpp>
#include <lingua/core/types.hpp>

#include <string>
#include <vector>

namespace lingua::platform {

HostOS detect_host_os();

/// Run a command given as an argv vector. Returns the process exit code.
Result<int> spawn(const std::vector<std::string>& argv);

/// Check whether an executable exists on PATH.
bool tool_exists(const std::string& name);

} // namespace lingua::platform