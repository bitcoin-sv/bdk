#include "core/FlagMap.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace bs
{
    boost::bimap< opcodetype, std::string > get_opcode_name_map()
    {
        boost::bimap< opcodetype, std::string > map;

        for (int op = 0; op < FIRST_UNDEFINED_OP_VALUE; op++)
        {
            if (op < OP_PUSHDATA1) {
                continue;
            }

            const char *name = GetOpName((opcodetype)op);
            if (strcmp(name, "OP_UNKNOWN") == 0) {
                continue;
            }

            const std::string op_name_str(name);
            map.insert(boost::bimap< opcodetype, std::string >::value_type((opcodetype)op, op_name_str));
        }

        //// Insert other extra values ??
        //map.insert(boost::bimap< opcodetype, std::string >::value_type(OP_SMALLINTEGER, "OP_SMALLINTEGER"));
        //map.insert(boost::bimap< opcodetype, std::string >::value_type(OP_PUBKEYS, "OP_PUBKEYS"));
        //map.insert(boost::bimap< opcodetype, std::string >::value_type(OP_PUBKEYHASH, "OP_PUBKEYHASH"));
        //map.insert(boost::bimap< opcodetype, std::string >::value_type(OP_PUBKEY, "OP_PUBKEY"));
        //map.insert(boost::bimap< opcodetype, std::string >::value_type(OP_INVALIDOPCODE, "OP_INVALIDOPCODE"));
        return map;
    }
    boost::bimap< opcodetype, std::string > get_opcode_shortname_map()
    {
        typedef boost::bimap< opcodetype, std::string >::left_map::const_iterator left_iterator;
        boost::bimap< opcodetype, std::string > shortmap;
        const boost::bimap< opcodetype, std::string > map = get_opcode_name_map();
        for (left_iterator it = map.left.begin(); it != map.left.end(); ++it)
        {
            const opcodetype op_value = it->first;
            std::string op_name = it->second;
            boost::algorithm::replace_first(op_name, "OP_", "");
            shortmap.insert(boost::bimap< opcodetype, std::string >::value_type(op_value, op_name));
        }
        return shortmap;
    }

    boost::bimap< uint32_t, std::string > get_script_flag_name_map()
    {
        boost::bimap< uint32_t, std::string > map;

        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_NONE, "SCRIPT_VERIFY_NONE"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_P2SH, "SCRIPT_VERIFY_P2SH"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_STRICTENC, "SCRIPT_VERIFY_STRICTENC"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_DERSIG, "SCRIPT_VERIFY_DERSIG"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_LOW_S, "SCRIPT_VERIFY_LOW_S"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_NULLDUMMY, "SCRIPT_VERIFY_NULLDUMMY"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_SIGPUSHONLY, "SCRIPT_VERIFY_SIGPUSHONLY"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_MINIMALDATA, "SCRIPT_VERIFY_MINIMALDATA"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS, "SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_CLEANSTACK, "SCRIPT_VERIFY_CLEANSTACK"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, "SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_CHECKSEQUENCEVERIFY, "SCRIPT_VERIFY_CHECKSEQUENCEVERIFY"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_MINIMALIF, "SCRIPT_VERIFY_MINIMALIF"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_NULLFAIL, "SCRIPT_VERIFY_NULLFAIL"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_VERIFY_COMPRESSED_PUBKEYTYPE, "SCRIPT_VERIFY_COMPRESSED_PUBKEYTYPE"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_ENABLE_SIGHASH_FORKID, "SCRIPT_ENABLE_SIGHASH_FORKID"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_GENESIS, "SCRIPT_GENESIS"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_UTXO_AFTER_GENESIS, "SCRIPT_UTXO_AFTER_GENESIS"));
        map.insert(boost::bimap< uint32_t, std::string >::value_type(SCRIPT_FLAG_LAST, "SCRIPT_FLAG_LAST"));

        return map;
    }
    boost::bimap< uint32_t, std::string > get_script_flag_shortname_map()
    {
        typedef boost::bimap< uint32_t, std::string >::left_map::const_iterator left_iterator;
        boost::bimap< uint32_t, std::string > shortmap;
        const boost::bimap< uint32_t, std::string > map = get_script_flag_name_map();
        for (left_iterator it = map.left.begin(); it != map.left.end(); ++it)
        {
            const uint32_t flag_value = it->first;
            const std::string flag_fullname = it->second;
            std::string flag_name = flag_fullname;
            switch (flag_value)
            {
            case SCRIPT_ENABLE_SIGHASH_FORKID:
                flag_name = "SIGHASH_FORKID";
                break;
            case SCRIPT_GENESIS:
                flag_name = "GENESIS";
                break;
            case SCRIPT_UTXO_AFTER_GENESIS:
                flag_name = "UTXO_AFTER_GENESIS";
                break;
            default:
                boost::algorithm::replace_first(flag_name, "SCRIPT_VERIFY_", "");
            }
            shortmap.insert(boost::bimap< uint32_t, std::string >::value_type(flag_value, flag_name));
        }
        return shortmap;
    }

    boost::bimap< ScriptError_t, std::string > get_script_error_name_map()
    {        
        boost::bimap< ScriptError_t, std::string > map;

        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_OK, "SCRIPT_ERR_OK"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_UNKNOWN_ERROR, "SCRIPT_ERR_UNKNOWN_ERROR"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_EVAL_FALSE, "SCRIPT_ERR_EVAL_FALSE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_OP_RETURN, "SCRIPT_ERR_OP_RETURN"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_SCRIPT_SIZE, "SCRIPT_ERR_SCRIPT_SIZE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_PUSH_SIZE, "SCRIPT_ERR_PUSH_SIZE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_OP_COUNT, "SCRIPT_ERR_OP_COUNT"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_STACK_SIZE, "SCRIPT_ERR_STACK_SIZE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_SIG_COUNT, "SCRIPT_ERR_SIG_COUNT"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_PUBKEY_COUNT, "SCRIPT_ERR_PUBKEY_COUNT"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_INVALID_OPERAND_SIZE, "SCRIPT_ERR_INVALID_OPERAND_SIZE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_INVALID_NUMBER_RANGE, "SCRIPT_ERR_INVALID_NUMBER_RANGE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_IMPOSSIBLE_ENCODING, "SCRIPT_ERR_IMPOSSIBLE_ENCODING"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_INVALID_SPLIT_RANGE, "SCRIPT_ERR_INVALID_SPLIT_RANGE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_SCRIPTNUM_OVERFLOW, "SCRIPT_ERR_SCRIPTNUM_OVERFLOW"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_SCRIPTNUM_MINENCODE, "SCRIPT_ERR_SCRIPTNUM_MINENCODE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_VERIFY, "SCRIPT_ERR_VERIFY"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_EQUALVERIFY, "SCRIPT_ERR_EQUALVERIFY"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_CHECKMULTISIGVERIFY, "SCRIPT_ERR_CHECKMULTISIGVERIFY"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_CHECKSIGVERIFY, "SCRIPT_ERR_CHECKSIGVERIFY"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_NUMEQUALVERIFY, "SCRIPT_ERR_NUMEQUALVERIFY"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_BAD_OPCODE, "SCRIPT_ERR_BAD_OPCODE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_DISABLED_OPCODE, "SCRIPT_ERR_DISABLED_OPCODE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_INVALID_STACK_OPERATION, "SCRIPT_ERR_INVALID_STACK_OPERATION"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_INVALID_ALTSTACK_OPERATION, "SCRIPT_ERR_INVALID_ALTSTACK_OPERATION"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_UNBALANCED_CONDITIONAL, "SCRIPT_ERR_UNBALANCED_CONDITIONAL"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_DIV_BY_ZERO, "SCRIPT_ERR_DIV_BY_ZERO"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_MOD_BY_ZERO, "SCRIPT_ERR_MOD_BY_ZERO"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_NEGATIVE_LOCKTIME, "SCRIPT_ERR_NEGATIVE_LOCKTIME"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_UNSATISFIED_LOCKTIME, "SCRIPT_ERR_UNSATISFIED_LOCKTIME"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_SIG_HASHTYPE, "SCRIPT_ERR_SIG_HASHTYPE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_SIG_DER, "SCRIPT_ERR_SIG_DER"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_MINIMALDATA, "SCRIPT_ERR_MINIMALDATA"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_SIG_PUSHONLY, "SCRIPT_ERR_SIG_PUSHONLY"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_SIG_HIGH_S, "SCRIPT_ERR_SIG_HIGH_S"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_SIG_NULLDUMMY, "SCRIPT_ERR_SIG_NULLDUMMY"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_PUBKEYTYPE, "SCRIPT_ERR_PUBKEYTYPE"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_CLEANSTACK, "SCRIPT_ERR_CLEANSTACK"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_MINIMALIF, "SCRIPT_ERR_MINIMALIF"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_SIG_NULLFAIL, "SCRIPT_ERR_SIG_NULLFAIL"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS, "SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_NONCOMPRESSED_PUBKEY, "SCRIPT_ERR_NONCOMPRESSED_PUBKEY"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_ILLEGAL_FORKID, "SCRIPT_ERR_ILLEGAL_FORKID"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_MUST_USE_FORKID, "SCRIPT_ERR_MUST_USE_FORKID"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_BIG_INT, "SCRIPT_ERR_BIG_INT"));
        map.insert(boost::bimap< ScriptError_t, std::string >::value_type(SCRIPT_ERR_ERROR_COUNT, "SCRIPT_ERR_ERROR_COUNT"));

        return map;
    }
    boost::bimap< ScriptError_t, std::string > get_script_error_shortname_map()
    {
        typedef boost::bimap< ScriptError_t, std::string >::left_map::const_iterator left_iterator;
        boost::bimap< ScriptError_t, std::string > shortmap;
        const boost::bimap< ScriptError_t, std::string > map = get_script_error_name_map();
        for (left_iterator it = map.left.begin(); it != map.left.end(); ++it)
        {
            const ScriptError_t err_value = it->first;
            const std::string err_fullname = it->second;
            std::string err_name = err_fullname;

            switch (err_value)
            {
            case SCRIPT_ERR_INVALID_SPLIT_RANGE:
                err_name = "SPLIT_RANGE";
                break;
            case SCRIPT_ERR_INVALID_OPERAND_SIZE:
                err_name = "OPERAND_SIZE";
                break;
            case SCRIPT_ERR_SIG_NULLFAIL:
                err_name = "NULLFAIL";
                break;
            case SCRIPT_ERR_MUST_USE_FORKID:
                err_name = "MISSING_FORKID";
                break;
            default:
                boost::algorithm::replace_first(err_name, "SCRIPT_ERR_", "");
            }
            shortmap.insert(boost::bimap< ScriptError_t, std::string >::value_type(err_value, err_name));
        }
        return shortmap;
    }

    uint32_t string2flag(const std::string& strFlags) {
        if (strFlags.empty()) {
            return 0;
        }

        const auto mapflag = get_script_flag_name_map();
        uint32_t flags = 0;
        std::vector<std::string> words;
        boost::algorithm::split(words, strFlags, boost::algorithm::is_any_of(","));

        for (std::string &word : words) {
            if (mapflag.right.find(word) == mapflag.right.end())
                throw std::runtime_error("Bad script flag name : " + word);
            flags |= mapflag.right.at(word);
        }
        return flags;
    }

    uint32_t string2flag_short(const std::string& strFlags) {
        if (strFlags.empty()) {
            return 0;
        }

        const auto mapflag = get_script_flag_shortname_map();
        uint32_t flags = 0;
        std::vector<std::string> words;
        boost::algorithm::split(words, strFlags, boost::algorithm::is_any_of(","));

        for (std::string &word : words) {
            if (mapflag.right.find(word) == mapflag.right.end())
                throw std::runtime_error("Bad script flag short name : " + word);
            flags |= mapflag.right.at(word);
        }
        return flags;
    }
}