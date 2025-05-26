/// Define test module name with debug postfix
/// Use it as an example how to add a test module
#ifdef NDEBUG
#define BOOST_TEST_MODULE test_script_error
#else
#define BOOST_TEST_MODULE test_script_errord
#endif

#include "script/script_error.h"

#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(test_script_error_suite)

BOOST_AUTO_TEST_CASE(script_error_count)
{
	// We make sure that bsv hasn't introduced a new error in enum list
	// If that happens, this test failure notifies this
	BOOST_TEST(SCRIPT_ERR_ERROR_COUNT==47);
}

BOOST_AUTO_TEST_SUITE_END()