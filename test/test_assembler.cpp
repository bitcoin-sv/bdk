/// Define test module name with debug postfix
/// Use it as an example how to add a test module
#ifdef NDEBUG
#define BOOST_TEST_MODULE test_assembler
#else
#define BOOST_TEST_MODULE test_assemblerd
#endif

#include "Assembler.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace bsv;

BOOST_AUTO_TEST_SUITE(test_assembler_suite)

BOOST_AUTO_TEST_CASE(to_asm_from_asm)
{
    std::string script("4 5 ADD 9 EQUAL");

    CScript converted_script = bsv::from_asm(script);

    std::string converted_script_string = bsv::to_asm(converted_script);

    BOOST_CHECK_EQUAL(script,converted_script_string);
}

BOOST_AUTO_TEST_CASE(to_asm_from_asm_fail)
{
    std::string script("4 5 OP_ADD 9 EQUAL");

    CScript converted_script = bsv::from_asm(script);

    std::string converted_script_string = bsv::to_asm(converted_script);

    BOOST_CHECK_PREDICATE(std::not_equal_to<std::string>(), (script)(converted_script_string));
}

BOOST_AUTO_TEST_CASE(from_asm_check_exception)
{
    std::string script("mary had a little lamb");

    BOOST_CHECK_THROW(bsv::from_asm(script), std::runtime_error);
}



BOOST_AUTO_TEST_SUITE_END()
