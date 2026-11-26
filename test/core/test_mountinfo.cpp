#include <ostream>
#include <string>
#include <utility>
#include <vector>

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_mountinfo
#else
#define BOOST_TEST_MODULE test_mountinfod
#endif

#include <boost/test/unit_test.hpp>

#include "expected_printer.h"

#include <detail/mountinfo.hpp>

#include <tl/expected.hpp>

// Lines of proc_pid_mountinfo(5), well-formed and not. Both functions take
// strings, so their cases are tables.
BOOST_AUTO_TEST_SUITE(mountinfo)

// Each line and the fields SplitMountLine finds in it.
BOOST_AUTO_TEST_CASE(split_mount_line)
{
    using namespace bsv::detail;
    using enum MountinfoError;

    const std::vector<std::pair<std::string, tl::expected<MountFields, MountinfoError>>>
        cases{
            {"30 23 0:26 / /sys/fs/cgroup rw,nosuid,nodev,noexec,relatime shared:4 - "
             "cgroup2 cgroup2 rw,nsdelegate",
             MountFields{"/", "/sys/fs/cgroup", "cgroup2", "cgroup2 rw,nsdelegate"}},
            // No optional fields, and several.
            {"22 1 259:1 / / rw,relatime - ext4 /dev/nvme0n1p1 rw",
             MountFields{"/", "/", "ext4", "/dev/nvme0n1p1 rw"}},
            {"41 30 0:36 /pod1 /sys/fs/cgroup/cpu rw shared:4 master:17 - cgroup cgroup "
             "rw,cpu",
             MountFields{"/pod1", "/sys/fs/cgroup/cpu", "cgroup", "cgroup rw,cpu"}},
            // A '-' that is not a whole field is an option, not the separator.
            {"30 23 0:26 / /m rw,a-b - cgroup2 cgroup2 rw",
             MountFields{"/", "/m", "cgroup2", "cgroup2 rw"}},
            // Paths are left escaped.
            {"30 23 0:26 /a\\040b /m\\040n rw - cgroup2 cgroup2 rw",
             MountFields{"/a\\040b", "/m\\040n", "cgroup2", "cgroup2 rw"}},
            // What follows the type is not checked.
            {"30 23 0:26 / /m rw - cgroup2 ", MountFields{"/", "/m", "cgroup2", ""}},
            {"30 23 0:26", tl::unexpected{MalformedLine}},
            {"", tl::unexpected{MalformedLine}},
            // A root not starting with '/', or empty.
            {"30 23 0:26 x /m rw - cgroup2 cgroup2 rw", tl::unexpected{MalformedLine}},
            {"30 23 0:26  /m rw - cgroup2 cgroup2 rw", tl::unexpected{MalformedLine}},
            // No " - " separator, or nothing after it.
            {"30 23 0:26 / /m rw shared:4 cgroup2 cgroup2 rw",
             tl::unexpected{MalformedLine}},
            {"30 23 0:26 / /m rw -", tl::unexpected{MalformedLine}},
            {"30 23 0:26 / /m rw - ", tl::unexpected{MalformedLine}},
            // A type without the space after it.
            {"30 23 0:26 / /m rw - cgroup2", tl::unexpected{MalformedLine}},
    };
    for(const auto& [line, expected] : cases)
    {
        BOOST_TEST_CONTEXT("line '" << line << "'")
        {
            BOOST_CHECK_EQUAL(expected, SplitMountLine(line));
        }
    }
}

// Each path and what UnescapeMountPath decodes it to.
BOOST_AUTO_TEST_CASE(unescape_mount_path)
{
    using namespace bsv::detail;
    using enum MountinfoError;

    const std::vector<std::pair<std::string, tl::expected<std::string, MountinfoError>>>
        cases{
            {"/sys/fs/cgroup", "/sys/fs/cgroup"},
            {"", ""},
            // The four bytes the kernel escapes: space, tab, newline, backslash.
            {"/a\\040b", "/a b"},
            {"/a\\011b", "/a\tb"},
            {"/a\\012b", "/a\nb"},
            {"/a\\134b", "/a\\b"},
            {"/m\\377", "/m\xff"},
            // Octal digits only, three of them, at most \377.
            {"/m\\089", tl::unexpected{InvalidEscape}},
            {"/m\\400", tl::unexpected{InvalidEscape}},
            {"/m\\04", tl::unexpected{InvalidEscape}},
            {"/m\\", tl::unexpected{InvalidEscape}},
    };
    for(const auto& [path, expected] : cases)
    {
        BOOST_TEST_CONTEXT("path '" << path << "'")
        {
            BOOST_CHECK_EQUAL(expected, UnescapeMountPath(path));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
