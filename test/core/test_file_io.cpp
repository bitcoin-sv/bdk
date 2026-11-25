#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_file_io
#else
#define BOOST_TEST_MODULE test_file_iod
#endif

#include <boost/test/unit_test.hpp>

#include "temp_directory.h"

#include <detail/file_io.hpp>

#include <tl/expected.hpp>

static std::error_code ToErrorCode(const std::errc error)
{
    return std::make_error_code(error);
}

static std::error_code ToErrorCode(const tl::expected<std::string, std::errc>& result)
{
    return result ? std::error_code{} : ToErrorCode(result.error());
}

BOOST_AUTO_TEST_SUITE(file_io, *boost::unit_test::label("integration"))

// ReadFile reads past its first chunk to the end, and fails with open()'s or
// read()'s error.
BOOST_AUTO_TEST_CASE(read_file_reads_to_the_end)
{
    using namespace bsv::detail;
    using namespace bsv::test;
    using enum std::errc;

    const TempDirectory dir;
    const std::string large(10'000, 'x');
    WriteFile(dir.Path() / "large", large);
    BOOST_CHECK(large == ReadFile(dir.Path() / "large").value_or(""));
    WriteFile(dir.Path() / "empty", "");
    BOOST_CHECK_EQUAL("", ReadFile(dir.Path() / "empty").value_or("error"));

    BOOST_CHECK_EQUAL(ToErrorCode(no_such_file_or_directory),
                      ToErrorCode(ReadFile(dir.Path() / "missing")));
    std::filesystem::create_symlink("loop", dir.Path() / "loop");
    BOOST_CHECK_EQUAL(ToErrorCode(too_many_symbolic_link_levels),
                      ToErrorCode(ReadFile(dir.Path() / "loop")));
    // A directory opens, and its read fails.
    BOOST_CHECK_EQUAL(ToErrorCode(is_a_directory), ToErrorCode(ReadFile(dir.Path())));
}

// ReadSmallFile reads a file under its limit, and fails with file_too_large for
// one of the limit or more.
BOOST_AUTO_TEST_CASE(read_small_file_rejects_a_file_of_its_limit)
{
    using namespace bsv::detail;
    using namespace bsv::test;
    using enum std::errc;

    constexpr size_t limit = 8;
    const TempDirectory dir;
    WriteFile(dir.Path() / "under", "1234567");
    WriteFile(dir.Path() / "empty", "");
    WriteFile(dir.Path() / "at", "12345678");
    WriteFile(dir.Path() / "over", "123456789");
    BOOST_CHECK_EQUAL("1234567", ReadSmallFile(dir.Path() / "under", limit).value_or(""));
    BOOST_CHECK_EQUAL("", ReadSmallFile(dir.Path() / "empty", limit).value_or("error"));
    BOOST_CHECK_EQUAL(ToErrorCode(file_too_large),
                      ToErrorCode(ReadSmallFile(dir.Path() / "at", limit)));
    BOOST_CHECK_EQUAL(ToErrorCode(file_too_large),
                      ToErrorCode(ReadSmallFile(dir.Path() / "over", limit)));

    BOOST_CHECK_EQUAL(ToErrorCode(no_such_file_or_directory),
                      ToErrorCode(ReadSmallFile(dir.Path() / "missing", limit)));
    BOOST_CHECK_EQUAL(ToErrorCode(is_a_directory),
                      ToErrorCode(ReadSmallFile(dir.Path(), limit)));
}

BOOST_AUTO_TEST_SUITE_END()
