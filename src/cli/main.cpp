#include <lingua/lingua.hpp>

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
              << "  inspect <file>           Inspect file\n"
              << "  extract <file> -o <dir>  Extract (print sections)\n"
              << "  import <file> [--dry-run] [--staging <dir>]\n";
}

static std::string read_file_arg(int argc, char** argv, int& i) {
    if (i + 1 >= argc) return {};
    return argv[++i];
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
        auto f = lingua::open(argv[2]);
        if (!f) { std::cerr << "error: " << f.error().message << "\n"; return 1; }
        auto info = lingua::inspect(*f);
        if (!info) { std::cerr << "error: " << info.error().message << "\n"; return 1; }
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
        std::cout << "metadata:\n";
        for (auto& [k,v] : info->metadata) std::cout << "  " << k << ": " << v << "\n";
        if (!info->warnings.empty()) {
            std::cout << "warnings:\n";
            for (auto& w : info->warnings) std::cout << "  - " << w << "\n";
        }
        return 0;
    }

    if (cmd == "extract") {
        // print section list to dir? We'll just list
        if (argc < 3) { std::cerr << "missing file\n"; return 1; }
        auto f = lingua::open(argv[2]);
        if (!f) { std::cerr << "error: " << f.error().message << "\n"; return 1; }
        auto info = lingua::inspect(*f);
        if (!info) { std::cerr << "error: " << info.error().message << "\n"; return 1; }
        std::string outdir = ".";
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "-o" && i+1 < argc) outdir = argv[++i];
        }
        fs::create_directories(outdir);
        std::cout << "sections: " << info->sections.size() << "\n";
        for (auto& s : info->sections) std::cout << s.name << "\t" << s.offset << "\t" << s.size << "\n";
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
            else if (a == "--staging" && i+1 < argc) opts.staging_dir = argv[++i];
            else if (a == "--sandbox") opts.sandbox = true;
        }
        auto res = lingua::import_app(*f, opts);
        if (!res) { std::cerr << "error: " << res.error().message << "\n"; return 1; }
        std::cout << "success: " << (res->success ? "true" : "false") << "\n";
        std::cout << "staged: " << res->staged_path.string() << "\n";
        if (!res->adapter_command.empty()) std::cout << "command: " << res->adapter_command << "\n";
        for (auto& w : res->warnings) std::cout << "warning: " << w << "\n";
        return 0;
    }

    std::cerr << "unknown command: " << cmd << "\n";
    print_usage();
    return 1;
}