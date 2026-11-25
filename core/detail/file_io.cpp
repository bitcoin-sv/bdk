#include <detail/file_io.hpp>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>

#include <cfile_util.h>
#include <tl/expected.hpp>

static tl::expected<UniqueFileDescriptor, std::errc> OpenReadOnly(
    const std::filesystem::path& path)
{
    const int fd{open(path.c_str(), O_RDONLY | O_CLOEXEC)};
    if(fd < 0)
        return tl::unexpected{static_cast<std::errc>(errno)};

    return UniqueFileDescriptor{fd};
}

static tl::expected<std::string, std::errc> ReadUpTo(const UniqueFileDescriptor& file,
                                                     const size_t limit)
{
    constexpr size_t CHUNK_SIZE{4'096};
    std::string content;
    while(content.size() < limit)
    {
        const size_t size{content.size()};
        const size_t chunk{std::min(CHUNK_SIZE, limit - size)};
        content.resize(size + chunk);
        const ssize_t bytes{read(file.Get(), content.data() + size, chunk)};
        if(bytes < 0)
            return tl::unexpected{static_cast<std::errc>(errno)};

        content.resize(size + static_cast<size_t>(bytes));
        if(bytes == 0)
            break;
    }
    return content;
}

tl::expected<std::string, std::errc> bsv::detail::ReadFile(
    const std::filesystem::path& path)
{
    return OpenReadOnly(path).and_then(
        [](const UniqueFileDescriptor& file)
        { return ReadUpTo(file, std::numeric_limits<size_t>::max()); });
}

tl::expected<std::string, std::errc> bsv::detail::ReadSmallFile(
    const std::filesystem::path& path,
    const size_t limit)
{
    return OpenReadOnly(path)
        .and_then([limit](const UniqueFileDescriptor& file)
                  { return ReadUpTo(file, limit); })
        .and_then(
            [limit](const std::string& content) -> tl::expected<std::string, std::errc>
            {
                if(content.size() == limit)
                    return tl::unexpected{std::errc::file_too_large};

                return content;
            });
}
