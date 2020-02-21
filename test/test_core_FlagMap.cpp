

/// Define test module name with debug postfix
#ifdef NDEBUG 
#  define BOOST_TEST_MODULE test_core_extra
#else
#  define BOOST_TEST_MODULE test_core_extrad
#endif

#include <boost/test/unit_test.hpp>

#include <core/FlagMap.hpp>

#include <config.h>                  // bitcoin code
#include <core_io.h>                 // bitcoin code
#include <script/script.h>           // bitcoin code

#include <data/script_tests.json.h>  // bitcoin code
#include <test/jsonutil.h>           // bitcoin code

BOOST_AUTO_TEST_SUITE(test_core_FlagMap)

BOOST_AUTO_TEST_CASE(test_opcode_name_map)
{
    const boost::bimap< opcodetype, std::string > map = bs::get_opcode_name_map();
    for (int op = 0; op < FIRST_UNDEFINED_OP_VALUE; op++)
    {
        if (op < OP_PUSHDATA1) {
            continue;
        }

        const char *name = GetOpName((opcodetype)op);
        if (strcmp(name, "OP_UNKNOWN") == 0) {
            continue;
        }

        const std::string bitcoin_op_name_str(name);
        BOOST_CHECK(map.right.find(bitcoin_op_name_str) != map.right.end());
        const opcodetype op_value = map.right.at(bitcoin_op_name_str);
        BOOST_CHECK(op_value == op);
    }
}

BOOST_AUTO_TEST_CASE(test_script_flag_name_map)
{
    const boost::bimap< uint32_t, std::string > map = bs::get_script_flag_name_map();
    BOOST_CHECK(map.size() == 19);
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_NONE) == "SCRIPT_VERIFY_NONE");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_P2SH) == "SCRIPT_VERIFY_P2SH");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_STRICTENC) == "SCRIPT_VERIFY_STRICTENC");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_DERSIG) == "SCRIPT_VERIFY_DERSIG");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_LOW_S) == "SCRIPT_VERIFY_LOW_S");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_NULLDUMMY) == "SCRIPT_VERIFY_NULLDUMMY");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_SIGPUSHONLY) == "SCRIPT_VERIFY_SIGPUSHONLY");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_MINIMALDATA) == "SCRIPT_VERIFY_MINIMALDATA");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS) == "SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_CLEANSTACK) == "SCRIPT_VERIFY_CLEANSTACK");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY) == "SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_CHECKSEQUENCEVERIFY) == "SCRIPT_VERIFY_CHECKSEQUENCEVERIFY");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_MINIMALIF) == "SCRIPT_VERIFY_MINIMALIF");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_NULLFAIL) == "SCRIPT_VERIFY_NULLFAIL");
    BOOST_CHECK(map.left.at(SCRIPT_VERIFY_COMPRESSED_PUBKEYTYPE) == "SCRIPT_VERIFY_COMPRESSED_PUBKEYTYPE");
    BOOST_CHECK(map.left.at(SCRIPT_ENABLE_SIGHASH_FORKID) == "SCRIPT_ENABLE_SIGHASH_FORKID");
    BOOST_CHECK(map.left.at(SCRIPT_GENESIS) == "SCRIPT_GENESIS");
    BOOST_CHECK(map.left.at(SCRIPT_UTXO_AFTER_GENESIS) == "SCRIPT_UTXO_AFTER_GENESIS");
    BOOST_CHECK(map.left.at(SCRIPT_FLAG_LAST) == "SCRIPT_FLAG_LAST");
}

