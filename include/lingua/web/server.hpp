#pragma once

#include <lingua/core/result.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace lingua::web {

struct ServerConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 8080;
    bool allow_remote = false;
    std::string tls_cert;
    std::string tls_key;
    std::string auth_token;
    std::uint64_t max_upload_bytes = 256ULL * 1024 * 1024;
    std::filesystem::path staging_dir;
};

class WebServer {
public:
    explicit WebServer(ServerConfig cfg);
    ~WebServer();
    Result<void> start();
    void stop();
    bool running() const;

public:
    struct Impl;
private:
    std::unique_ptr<Impl> impl_;
};

} // namespace lingua::web