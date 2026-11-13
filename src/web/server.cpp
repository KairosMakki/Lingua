#include <lingua/web/server.hpp>
#include <lingua/lingua.hpp>
#include <lingua/core/error.hpp>
#include <lingua/core/logger.hpp>
#include <lingua/import/import.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <span>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
#define CLOSESOCK closesocket
#define INVALID_SOCK INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#define CLOSESOCK close
#define INVALID_SOCK -1
#endif

#if defined(LINGUA_HAVE_OPENSSL)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

namespace fs = std::filesystem;
namespace lingua::web {

struct WebServer::Impl {
    ServerConfig cfg;
    std::atomic<bool> running{false};
    socket_t listen_sock = INVALID_SOCK;
    std::thread accept_thread;
#if defined(LINGUA_HAVE_OPENSSL)
    SSL_CTX* ssl_ctx = nullptr;
#endif
};

namespace {
std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf;
                } else out += c;
        }
    }
    return out;
}

bool is_localhost(const std::string& host) {
    return host == "127.0.0.1" || host == "localhost" || host == "::1";
}

socket_t create_listen(const std::string& host, uint16_t port) {
#if defined(_WIN32)
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCK) return INVALID_SOCK;
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (host == "0.0.0.0") addr.sin_addr.s_addr = INADDR_ANY;
    else inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { CLOSESOCK(s); return INVALID_SOCK; }
    if (listen(s, 16) != 0) { CLOSESOCK(s); return INVALID_SOCK; }
    return s;
}

std::string read_request(socket_t s, size_t max_bytes, std::vector<char>& body_out) {
    std::string req;
    char buf[8192];
    // read headers
    while (req.find("\r\n\r\n") == std::string::npos && req.size() < 65536) {
        int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        req.append(buf, n);
    }
    size_t hdr_end = req.find("\r\n\r\n");
    if (hdr_end == std::string::npos) return req;
    std::string headers = req.substr(0, hdr_end);
    size_t body_start = hdr_end + 4;
    std::string body = req.substr(body_start);
    // content-length
    size_t cl = 0;
    std::string low = headers;
    std::transform(low.begin(), low.end(), low.begin(), ::tolower);
    size_t pos = low.find("content-length:");
    if (pos != std::string::npos) {
        cl = std::stoull(headers.substr(pos + 15));
    }
    while (body.size() < cl && body.size() < max_bytes) {
        int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        body.append(buf, n);
        if (body.size() > max_bytes) break;
    }
    if (body.size() > cl) body.resize(cl);
    body_out.assign(body.begin(), body.end());
    return headers;
}

void send_response(socket_t s, int status, const std::string& content_type, const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << (status==200?"OK":(status==400?"Bad Request":(status==401?"Unauthorized":(status==404?"Not Found":"Internal Error")))) << "\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << body;
    std::string r = oss.str();
    send(s, r.c_str(), (int)r.size(), 0);
}

std::string get_header(const std::string& headers, const std::string& name) {
    std::istringstream in(headers);
    std::string line;
    std::getline(in, line); // request line
    std::string lowname = name;
    std::transform(lowname.begin(), lowname.end(), lowname.begin(), ::tolower);
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        if (key == lowname) {
            std::string val = line.substr(colon+1);
            while (!val.empty() && val.front() == ' ') val.erase(val.begin());
            return val;
        }
    }
    return {};
}

