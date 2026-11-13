#include <lingua/core/mapped_file.hpp>
#include <lingua/core/error.hpp>

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lingua {

MappedFile::~MappedFile() {
    if (data_ && handle_) {
#if defined(_WIN32)
        UnmapViewOfFile(data_);
        CloseHandle(static_cast<HANDLE>(handle_));
#else
        munmap(const_cast<void*>(static_cast<const void*>(data_)), size_);
        close(reinterpret_cast<intptr_t>(handle_) - 1); // placeholder
#endif
    }
    if (owned_) {
        // owned buffer lifetime managed by shared_ptr
    }
}

MappedFile::MappedFile(MappedFile&& other) noexcept
    : data_(other.data_), size_(other.size_), handle_(other.handle_), owned_(std::move(other.owned_)) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.handle_ = nullptr;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        this->~MappedFile();
        data_ = other.data_;
        size_ = other.size_;
        handle_ = other.handle_;
        owned_ = std::move(other.owned_);
        other.data_ = nullptr;
        other.size_ = 0;
        other.handle_ = nullptr;
    }
    return *this;
}

Result<MappedFile> MappedFile::open(const std::filesystem::path& path) {
    MappedFile m;
#if defined(_WIN32)
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return Error{ErrorCode::IoError, "cannot open file", path.string()};
    }
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(file, &sz)) {
        CloseHandle(file);
        return Error{ErrorCode::IoError, "cannot get file size"};
    }
    if (sz.QuadPart < 0) {
        CloseHandle(file);
        return Error{ErrorCode::InvalidArgument, "negative file size"};
    }
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping) {
        CloseHandle(file);
        return Error{ErrorCode::IoError, "cannot create file mapping"};
    }
    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(file);
    if (!view) {
        CloseHandle(mapping);
        return Error{ErrorCode::IoError, "cannot map view"};
    }
    m.data_ = static_cast<const std::uint8_t*>(view);
    m.size_ = static_cast<std::size_t>(sz.QuadPart);
    m.handle_ = mapping;
#else
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return Error{ErrorCode::IoError, "cannot open file", path.string()};
    struct stat st{};
    if (fstat(fd, &st) != 0) { close(fd); return Error{ErrorCode::IoError, "fstat failed"}; }
    if (st.st_size == 0) {
        close(fd);
        m.data_ = nullptr;
        m.size_ = 0;
        m.handle_ = nullptr;
        return m;
    }
    void* view = mmap(nullptr, static_cast<std::size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
    if (view == MAP_FAILED) { close(fd); return Error{ErrorCode::IoError, "mmap failed"}; }
    m.data_ = static_cast<const std::uint8_t*>(view);
    m.size_ = static_cast<std::size_t>(st.st_size);
    m.handle_ = reinterpret_cast<void*>(static_cast<intptr_t>(fd + 1));
    // keep fd open? We stored fd+1; need close later. Store fd separately? We'll close in destructor via handle_-1.
#endif
    return m;
}

Result<MappedFile> MappedFile::from_buffer(std::vector<std::uint8_t> data) {
    MappedFile m;
    auto shared = std::make_shared<std::vector<std::uint8_t>>(std::move(data));
    m.data_ = shared->empty() ? nullptr : shared->data();
    m.size_ = shared->size();
    m.owned_ = shared;
    m.handle_ = nullptr;
    return m;
}

} // namespace lingua