BOOST_AUTO_TEST_CASE(test_script_error_name_map)
{
    const boost::bimap< ScriptError_t, std::string > map = bs::get_script_error_name_map();
    BOOST_CHECK(map.size() == 46);

    BOOST_CHECK(map.left.at(SCRIPT_ERR_OK) == "SCRIPT_ERR_OK");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_UNKNOWN_ERROR) == "SCRIPT_ERR_UNKNOWN_ERROR");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_EVAL_FALSE) == "SCRIPT_ERR_EVAL_FALSE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_OP_RETURN) == "SCRIPT_ERR_OP_RETURN");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_SCRIPT_SIZE) == "SCRIPT_ERR_SCRIPT_SIZE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_PUSH_SIZE) == "SCRIPT_ERR_PUSH_SIZE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_OP_COUNT) == "SCRIPT_ERR_OP_COUNT");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_STACK_SIZE) == "SCRIPT_ERR_STACK_SIZE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_SIG_COUNT) == "SCRIPT_ERR_SIG_COUNT");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_PUBKEY_COUNT) == "SCRIPT_ERR_PUBKEY_COUNT");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_INVALID_OPERAND_SIZE) == "SCRIPT_ERR_INVALID_OPERAND_SIZE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_INVALID_NUMBER_RANGE) == "SCRIPT_ERR_INVALID_NUMBER_RANGE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_IMPOSSIBLE_ENCODING) == "SCRIPT_ERR_IMPOSSIBLE_ENCODING");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_INVALID_SPLIT_RANGE) == "SCRIPT_ERR_INVALID_SPLIT_RANGE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_SCRIPTNUM_OVERFLOW) == "SCRIPT_ERR_SCRIPTNUM_OVERFLOW");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_SCRIPTNUM_MINENCODE) == "SCRIPT_ERR_SCRIPTNUM_MINENCODE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_VERIFY) == "SCRIPT_ERR_VERIFY");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_EQUALVERIFY) == "SCRIPT_ERR_EQUALVERIFY");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_CHECKMULTISIGVERIFY) == "SCRIPT_ERR_CHECKMULTISIGVERIFY");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_CHECKSIGVERIFY) == "SCRIPT_ERR_CHECKSIGVERIFY");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_NUMEQUALVERIFY) == "SCRIPT_ERR_NUMEQUALVERIFY");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_BAD_OPCODE) == "SCRIPT_ERR_BAD_OPCODE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_DISABLED_OPCODE) == "SCRIPT_ERR_DISABLED_OPCODE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_INVALID_STACK_OPERATION) == "SCRIPT_ERR_INVALID_STACK_OPERATION");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_INVALID_ALTSTACK_OPERATION) == "SCRIPT_ERR_INVALID_ALTSTACK_OPERATION");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_UNBALANCED_CONDITIONAL) == "SCRIPT_ERR_UNBALANCED_CONDITIONAL");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_DIV_BY_ZERO) == "SCRIPT_ERR_DIV_BY_ZERO");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_MOD_BY_ZERO) == "SCRIPT_ERR_MOD_BY_ZERO");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_NEGATIVE_LOCKTIME) == "SCRIPT_ERR_NEGATIVE_LOCKTIME");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_UNSATISFIED_LOCKTIME) == "SCRIPT_ERR_UNSATISFIED_LOCKTIME");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_SIG_HASHTYPE) == "SCRIPT_ERR_SIG_HASHTYPE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_SIG_DER) == "SCRIPT_ERR_SIG_DER");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_MINIMALDATA) == "SCRIPT_ERR_MINIMALDATA");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_SIG_PUSHONLY) == "SCRIPT_ERR_SIG_PUSHONLY");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_SIG_HIGH_S) == "SCRIPT_ERR_SIG_HIGH_S");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_SIG_NULLDUMMY) == "SCRIPT_ERR_SIG_NULLDUMMY");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_PUBKEYTYPE) == "SCRIPT_ERR_PUBKEYTYPE");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_CLEANSTACK) == "SCRIPT_ERR_CLEANSTACK");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_MINIMALIF) == "SCRIPT_ERR_MINIMALIF");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_SIG_NULLFAIL) == "SCRIPT_ERR_SIG_NULLFAIL");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS) == "SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_NONCOMPRESSED_PUBKEY) == "SCRIPT_ERR_NONCOMPRESSED_PUBKEY");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_ILLEGAL_FORKID) == "SCRIPT_ERR_ILLEGAL_FORKID");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_MUST_USE_FORKID) == "SCRIPT_ERR_MUST_USE_FORKID");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_BIG_INT) == "SCRIPT_ERR_BIG_INT");
    BOOST_CHECK(map.left.at(SCRIPT_ERR_ERROR_COUNT) == "SCRIPT_ERR_ERROR_COUNT");
}

