#pragma once

#include <lingua/core/error.hpp>
#include <lingua/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace lingua {

/// A read-only memory mapping of a file. Platform-specific implementation lives
/// in the core library.
class MappedFile {
public:
    MappedFile() = default;
    ~MappedFile();
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;

    [[nodiscard]] static Result<MappedFile> open(const std::filesystem::path& path);
    [[nodiscard]] static Result<MappedFile> from_buffer(std::vector<std::uint8_t> data);

    [[nodiscard]] const std::uint8_t* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::span<const std::uint8_t> span() const noexcept { return {data_, size_}; }
    [[nodiscard]] bool valid() const noexcept { return data_ != nullptr || size_ == 0; }

private:
    friend class MappedFileImpl;
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    void* handle_ = nullptr; // platform handle
    std::shared_ptr<std::vector<std::uint8_t>> owned_; // for from_buffer
};

struct File {
    std::filesystem::path path;
    MappedFile mapping;
};

} // namespace lingua