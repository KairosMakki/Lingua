#include <lingua/lingua.hpp>

#include <lingua/adapters/adapter.hpp>
#include <lingua/core/error.hpp>
#include <lingua/core/mapped_file.hpp>
#include <lingua/detect/detector.hpp>
#include <lingua/format/appimage/appimage_parser.hpp>
#include <lingua/format/deb/deb_parser.hpp>
#include <lingua/format/elf/elf_parser.hpp>
#include <lingua/format/pe/pe_parser.hpp>
#include <lingua/import/import.hpp>

#include <filesystem>
#include <memory>
#include <span>

namespace lingua {

Result<File> open(const std::filesystem::path& path) {
    auto mapped = MappedFile::open(path);
    if (!mapped) return mapped.error();
    File f;
    f.path = path;
    f.mapping = std::move(*mapped);
    return f;
}

Result<FileInfo> inspect(const File& file) {
    auto span = file.mapping.span();
    Format fmt = detect_format(span);
    if (fmt == Format::Unknown) {
        // try registry? fallback
    }

    std::shared_ptr<IFormatParser> parser;
    switch (fmt) {
        case Format::PE: parser = std::make_shared<pe::PeParser>(); break;
        case Format::ELF: parser = std::make_shared<elf::ElfParser>(); break;
        case Format::Deb: parser = std::make_shared<deb::DebParser>(); break;
        case Format::AppImage: parser = std::make_shared<appimage::AppImageParser>(); break;
        default: return Error{ErrorCode::UnsupportedFormat, "unknown format"};
    }
    auto res = parser->parse(span, file.path);
    if (!res) return res.error();
    FileInfo info = *res;
    if (info.path.empty()) info.path = file.path;
    if (info.format == Format::Unknown) info.format = fmt;
    return info;
}

} // namespace lingua

// Free function import_app in lingua namespace (declared? not in header sketch but add)
namespace lingua {
Result<importing::ImportResult> import_app(const File& file, const importing::ImportOptions& opts) {
    auto ins = inspect(file);
    if (!ins) return ins.error();
    return importing::import_app(*ins, file.path, opts);
}
}
