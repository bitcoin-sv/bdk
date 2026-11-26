#pragma once

// Reading a file by path, with a failure as its errno, as std::errc, so each
// caller decides what it means.

#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>

#include <tl/expected.hpp>

namespace bsv::detail
{
    tl::expected<std::string, std::errc> ReadFile(const std::filesystem::path&);

    // file_too_large when the file is `limit` bytes or more.
    tl::expected<std::string, std::errc> ReadSmallFile(const std::filesystem::path&,
                                                       size_t limit);
}
