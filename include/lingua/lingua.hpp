#pragma once

#include <lingua/core/byte_reader.hpp>
#include <lingua/core/error.hpp>
#include <lingua/core/format_parser.hpp>
#include <lingua/core/logger.hpp>
#include <lingua/core/mapped_file.hpp>
#include <lingua/core/result.hpp>
#include <lingua/core/types.hpp>

#include <lingua/detect/detector.hpp>
#include <lingua/import/import.hpp>
#include <lingua/adapters/adapter.hpp>

namespace lingua {

using importing::ImportOptions;
using importing::ImportResult;

/// Open a file for inspection/import.
Result<File> open(const std::filesystem::path& path);

/// Inspect an already opened file.
Result<FileInfo> inspect(const File& file);

/// Import an opened file using the import pipeline.
Result<ImportResult> import_app(const File& file, const ImportOptions& opts = {});

/// Detect format from raw bytes.
Format detect_format(std::span<const std::uint8_t> data);

} // namespace lingua