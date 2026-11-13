#include <lingua/adapters/adapter.hpp>
#include <lingua/core/error.hpp>
#include <lingua/platform/platform.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace lingua::adapters {

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

namespace {

bool tool_exists(const std::string& name) {
#if defined(_WIN32)
    // check PATH
    char buf[4096];
    std::string cmd = "where " + name + " >nul 2>&1";
    return std::system(cmd.c_str()) == 0;
#else
    std::string cmd = "command -v " + name + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
#endif
}

class PeAdapter : public IAdapter {
public:
    std::string name() const override { return "pe-adapter"; }
    AdapterResult can_handle(Format format, HostOS host) const override {
        AdapterResult r;
        if (format != Format::PE) { r.supported = false; r.message = "not a PE file"; return r; }
        if (host == HostOS::Windows) {
            r.supported = true; r.tool = "native"; r.message = "run via native Windows loader";
        } else if (host == HostOS::Linux || host == HostOS::MacOS) {
            if (tool_exists("wine")) {
                r.supported = true; r.tool = "wine"; r.message = "run via Wine";
            } else {
                r.supported = false; r.tool = "wine"; r.message = "Wine not found; install wine to run PE binaries";
            }
        } else {
            r.supported = false; r.message = "unsupported host";
        }
        return r;
    }
    Result<AdapterResult> prepare(const std::filesystem::path& staged, HostOS host) const override {
        AdapterResult r = can_handle(Format::PE, host);
        if (!r.supported) return Error{ErrorCode::UnsupportedHost, r.message};
        if (r.tool == "wine") r.command = "wine " + staged.string();
        else r.command = staged.string();
        return r;
    }
};

class ElfAdapter : public IAdapter {
public:
    std::string name() const override { return "elf-adapter"; }
    AdapterResult can_handle(Format format, HostOS host) const override {
        AdapterResult r;
        if (format != Format::ELF && format != Format::AppImage) { r.supported = false; r.message = "not an ELF/AppImage"; return r; }
        if (host == HostOS::Linux || host == HostOS::FreeBSD || host == HostOS::MacOS) {
            r.supported = true; r.tool = "native"; r.message = "run via native loader";
        } else {
            r.supported = false; r.message = "ELF not natively supported on this host";
        }
        return r;
    }
    Result<AdapterResult> prepare(const std::filesystem::path& staged, HostOS host) const override {
        AdapterResult r = can_handle(Format::ELF, host);
        if (!r.supported) return Error{ErrorCode::UnsupportedHost, r.message};
        r.command = staged.string();
        return r;
    }
};

class DebAdapter : public IAdapter {
public:
    std::string name() const override { return "deb-adapter"; }
    AdapterResult can_handle(Format format, HostOS host) const override {
        AdapterResult r;
        if (format != Format::Deb) { r.supported = false; r.message = "not a deb"; return r; }
        if (host == HostOS::Linux) {
            if (tool_exists("dpkg")) { r.supported = true; r.tool = "dpkg"; r.message = "install via dpkg/apt"; }
            else { r.supported = false; r.tool = "dpkg"; r.message = "dpkg not found"; }
        } else {
            r.supported = false; r.message = "deb install only on Linux; extraction available";
        }
        return r;
    }
    Result<AdapterResult> prepare(const std::filesystem::path& staged, HostOS host) const override {
        AdapterResult r = can_handle(Format::Deb, host);
        if (host != HostOS::Linux) {
            r.supported = true; r.tool = "extract"; r.message = "extract contents only"; r.command = "extract " + staged.string();
            return r;
        }
        if (!r.supported) return Error{ErrorCode::UnsupportedHost, r.message};
        r.command = "dpkg -i " + staged.string();
        return r;
    }
};

class AppImageAdapter : public IAdapter {
public:
    std::string name() const override { return "appimage-adapter"; }
    AdapterResult can_handle(Format format, HostOS host) const override {
        AdapterResult r;
        if (format != Format::AppImage) { r.supported = false; r.message = "not an AppImage"; return r; }
        if (host == HostOS::Linux) {
            r.supported = true; r.tool = "native"; r.message = "execute or mount";
        } else {
            r.supported = false; r.message = "AppImage requires Linux";
        }
        return r;
    }
    Result<AdapterResult> prepare(const std::filesystem::path& staged, HostOS host) const override {
        AdapterResult r = can_handle(Format::AppImage, host);
        if (!r.supported) return Error{ErrorCode::UnsupportedHost, r.message};
        r.command = staged.string();
        return r;
    }
};

class MachOAdapter : public IAdapter {
public:
    std::string name() const override { return "macho-adapter"; }
    AdapterResult can_handle(Format format, HostOS host) const override {
        AdapterResult r;
        if (format != Format::MachO) { r.supported = false; r.message = "not a Mach-O file"; return r; }
        if (host == HostOS::MacOS) { r.supported = true; r.tool = "native"; r.message = "run via native loader"; }
        else { r.supported = false; r.message = "Mach-O requires macOS"; }
        return r;
    }
    Result<AdapterResult> prepare(const std::filesystem::path& staged, HostOS host) const override {
        AdapterResult r = can_handle(Format::MachO, host);
        if (!r.supported) return Error{ErrorCode::UnsupportedHost, r.message};
        r.command = staged.string();
        return r;
    }
};

class RpmAdapter : public IAdapter {
public:
    std::string name() const override { return "rpm-adapter"; }
    AdapterResult can_handle(Format format, HostOS host) const override {
        AdapterResult r;
        if (format != Format::Rpm) { r.supported = false; r.message = "not an rpm"; return r; }
        if (host == HostOS::Linux) {
            if (tool_exists("rpm")) { r.supported = true; r.tool = "rpm"; r.message = "install via rpm"; }
            else { r.supported = false; r.tool = "rpm"; r.message = "rpm not found; extraction available"; }
        } else {
            r.supported = false; r.message = "rpm install only on Linux; extraction available";
        }
        return r;
    }
    Result<AdapterResult> prepare(const std::filesystem::path& staged, HostOS host) const override {
        AdapterResult r = can_handle(Format::Rpm, host);
        if (host != HostOS::Linux) {
            r.supported = true; r.tool = "extract"; r.message = "extract contents only"; r.command = "extract " + staged.string();
            return r;
        }
        if (!r.supported) return Error{ErrorCode::UnsupportedHost, r.message};
        r.command = "rpm -i " + staged.string();
        return r;
    }
};

class ApkAdapter : public IAdapter {
public:
    std::string name() const override { return "apk-adapter"; }
    AdapterResult can_handle(Format format, HostOS host) const override {
        AdapterResult r;
        if (format != Format::Apk) { r.supported = false; r.message = "not an apk"; return r; }
        r.supported = false; r.message = "APK install requires Android; inspection/extraction available";
        return r;
    }
    Result<AdapterResult> prepare(const std::filesystem::path& staged, HostOS host) const override {
        AdapterResult r;
        r.supported = true; r.tool = "extract"; r.message = "extract contents only"; r.command = "extract " + staged.string();
        return r;
    }
};

} // namespace

std::vector<std::shared_ptr<IAdapter>> default_adapters() {
    std::vector<std::shared_ptr<IAdapter>> v;
    v.push_back(std::make_shared<PeAdapter>());
    v.push_back(std::make_shared<ElfAdapter>());
    v.push_back(std::make_shared<DebAdapter>());
    v.push_back(std::make_shared<AppImageAdapter>());
    v.push_back(std::make_shared<MachOAdapter>());
    v.push_back(std::make_shared<RpmAdapter>());
    v.push_back(std::make_shared<ApkAdapter>());
    return v;
}

std::shared_ptr<IAdapter> find_adapter(Format format, HostOS host) {
    for (auto& a : default_adapters()) {
        auto r = a->can_handle(format, host);
        if (r.supported) return a;
    }
    // return first matching format even if unsupported, for messaging
    for (auto& a : default_adapters()) {
        auto r = a->can_handle(format, host);
        if (r.message.find("not a") == std::string::npos && r.message.find("not an") == std::string::npos) return a;
    }
    return nullptr;
}

} // namespace lingua::adapters

namespace lingua::platform {
HostOS detect_host_os() { return lingua::adapters::detect_host_os(); }
}