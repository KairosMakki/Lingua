#include <lingua/lingua.hpp>

#include <lingua/adapters/adapter.hpp>
#include <lingua/core/error.hpp>
#include <lingua/core/format_parser.hpp>
#include <lingua/core/hash.hpp>
#include <lingua/core/mapped_file.hpp>
#include <lingua/detect/detector.hpp>
#include <lingua/format/appimage/appimage_parser.hpp>
#include <lingua/format/apk/apk_parser.hpp>
#include <lingua/format/flatpak/flatpak_parser.hpp>
#include <lingua/format/msi/msi_parser.hpp>
#include <lingua/format/snap/snap_parser.hpp>
#include <lingua/format/deb/deb_parser.hpp>
#include <lingua/format/elf/elf_parser.hpp>
#include <lingua/format/macho/macho_parser.hpp>
#include <lingua/format/pe/pe_parser.hpp>
#include <lingua/format/rpm/rpm_parser.hpp>
#include <lingua/import/import.hpp>

#include <filesystem>
#include <memory>
#include <span>

namespace lingua {

namespace {
void ensure_registry() {
    auto& reg = FormatRegistry::instance();
    if (!reg.parsers().empty()) return;
    reg.register_parser(std::make_shared<pe::PeParser>());
    reg.register_parser(std::make_shared<appimage::AppImageParser>());
    reg.register_parser(std::make_shared<deb::DebParser>());
    reg.register_parser(std::make_shared<apk::ApkParser>());
    reg.register_parser(std::make_shared<msi::MsiParser>());
    reg.register_parser(std::make_shared<flatpak::FlatpakParser>());
    reg.register_parser(std::make_shared<snap::SnapParser>());
    reg.register_parser(std::make_shared<macho::MachOParser>());
    reg.register_parser(std::make_shared<rpm::RpmParser>());
    reg.register_parser(std::make_shared<elf::ElfParser>());
    reg.register_parser(std::make_shared<macho::MachOParser>());
    reg.register_parser(std::make_shared<rpm::RpmParser>());
    reg.register_parser(std::make_shared<apk::ApkParser>());
}
}

Result<File> open(const std::filesystem::path& path) {
    auto mapped = MappedFile::open(path);
    if (!mapped) return mapped.error();
    File f;
    f.path = path;
    f.mapping = std::move(*mapped);
    return f;
}

Result<FileInfo> inspect(const File& file) {
    ensure_registry();
    auto span = file.mapping.span();
    auto& reg = FormatRegistry::instance();
    auto parser = reg.detect(span);
    if (!parser) {
        Format fmt = detect_format(span);
        if (fmt == Format::Unknown) return Error{ErrorCode::UnsupportedFormat, "unknown format"};
        // fallback: construct by format
        switch (fmt) {
            case Format::PE: parser = std::make_shared<pe::PeParser>(); break;
            case Format::ELF: parser = std::make_shared<elf::ElfParser>(); break;
            case Format::Deb: parser = std::make_shared<deb::DebParser>(); break;
            case Format::AppImage: parser = std::make_shared<appimage::AppImageParser>(); break;
            case Format::MachO: parser = std::make_shared<macho::MachOParser>(); break;
            case Format::Rpm: parser = std::make_shared<rpm::RpmParser>(); break;
            case Format::Apk: parser = std::make_shared<apk::ApkParser>(); break;
            case Format::Msi: parser = std::make_shared<msi::MsiParser>(); break;
            case Format::Flatpak: parser = std::make_shared<flatpak::FlatpakParser>(); break;
            case Format::Snap: parser = std::make_shared<snap::SnapParser>(); break;
            default: return Error{ErrorCode::UnsupportedFormat, "unsupported format"};
        }
    }
    auto res = parser->parse(span, file.path);
    if (!res) return res.error();
    FileInfo info = *res;
    info.sha256 = sha256(span);
    if (info.path.empty()) info.path = file.path;
    if (info.format == Format::Unknown) info.format = parser->format();
    return info;
}

} // namespace lingua

namespace lingua {
Result<importing::ImportResult> import_app(const File& file, const importing::ImportOptions& opts) {
    auto ins = inspect(file);
    if (!ins) return ins.error();
    return importing::import_app(*ins, file.path, opts);
}
}