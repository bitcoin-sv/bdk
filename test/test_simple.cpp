/// Define test module name with debug postfix
/// Use it as an example how to add a test module
#ifdef NDEBUG
#define BOOST_TEST_MODULE test_simple
#else
#define BOOST_TEST_MODULE test_simpled
#endif

#include <boost/test/unit_test.hpp>

#include <config.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/script_error.h>
#include <taskcancellation.h>

BOOST_AUTO_TEST_SUITE(test_simple_suite)

BOOST_AUTO_TEST_CASE(test_case_1)
{
    static const uint8_t direct[] = {1, 0x5a};
    // static const uint8_t direct[] = {0x81, 0x82, OP_ADD, 0x83, OP_EQUAL, OP_CHECK};
    CScript scr;
    ScriptError err;
    LimitedStack direct_stack(UINT32_MAX);
    auto source = task::CCancellationSource::Make();
    const GlobalConfig& testConfig = GlobalConfig::GetConfig();

    auto res = EvalScript(testConfig, true, source->GetToken(), direct_stack,
                          CScript(&direct[0], &direct[sizeof(direct)]), SCRIPT_VERIFY_P2SH,
                          BaseSignatureChecker(), &err);

    BOOST_TEST(res.value());
}

BOOST_AUTO_TEST_SUITE_END()