template<typename bimap_type>
void do_test_shortname_map(const bimap_type& shortmap, const bimap_type& longmap, const std::string& truncated_str)
{
    typedef typename bimap_type::left_map::const_iterator left_iterator;
    BOOST_CHECK(shortmap.size() == longmap.size());
    for (left_iterator it = shortmap.left.begin(); it != shortmap.left.end(); ++it)
    {
        const typename bimap_type::left_key_type value = it->first;
        const typename bimap_type::right_key_type shortname = it->second;
        BOOST_CHECK(longmap.left.find(value) != longmap.left.end());
        const typename bimap_type::right_key_type longname = longmap.left.at(value);
        const typename bimap_type::right_key_type concatenated_name = truncated_str + shortname;
        if (shortname != "MISSING_FORKID") // the shit case
            BOOST_CHECK((shortname == longname) || (concatenated_name == longname) || longname.find(shortname) != std::string::npos);
    }
}
BOOST_AUTO_TEST_CASE(test_shortname_map)
{
    {
        const auto map = bs::get_opcode_name_map();
        const auto shortmap = bs::get_opcode_shortname_map();
        do_test_shortname_map(shortmap, map, "OP_");
    }

    {
        const auto map = bs::get_script_flag_name_map();
        const auto shortmap = bs::get_script_flag_shortname_map();
        do_test_shortname_map(shortmap, map, "SCRIPT_VERIFY_");
    }

    {
        const auto map = bs::get_script_error_name_map();
        const auto shortmap = bs::get_script_error_shortname_map();
        do_test_shortname_map(shortmap, map, "SCRIPT_ERR_");
    }
}

/// This code is partial from /// This is a modification of script_json_test in $BITCOIN/src/test/script_tests.cpp::script_json_test
BOOST_AUTO_TEST_CASE(test_string2flag)
{
    // Read tests from test/data/script_tests.json
    // Format is an array of arrays
    // Inner arrays are [ ["wit"..., nValue]?, "scriptSig", "scriptPubKey",
    // "flags", "expected_scripterror" ]
    // ... where scriptSig and scriptPubKey are stringified
    // scripts.
    UniValue tests = read_json(std::string(
        json_tests::script_tests,
        json_tests::script_tests + sizeof(json_tests::script_tests)));

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        UniValue test = tests[idx];
        std::string strTest = test.write();
        Amount nValue(0);
        unsigned int pos = 0;
        if (test.size() > 0 && test[pos].isArray()) {
            //nValue = AmountFromValue(test[pos][0]);
            pos++;
        }

        // Allow size > 3; extra stuff ignored (useful for comments)
        if (test.size() < 4 + pos) {
            if (test.size() != 1) {
                BOOST_ERROR("Bad jsont test string : " << strTest);
            }
            continue;
        }

        const std::string scriptSigString = test[pos++].get_str();
        const std::string scriptPubKeyString = test[pos++].get_str();
        const std::string scriptFlagsString = test[pos++].get_str();
        const std::string scriptErrorString = test[pos++].get_str();
        try
        {
            unsigned int scriptflags = bs::string2flag_short(scriptFlagsString);
        }
        catch (std::runtime_error &e) {
            const std::string error_msg = "Test string2flag failed with json " + strTest + " flags string " + scriptFlagsString + " exception " + e.what();
            BOOST_CHECK_MESSAGE(false, error_msg);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()