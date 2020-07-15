/// Define test module name with debug postfix
/// Use it as an example how to add a test module
#ifdef NDEBUG
#define BOOST_TEST_MODULE test_assembler
#else
#define BOOST_TEST_MODULE test_assemblerd
#endif

#include "assembler.h"
#include "script.h"

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace bsv;

BOOST_AUTO_TEST_SUITE(test_assembler_suite)

BOOST_AUTO_TEST_CASE(to_asm_from_cscript)
{
    const vector<uint8_t> input{0x93, 0x87};
    const std::string actual{bsv::to_asm(input)};
    const std::string expected{"ADD EQUAL"};
    BOOST_CHECK_EQUAL(expected, actual);
}

BOOST_AUTO_TEST_CASE(to_asm_from_opcode_string)
{
    const std::string input("4 5 0x93 9 0x87");
    const std::string actual{bsv::to_asm(input)};
    const std::string expected{"4 5 ADD 9 EQUAL"};
    BOOST_CHECK_EQUAL(expected, actual);
}

BOOST_AUTO_TEST_CASE(to_asm_from_asm)
{
    const std::string input("4 5 ADD 9 EQUAL");
    const CScript intermediate{bsv::from_asm(input)};
    std::cout << intermediate << '\n';
    const std::string output{bsv::to_asm(intermediate)};
    BOOST_CHECK_EQUAL(input, output);
}

BOOST_AUTO_TEST_CASE(to_asm_from_asm_fail)
{
    const std::string input("4 5 OP_ADD 9 EQUAL");
    const CScript intermediate{bsv::from_asm(input)};
    const std::string output{bsv::to_asm(intermediate)};
    BOOST_CHECK_PREDICATE(std::not_equal_to<std::string>(), (input)(output));
}

BOOST_AUTO_TEST_CASE(from_asm_check_exception)
{
    const std::string input("mary had a little lamb");
    BOOST_CHECK_THROW(bsv::from_asm(input), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
