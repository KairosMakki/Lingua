#include <lingua/lingua.hpp>
#include <lingua/core/archive.hpp>

using namespace lingua;
using namespace lingua::importing;

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void print_usage() {
    std::cout << "Usage: lingua <command> [options]\n"
              << "Commands:\n"
              << "  detect <file>            Detect format\n"
              << "  inspect <file> [--json] [--security]\n"
              << "  extract <file> -o <dir>  Extract contents\n"
              << "  import <file> [--dry-run] [--staging <dir>] [--sandbox]\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(); return 1; }
    std::string cmd = argv[1];
    if (cmd == "--help" || cmd == "-h") { print_usage(); return 0; }

    if (cmd == "detect") {
        if (argc < 3) { std::cerr << "missing file\n"; return 1; }
        auto f = lingua::open(argv[2]);
        if (!f) { std::cerr << "error: " << f.error().message << "\n"; return 1; }
        auto info = lingua::inspect(*f);
        if (!info) { std::cerr << "error: " << info.error().message << "\n"; return 1; }
        std::cout << to_string(info->format) << "\n";
        return 0;
    }

    if (cmd == "inspect") {
        if (argc < 3) { std::cerr << "missing file\n"; return 1; }
        bool json = false, security = false;
        for (int i = 3; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--json") json = true;
            if (a == "--security") security = true;
        }
        auto f = lingua::open(argv[2]);
        if (!f) { std::cerr << "error: " << f.error().message << "\n"; return 1; }
        auto info = lingua::inspect(*f);
        if (!info) { std::cerr << "error: " << info.error().message << "\n"; return 1; }
        if (json) {
            std::cout << "{\"format\":\"" << to_string(info->format) << "\""
                      << ",\"architecture\":\"" << to_string(info->architecture) << "\""
                      << ",\"endianness\":\"" << (info->endianness == Endianness::Little ? "little" : (info->endianness == Endianness::Big ? "big" : "unknown")) << "\""
                      << ",\"size\":" << info->file_size
                      << ",\"sections\":" << info->sections.size()
                      << ",\"dependencies\":[";
            for (size_t i = 0; i < info->dependencies.size(); ++i) { if (i) std::cout << ","; std::cout << "\"" << info->dependencies[i].name << "\""; }
            std::cout << "],\"imports\":[";
            for (size_t i = 0; i < info->imports.size(); ++i) { if (i) std::cout << ","; std::cout << "\"" << info->imports[i] << "\""; }
            std::cout << "],\"exports\":[";
            for (size_t i = 0; i < info->exports.size(); ++i) { if (i) std::cout << ","; std::cout << "\"" << info->exports[i] << "\""; }
            std::cout << "],\"metadata\":{";
            bool first = true;
            for (auto& [k,v] : info->metadata) {
                if (!first) std::cout << ",";
                first = false;
                std::cout << "\"" << k << "\":\"" << v << "\"";
            }
            std::cout << "},\"warnings\":[";
            for (size_t i = 0; i < info->warnings.size(); ++i) { if (i) std::cout << ","; std::cout << "\"" << info->warnings[i] << "\""; }
            std::cout << "]}\n";
            return 0;
        }
        std::cout << "format: " << to_string(info->format) << "\n";
        std::cout << "architecture: " << to_string(info->architecture) << "\n";
        std::cout << "endianness: " << (info->endianness == Endianness::Little ? "little" : (info->endianness == Endianness::Big ? "big" : "unknown")) << "\n";
        std::cout << "size: " << info->file_size << "\n";
        std::cout << "sha256: " << info->sha256 << "\n";
        std::cout << "sections: " << info->sections.size() << "\n";
        for (auto& s : info->sections) std::cout << "  - " << s.name << " (size=" << s.size << ")\n";
        std::cout << "dependencies: " << info->dependencies.size() << "\n";
        for (auto& d : info->dependencies) std::cout << "  - " << d.name << " " << d.version_constraint << "\n";
        std::cout << "imports: " << info->imports.size() << "\n";
        for (auto& im : info->imports) std::cout << "  - " << im << "\n";
        std::cout << "exports: " << info->exports.size() << "\n";
        for (auto& ex : info->exports) std::cout << "  - " << ex << "\n";
        std::cout << "metadata:\n";
        for (auto& [k,v] : info->metadata) std::cout << "  " << k << ": " << v << "\n";
        if (security) {
            std::cout << "security:\n";
            for (auto key : {"pie","nx","relro","stack_canary","authenticode","dotnet"}) {
                auto it = info->metadata.find(key);
                if (it != info->metadata.end()) std::cout << "  " << key << ": " << it->second << "\n";
            }
        }
        if (!info->warnings.empty()) {
            std::cout << "warnings:\n";
            for (auto& w : info->warnings) std::cout << "  - " << w << "\n";
        }
        return 0;
    }

    if (cmd == "extract") {
        if (argc < 3) { std::cerr << "missing file\n"; return 1; }
        std::string outdir = ".";
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "-o" && i + 1 < argc) outdir = argv[++i];
        }
        fs::create_directories(outdir);
        auto f = lingua::open(argv[2]);
        if (!f) { std::cerr << "error: " << f.error().message << "\n"; return 1; }
        auto info = lingua::inspect(*f);
        if (!info) { std::cerr << "error: " << info.error().message << "\n"; return 1; }

        auto write_member = [&](const std::string& name, const std::vector<uint8_t>& data) {
            fs::path dest = safe_join(outdir, name);
            if (dest.empty()) { std::cerr << "skip unsafe path: " << name << "\n"; return; }
            fs::create_directories(dest.parent_path());
            std::ofstream out(dest, std::ios::binary);
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            std::cout << "extracted: " << name << "\n";
        };

        if (info->format == Format::Deb) {
            std::vector<uint8_t> buf(f->mapping.span().begin(), f->mapping.span().end());
            auto members = archive::parse_ar(buf);
            for (auto& m : members) {
                if (m.name.rfind("data.tar", 0) == 0) {
                    auto tar = archive::decompress(m.data);
                    if (tar.empty()) tar = m.data;
                    for (auto& e : archive::parse_tar(tar)) {
                        if (e.is_dir) continue;
                        write_member(e.name, e.data);
                    }
                }
            }
        } else if (info->format == Format::Apk) {
            std::vector<uint8_t> buf(f->mapping.span().begin(), f->mapping.span().end());
            for (auto& e : archive::parse_zip(buf)) {
                if (e.is_dir) continue;
                write_member(e.name, e.data);
            }
        } else if (info->format == Format::Rpm) {
            std::vector<uint8_t> buf(f->mapping.span().begin(), f->mapping.span().end());
            // find payload offset from metadata? parse ar? Use sections
            size_t off = 0, sz = 0;
            for (auto& s : info->sections) { if (s.name == "payload") { off = s.offset; sz = s.size; } }
            if (sz == 0 && !buf.empty()) { off = 0; sz = buf.size(); }
            std::vector<uint8_t> payload(buf.begin() + off, buf.begin() + off + sz);
            auto dec = archive::decompress(payload);
            if (dec.empty()) dec = payload;
            for (auto& e : archive::parse_cpio(dec)) {
                if (e.is_dir) continue;
                write_member(e.name, e.data);
            }
        } else if (info->format == Format::AppImage) {
            auto span = f->mapping.span();
            // find payload
            size_t off = 0; std::string type;
            for (size_t i = 0; i + 4 <= span.size(); ++i) {
                if (span[i]=='h' && span[i+1]=='s' && span[i+2]=='q' && span[i+3]=='s') { off=i; type="squashfs"; break; }
                if (i+5<=span.size() && span[i]=='C' && span[i+1]=='D' && span[i+2]=='0' && span[i+3]=='0' && span[i+4]=='1') { off=i; type="iso9660"; break; }
            }
            if (off > 0) {
                std::vector<uint8_t> payload(span.begin()+off, span.end());
                write_member(std::string("payload.") + (type=="squashfs"?"squashfs":"iso"), payload);
            }
        } else {
            // extract sections
            auto span = f->mapping.span();
            for (auto& s : info->sections) {
                if (s.offset + s.size <= span.size()) {
                    std::vector<uint8_t> data(span.begin() + s.offset, span.begin() + s.offset + s.size);
                    std::string name = s.name.empty() ? "section" : s.name;
                    // sanitize filename
                    for (char& c : name) if (c=='/'||c=='\\') c='_';
                    write_member(name, data);
                }
            }
        }
        return 0;
    }

    if (cmd == "import") {
        if (argc < 3) { std::cerr << "missing file\n"; return 1; }
        auto f = lingua::open(argv[2]);
        if (!f) { std::cerr << "error: " << f.error().message << "\n"; return 1; }
        ImportOptions opts;
        for (int i = 3; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--dry-run") opts.dry_run = true;
            else if (a == "--staging" && i + 1 < argc) opts.staging_dir = argv[++i];
            else if (a == "--sandbox") opts.sandbox = true;
        }
        auto res = lingua::import_app(*f, opts);
        if (!res) { std::cerr << "error: " << res.error().message << "\n"; return 1; }
        std::cout << "success: " << (res->success ? "true" : "false") << "\n";
        std::cout << "staged: " << res->staged_path.string() << "\n";
        if (!res->adapter_argv.empty()) {
            std::cout << "command:";
            for (auto& a : res->adapter_argv) std::cout << " " << a;
            std::cout << "\n";
        }
        for (auto& w : res->warnings) std::cout << "warning: " << w << "\n";
        return 0;
    }

    std::cerr << "unknown command: " << cmd << "\n";
    print_usage();
    return 1;
}