std::string get_path(const std::string& headers) {
    std::istringstream in(headers);
    std::string line;
    std::getline(in, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream ls(line);
    std::string method, path, ver;
    ls >> method >> path >> ver;
    return path;
}
std::string get_method(const std::string& headers) {
    std::istringstream in(headers);
    std::string line;
    std::getline(in, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream ls(line);
    std::string method, path, ver;
    ls >> method >> path >> ver;
    return method;
}

const char* INDEX_HTML = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Lingua Import</title></head>
<body>
<h1>Lingua Import</h1>
<p>Upload an application package or executable for inspection. Imported code is untrusted.</p>
<input type="file" id="f">
<button onclick="upload()">Upload & Inspect</button>
<pre id="out"></pre>
<script>
async function upload(){
  const file=document.getElementById('f').files[0];
  if(!file){alert('choose file');return;}
  const buf=await file.arrayBuffer();
  const r=await fetch('/api/inspect',{method:'POST',body:buf});
  const j=await r.json();
  document.getElementById('out').textContent=JSON.stringify(j,null,2);
}
</script>
</body></html>)HTML";

void handle_client(socket_t s, WebServer::Impl* impl) {
    std::vector<char> body;
    std::string headers = read_request(s, impl->cfg.max_upload_bytes, body);
    if (headers.empty()) { CLOSESOCK(s); return; }

    // auth check
    std::string host = impl->cfg.host;
    if (!is_localhost(host)) {
        std::string token = get_header(headers, "Authorization");
        std::string expected = "Bearer " + impl->cfg.auth_token;
        if (impl->cfg.auth_token.empty() || token != expected) {
            send_response(s, 401, "application/json", "{\"error\":\"unauthorized\"}");
            CLOSESOCK(s); return;
        }
    }

    std::string method = get_method(headers);
    std::string path = get_path(headers);

    if (method == "GET" && (path == "/" || path == "/index.html")) {
        send_response(s, 200, "text/html", INDEX_HTML);
        CLOSESOCK(s); return;
    }

    // Save body to temp file
    auto save_temp = [&](const std::vector<char>& data) -> fs::path {
        fs::path dir = impl->cfg.staging_dir.empty() ? fs::temp_directory_path() / "lingua-web" : impl->cfg.staging_dir;
        fs::create_directories(dir);
        fs::path p = dir / "upload.bin";
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out.write(data.data(), data.size());
        out.close();
        return p;
    };

    if (method == "POST" && path == "/api/detect") {
        if (body.size() > impl->cfg.max_upload_bytes) { send_response(s,400,"application/json","{\"error\":\"too large\"}"); CLOSESOCK(s); return; }
        std::span<const uint8_t> sp(reinterpret_cast<const uint8_t*>(body.data()), body.size());
        Format fmt = detect_format(sp);
        std::string json = "{\"format\":\"" + std::string(to_string(fmt)) + "\"}";
        send_response(s, 200, "application/json", json);
        CLOSESOCK(s); return;
    }

    if (method == "POST" && path == "/api/inspect") {
        if (body.size() > impl->cfg.max_upload_bytes) { send_response(s,400,"application/json","{\"error\":\"too large\"}"); CLOSESOCK(s); return; }
        fs::path tmp = save_temp(body);
        auto f = open(tmp);
        if (!f) { send_response(s,400,"application/json","{\"error\":\"open failed\"}"); CLOSESOCK(s); return; }
        auto info = inspect(*f);
        if (!info) { send_response(s,400,"application/json","{\"error\":\"inspect failed\"}"); CLOSESOCK(s); return; }
        std::ostringstream json;
        json << "{\"format\":\"" << to_string(info->format) << "\","
             << "\"architecture\":\"" << to_string(info->architecture) << "\","
             << "\"endianness\":\"" << (info->endianness==Endianness::Little?"little":(info->endianness==Endianness::Big?"big":"unknown")) << "\","
             << "\"size\":" << info->file_size << ","
             << "\"dependencies\":[";
        bool first = true;
        for (auto& d : info->dependencies) { if (!first) json << ","; first=false; json << "\"" << json_escape(d.name) << "\""; }
        json << "],\"imports\":[";
        first = true;
        for (auto& im : info->imports) { if (!first) json << ","; first=false; json << "\"" << json_escape(im) << "\""; }
        json << "],\"warnings\":[";
        first = true;
        for (auto& w : info->warnings) { if (!first) json << ","; first=false; json << "\"" << json_escape(w) << "\""; }
        json << "]}";
        send_response(s, 200, "application/json", json.str());
        CLOSESOCK(s); return;
    }

    if (method == "POST" && path.rfind("/api/import",0)==0) {
        if (body.size() > impl->cfg.max_upload_bytes) { send_response(s,400,"application/json","{\"error\":\"too large\"}"); CLOSESOCK(s); return; }
        fs::path tmp = save_temp(body);
        auto f = open(tmp);
        if (!f) { send_response(s,400,"application/json","{\"error\":\"open failed\"}"); CLOSESOCK(s); return; }
        ImportOptions opts;
        opts.dry_run = (path.find("dry_run=1") != std::string::npos);
        opts.staging_dir = impl->cfg.staging_dir.empty() ? fs::temp_directory_path() / "lingua-staging" : impl->cfg.staging_dir;
        auto res = import_app(*f, opts);
        if (!res) { send_response(s,400,"application/json","{\"error\":\"import failed\"}"); CLOSESOCK(s); return; }
        std::ostringstream json;
        json << "{\"success\":" << (res->success?"true":"false")
             << ",\"staged\":\"" << json_escape(res->staged_path.string()) << "\""
             << ",\"command\":\"" << json_escape([&](){ std::string c; for (size_t i=0;i<res->adapter_argv.size();++i){ if(i)c+=" "; c+=res->adapter_argv[i]; } return c; }()) << "\""
             << ",\"warnings\":[";
        bool first = true;
        for (auto& w : res->warnings) { if (!first) json << ","; first=false; json << "\"" << json_escape(w) << "\""; }
        json << "]}";
        send_response(s, 200, "application/json", json.str());
        CLOSESOCK(s); return;
    }

    send_response(s, 404, "application/json", "{\"error\":\"not found\"}");
    CLOSESOCK(s);
}

} // anon

WebServer::WebServer(ServerConfig cfg) : impl_(std::make_unique<Impl>()) {
    impl_->cfg = std::move(cfg);
}
WebServer::~WebServer() { stop(); }

Result<void> WebServer::start() {
    if (impl_->running) return Error{ErrorCode::InvalidArgument, "already running"};
    std::string host = impl_->cfg.host;
    if (!is_localhost(host) && impl_->cfg.auth_token.empty()) {
        return Error{ErrorCode::InvalidArgument, "auth token required for non-localhost binding"};
    }
    if (!impl_->cfg.tls_cert.empty()) {
        // TLS not implemented in this build
        return Error{ErrorCode::UnsupportedFormat, "TLS not implemented in this build"};
    }
    impl_->listen_sock = create_listen(host, impl_->cfg.port);
    if (impl_->listen_sock == INVALID_SOCK) {
        return Error{ErrorCode::IoError, "cannot bind socket"};
    }
    impl_->running = true;
    impl_->accept_thread = std::thread([this]() {
        while (impl_->running) {
            sockaddr_in caddr{};
            socklen_t clen = sizeof(caddr);
            socket_t c = accept(impl_->listen_sock, reinterpret_cast<sockaddr*>(&caddr), &clen);
            if (c == INVALID_SOCK) {
                if (impl_->running) std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            std::thread(handle_client, c, impl_.get()).detach();
        }
    });
    return {};
}

void WebServer::stop() {
    impl_->running = false;
    if (impl_->listen_sock != INVALID_SOCK) { CLOSESOCK(impl_->listen_sock); impl_->listen_sock = INVALID_SOCK; }
    if (impl_->accept_thread.joinable()) impl_->accept_thread.join();
}

bool WebServer::running() const { return impl_->running.load(); }

} // namespace lingua::web