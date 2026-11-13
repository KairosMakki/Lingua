#include <lingua/platform/platform.hpp>
#include <lingua/core/error.hpp>

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

} // namespace lingua::platform