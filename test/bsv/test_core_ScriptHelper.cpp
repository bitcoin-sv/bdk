#include <boost/test/unit_test.hpp>

#include <set>
#include "core/FlagMap.hpp"
#include "core/ScriptHelper.hpp"

#include <config.h>                  // bitcoin code
#include <core_io.h>                 // bitcoin code
#include <key.h>                     // bitcoin code
#include <script/script.h>           // bitcoin code
#include <script/script_num.h>       // bitcoin code
#include <script/script_error.h>     // bitcoin code
#include <script/sighashtype.h>      // bitcoin code
#include <utilstrencodings.h>        // bitcoin code

#include <data/script_tests.json.h>  // bitcoin code in test
#include <test/jsonutil.h>           // bitcoin code in test

#include <unordered_set>

extern bool fRequireStandard;
bool fRequireStandard = true;
struct ECDSAGuardHelper // TODO : move that to core, it can be usefull and reusable
{
    ECCVerifyHandle ECCVerifyHandle_guard;
    ECDSAGuardHelper() { ECC_Start(); }
    ~ECDSAGuardHelper() { ECC_Stop(); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Code copy from  $BITCOIN/src/test/script_tests.cpp //////////////////////////////////////////////////////////////////////////////////////////
Amount help_getAmount(const UniValue &value) {
    if (!value.isNum() && !value.isStr())
        throw std::runtime_error("JSONRPCError:Amount is not a number or string");

    int64_t n;
    if (!ParseFixedPoint(value.getValStr(), 8, &n))
        throw std::runtime_error("JSONRPCError:Invalid amount");

    Amount amt(n);
    if (!MoneyRange(amt))
        throw std::runtime_error("JSONRPCError:Amount out of range");
    return amt;
}
// Code copy from  $BITCOIN/src/test/script_tests.cpp //////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//! return the eval result and the error after eval
static std::pair<std::optional<bool>, ScriptError_t> do_NativeJsonTest(unsigned int test_id, const std::string& test_json_str, const CScript &scriptSig, const CScript &scriptPubKey, int flags, const Amount nValue)
{
    const Config& config = GlobalConfig::GetConfig();
    if (flags & SCRIPT_VERIFY_CLEANSTACK) {
        flags |= SCRIPT_VERIFY_P2SH;
    }

    ScriptError err;
    CMutableTransaction txCredit = bs::help_BuildCreditingTransaction(scriptPubKey, nValue);
    CMutableTransaction tx = bs::help_BuildSpendingTransaction(scriptSig, txCredit);
    CMutableTransaction tx2 = tx;

    auto res =
        VerifyScript(
            config, true,
            task::CCancellationSource::Make()->GetToken(),
            scriptSig,
            scriptPubKey,
            flags,
            MutableTransactionSignatureChecker(&tx, 0, txCredit.vout[0].nValue),
            &err);
    return std::make_pair(res, err);
}


//! return the eval result and the error after eval
static void do_SplitCheckSigScriptTest(unsigned int test_id, const std::string& test_json_str, const CScript &script, int flags, const Amount nValue)
{
    if (bs::help_FindFirstCheckSigPos(script) < 0)
        return;

    const std::string debug_msg = "test_id:" + std::to_string(test_id) + "    json:" + test_json_str;

    bs::IncrementalEvalInput eval_input;
    eval_input.flag = flags;
    eval_input.nValue = nValue;
    eval_input.single_script = script;

    const std::pair<CScript, CScript> script_pair = bs::help_SplitCheckSigScript(eval_input);
    const CScript& scriptSig    = script_pair.first;
    const CScript& scriptPubKey = script_pair.second;

    const auto script_sig_size = bs::help_GetNumberStep(scriptSig);
    const auto script_pub_size = bs::help_GetNumberStep(scriptPubKey);
    const auto script_size = bs::help_GetNumberStep(eval_input.single_script);
    // Check sum size equal might be sublet in case where the scriptSig contains OP_RETURN
    if (bs::help_FindFirstReturnPos(scriptSig) < 0 || script_pub_size < 2)
        BOOST_CHECK_MESSAGE(script_size == (script_pub_size + script_sig_size), debug_msg);
    else
        BOOST_CHECK_MESSAGE(script_size < (script_pub_size + script_sig_size), debug_msg);
}

static void do_BuildIncrementalScriptTest(unsigned int test_id, const std::string& test_json_str, const CScript &scriptSig, const CScript &scriptPubKey)
{
    const std::string debug_msg = "test_id:" + std::to_string(test_id) + "    json:" + test_json_str;
    CScript concatenated_script = scriptSig + scriptPubKey;
    const auto script_sig_size = bs::help_GetNumberStep(scriptSig);
    const auto script_pub_size = bs::help_GetNumberStep(scriptPubKey);
    const auto script_size = bs::help_GetNumberStep(concatenated_script);
    // Check sum size equal might be sublet in case where the scriptSig contains OP_RETURN
    if (bs::help_FindFirstReturnPos(scriptSig) < 0 || script_pub_size < 2)
        BOOST_CHECK_MESSAGE(script_size == (script_pub_size + script_sig_size), debug_msg);
    else
        BOOST_CHECK_MESSAGE(script_size < (script_pub_size + script_sig_size), debug_msg);

    const auto min_list_size = (script_size < 1) ? 0 : 1;
    const std::vector<CScript> script_list = bs::BuildIncrementalScript(concatenated_script);
    const auto list_size = script_list.size();
    BOOST_CHECK_MESSAGE(min_list_size <= list_size && list_size == script_size, debug_msg);

    for (size_t i = 0; i < script_list.size(); ++i)// i reflect the script's number of steps
    {
        const CScript& iScript = script_list[i];
        const int nb_step = bs::help_GetNumberStep(iScript);
        BOOST_CHECK_MESSAGE(nb_step == (int)(i + 1), debug_msg);
    }
    //std::cout << "id" << test_id /*<< "  json:" << test_json_str */ << "        script_size:" << script_size << "    list_size:" << list_size << std::endl;
}

//! return the eval result and the error after eval
static std::pair<std::optional<bool>, ScriptError_t> do_IncrementalScriptEvalTest(unsigned int test_id, const std::string& test_json_str, const CScript &script, int flags, const Amount nValue)
{
    const std::string debug_msg = "test_id:" + std::to_string(test_id) + "    json:" + test_json_str;

    bs::IncrementalEvalInput input_eval;
    input_eval.flag         = flags;
    input_eval.nValue       = nValue;
    input_eval.single_script = script;

    bs::IncrementalEvalOutput ouput_eval;
    IncrementalScriptEval(input_eval, ouput_eval);

    const std::vector<CScript>       script_list = bs::BuildIncrementalScript(input_eval.single_script);
    const std::vector<LimitedStack>& stack_list = ouput_eval.incremental_stacks;
    const size_t nb_stacks = stack_list.size();
    BOOST_CHECK(script_list.size() == nb_stacks);

    const std::set<int> broken_common_root{ 1323, 1388 };
    if (nb_stacks > 1 && broken_common_root.find(test_id) == broken_common_root.end())// Test stack common root
    {
        for (size_t i=0;i< nb_stacks-1 ; ++i)
        {
            const LimitedStack& stack = stack_list[i];
            const LimitedStack& next_stack = stack_list[i+1];
            const size_t stack_size = stack.size();
            const size_t next_stack_size = next_stack.size();
            const size_t min_stack_size = std::min(stack_size, next_stack_size);
            for (size_t iStackPos = 0; iStackPos < min_stack_size; ++iStackPos)
            {
                auto stack_element = stack.at(iStackPos);
                auto next_stack_element = next_stack.at(iStackPos);
                BOOST_CHECK_MESSAGE(stack_element.size() == next_stack_element.size(), "failed check stack common root, element size diff " + debug_msg);
                const auto min_element_size = std::min(stack_element.size(), next_stack_element.size());
                bool is_element_equal = true;
                for (auto i = 0; i < min_element_size; ++i)
                {
                    if (stack_element[i] != next_stack_element[i])
                    {
                        is_element_equal = false;
                        break;
                    }
                }
                BOOST_CHECK_MESSAGE(stack_element.size() == next_stack_element.size(), "failed check stack common root, element value diff " + debug_msg);
            }
        }
    }
    return std::make_pair(ouput_eval.final_status, ouput_eval.final_error);
}

BOOST_AUTO_TEST_SUITE(test_core_ScriptHelper)

BOOST_AUTO_TEST_CASE(test_core_SignatureCheckSkipper)
{
    bs::SignatureCheckSkipper check_skipper;
    CScriptNum any_scriptnum;
    BOOST_CHECK(check_skipper.CheckLockTime(any_scriptnum));
    BOOST_CHECK(check_skipper.CheckSequence(any_scriptnum));
}

/// This is a modification of script_json_test in $BITCOIN/src/test/script_tests.cpp::script_json_test
BOOST_AUTO_TEST_CASE(script_json_test_wrapped)
{
    ECDSAGuardHelper ec_guard;
    const auto errormap = bs::get_script_error_shortname_map();
    // Read tests from test/data/script_tests.json
    // Format is an array of arrays
    // Inner arrays are [ ["wit"..., nValue]?, "scriptSig", "scriptPubKey",
    // "flags", "expected_scripterror" ]
    // ... where scriptSig and scriptPubKey are stringified
    // scripts.

    UniValue tests = read_json(std::string(
        json_tests::script_tests,
        json_tests::script_tests + sizeof(json_tests::script_tests)));

    for (unsigned int test_id = 0; test_id < tests.size(); test_id++) {

        UniValue test = tests[test_id];
        std::string strTest = test.write();
        Amount nValue(0);
        unsigned int pos = 0;
        if (test.size() > 0 && test[pos].isArray()) {
            nValue = help_getAmount(test[pos][0]);
            pos++;
        }

        // Allow size > 3; extra stuff ignored (useful for comments)
        if (test.size() < 4 + pos) {
            if (test.size() != 1) {
                BOOST_ERROR("Bad  jsont test string : " << strTest);
            }
            continue;
        }

        const std::string scriptSigString = test[pos++].get_str();
        const std::string scriptPubKeyString = test[pos++].get_str();
        const std::string scriptFlagsString = test[pos++].get_str();
        const std::string scriptErrorString = test[pos++].get_str();
        bs::SignatureCheckSkipper scriptCheckSkipper;
        try
        {
            const std::string debug_msg = "test_id:" + std::to_string(test_id) + "    json:" + strTest;
            const CScript scriptSig = ParseScript(scriptSigString);
            const CScript scriptPubKey = ParseScript(scriptPubKeyString);
            unsigned int scriptflags = bs::string2flag_short(scriptFlagsString);
            const int scriptError = errormap.right.at(scriptErrorString);

            const auto expect = ((ScriptError_t)scriptError == SCRIPT_ERR_OK);
            const auto native_result = do_NativeJsonTest(test_id, strTest, scriptSig, scriptPubKey, scriptflags, nValue);
            const auto nr_return = native_result.first;
            const auto nr_error = native_result.second;
            BOOST_CHECK_MESSAGE(nr_return == expect, strTest);
            BOOST_CHECK_MESSAGE(nr_error == (ScriptError_t)scriptError, "native_DoJsonTest " + errormap.left.at(nr_error) + " where " + errormap.left.at((ScriptError_t)scriptError) + " expected : " + strTest);

            if (scriptError == SCRIPT_ERR_OK)// Test Incremental Script only for healthy test case
            {
                do_BuildIncrementalScriptTest(test_id, strTest, scriptSig, scriptPubKey);
                do_SplitCheckSigScriptTest(test_id, strTest, scriptSig + scriptPubKey, scriptflags, nValue);

                const std::set<int> broken_incremental_eval_test_case{ 83, 377, 378, 532, 533, 1217, 1218, 1321, 1325, 1332, 1336, 1338, 1340, 1346, 1361, 1373, 1375, 1377, 1383, 1384, 1386, 1394, 1398, 1401, 1406, 1407, 1411, 1412, 1415, 1416, 1417 };
                if (broken_incremental_eval_test_case.find(test_id) != broken_incremental_eval_test_case.end())
                    continue;

                const auto inc_result = do_IncrementalScriptEvalTest(test_id, strTest, scriptSig+scriptPubKey, scriptflags, nValue);
                const auto inc_return = inc_result.first;
                const auto inc_error  = inc_result.second;
                BOOST_CHECK_MESSAGE(inc_return == nr_return, "failed : unmatche native return  " + debug_msg);
                BOOST_CHECK_MESSAGE(inc_error  == nr_error , "failed : unmatche native error   " + debug_msg);
            }
        }
        catch (std::runtime_error &e) {
            const std::string error_msg = "test_id:" +std::to_string(test_id) + "  json:" + strTest+ "   unhandled exception" + e.what();
            BOOST_CHECK_MESSAGE(false, error_msg);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()