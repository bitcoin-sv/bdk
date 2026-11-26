#pragma once

// Splitting and unescaping the lines of proc/<pid>/mountinfo.

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

#include <tl/expected.hpp>

namespace bsv::detail
{
    // Why a mountinfo line or path was rejected.
    enum class MountinfoError : uint8_t
    {
        MalformedLine, // a field missing or out of place
        InvalidEscape, // a bad \ooo escape in a path
    };

    std::ostream& operator<<(std::ostream&, MountinfoError);

    // Parts of one line of proc_pid_mountinfo(5), as they appear in the file.
    struct MountFields
    {
        std::string_view root;       // escaped, see UnescapeMountPath
        std::string_view mountPoint; // escaped, see UnescapeMountPath
        std::string_view fsType;
        std::string_view rest; // source and super options

        bool operator==(const MountFields&) const = default;
    };

    std::ostream& operator<<(std::ostream&, const MountFields&);

    // `line` is a line of proc_pid_mountinfo(5), without its '\n'. MalformedLine
    // for a missing field, a root not starting with '/', or no " - " separator;
    // what follows the type is not checked.
    tl::expected<MountFields, MountinfoError> SplitMountLine(std::string_view line);

    // `path` with its \ooo escapes decoded; the kernel escapes space, tab,
    // newline and backslash. InvalidEscape for a backslash not followed by three
    // octal digits of at most \377.
    tl::expected<std::string, MountinfoError> UnescapeMountPath(std::string_view path);
}
