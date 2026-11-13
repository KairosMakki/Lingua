#include <lingua/platform/platform.hpp>
#include <lingua/core/error.hpp>

#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace lingua::platform {

HostOS detect_host_os() {
#if defined(_WIN32)
    return HostOS::Windows;
#elif defined(__APPLE__)
    return HostOS::MacOS;
#elif defined(__linux__)
    return HostOS::Linux;
#elif defined(__FreeBSD__)
    return HostOS::FreeBSD;
#else
    return HostOS::Unknown;
#endif
}

Result<int> spawn(const std::vector<std::string>& argv) {
    if (argv.empty()) return Error{ErrorCode::InvalidArgument, "empty argv"};
#if defined(_WIN32)
    std::string cmd;
    for (size_t i = 0; i < argv.size(); ++i) {
        if (i) cmd += " ";
        cmd += "\"" + argv[i] + "\"";
    }
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back('\0');
    if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        return Error{ErrorCode::InternalError, "CreateProcess failed"};
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
#else
    std::vector<char*> args;
    for (auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);
    pid_t pid;
    if (posix_spawn(&pid, argv[0].c_str(), nullptr, nullptr, args.data(), nullptr) != 0) {
        return Error{ErrorCode::InternalError, "posix_spawn failed"};
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return Error{ErrorCode::InternalError, "process did not exit normally"};
#endif
}

bool tool_exists(const std::string& name) {
    const char* pathenv = std::getenv("PATH");
    if (!pathenv) return false;
    std::string paths = pathenv;
    size_t start = 0;
    while (true) {
        size_t delim = paths.find_first_of(";:", start);
        std::string dir = (delim == std::string::npos) ? paths.substr(start) : paths.substr(start, delim-start);
        if (!dir.empty()) {
            std::filesystem::path candidate = std::filesystem::path(dir) / name;
#if defined(_WIN32)
            for (auto ext : {std::string(""), std::string(".exe"), std::string(".bat"), std::string(".cmd")}) {
                std::error_code ec;
                if (std::filesystem::exists(candidate.string() + ext, ec)) return true;
            }
#else
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec)) {
                auto perms = std::filesystem::status(candidate, ec).permissions();
                if ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) return true;
            }
#endif
        }
        if (delim == std::string::npos) break;
        start = delim + 1;
    }
    return false;
}

} // namespace lingua::platform