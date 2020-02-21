#include <config.h>                  // bitcoin code
#include <script/interpreter.h>      // bitcoin code
#include <script/script_num.h>       // bitcoin code

#include "core/FlagMap.hpp"
#include "core/ScriptHelper.hpp"
#include "core/ScriptHelper.hpp"

namespace bs
{
    int private_FindFirstOpPos(const CScript& script, const std::vector<opcodetype>& matches)// find one of the op in matches
    {
        CScript::const_iterator iter = script.begin();
        CScript::const_iterator end_iter = script.end();
        opcodetype tmp_opcode;
        valtype tmp_vchPushValue;
        int script_step{ -1 };
        while (iter < end_iter && script.GetOp(iter, tmp_opcode, tmp_vchPushValue))
        {
            ++script_step;
            for(auto matchOP : matches) {
                if (tmp_opcode == matchOP)
                {
                    return script_step;
                }
            }
        }
        return -1;
    }

    //// Eval full script in input and store final_status final_error in output
    void private_EvalScriptFull(const IncrementalEvalInput& input, IncrementalEvalOutput& output)
    {
        int flags = input.flag;
        const Amount& nValue = input.nValue;
        const CScript& script = input.single_script;
        const Config& config = GlobalConfig::GetConfig();
        if (flags & SCRIPT_VERIFY_CLEANSTACK) { flags |= SCRIPT_VERIFY_P2SH; }

        const auto checksig_pos = bs::help_FindFirstCheckSigPos(script);
        const bool script_has_checksig = (checksig_pos > -1);
        std::pair<CScript, CScript> script_pair = help_SplitCheckSigScript(input);
        const CScript scriptSig = script_pair.first;
        const CScript scriptPubKey = script_pair.second;
        ScriptError serror{ SCRIPT_ERR_UNKNOWN_ERROR };
        const bool consensus = true;
        const task::CCancellationToken token = task::CCancellationSource::Make()->GetToken();

        CMutableTransaction txCredit = bs::help_BuildCreditingTransaction(scriptPubKey, nValue);
        CMutableTransaction tx = bs::help_BuildSpendingTransaction(scriptSig, txCredit);
        CMutableTransaction tx2 = tx;
        std::unique_ptr<BaseSignatureChecker> checker_ptr;
        script_has_checksig
            ? checker_ptr = std::make_unique<MutableTransactionSignatureChecker>(&tx, 0, txCredit.vout[0].nValue)
            : checker_ptr = std::make_unique<SignatureCheckSkipper>();

        output.final_status = VerifyScript( config, consensus, token, scriptSig, scriptPubKey, flags, *checker_ptr.get(), &serror);
        output.final_error = serror;
    }

    bool SignatureCheckSkipper::CheckSig() const { return true; }

    bool SignatureCheckSkipper::CheckLockTime(const CScriptNum &nLockTime) const { return true; }

    bool SignatureCheckSkipper::CheckSequence(const CScriptNum &nSequence) const { return true; }

    CMutableTransaction help_BuildCreditingTransaction(const CScript &scriptPubKey, const Amount nValue)
    {
        CMutableTransaction txCredit;
        txCredit.nVersion = 1;
        txCredit.nLockTime = 0;
        txCredit.vin.resize(1);
        txCredit.vout.resize(1);
        txCredit.vin[0].prevout = COutPoint();
        txCredit.vin[0].scriptSig = CScript() << CScriptNum(0) << CScriptNum(0);
        txCredit.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
        txCredit.vout[0].scriptPubKey = scriptPubKey;
        txCredit.vout[0].nValue = nValue;

        return txCredit;
    }

    CMutableTransaction help_BuildSpendingTransaction(const CScript &scriptSig, const CMutableTransaction &txCredit)
    {
        CMutableTransaction txSpend;
        txSpend.nVersion = 1;
        txSpend.nLockTime = 0;
        txSpend.vin.resize(1);
        txSpend.vout.resize(1);
        txSpend.vin[0].prevout = COutPoint(txCredit.GetId(), 0);
        txSpend.vin[0].scriptSig = scriptSig;
        txSpend.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
        txSpend.vout[0].scriptPubKey = CScript();
        txSpend.vout[0].nValue = txCredit.vout[0].nValue;

        return txSpend;
    }

    int help_GetNumberStep(const CScript &script)
    {
        CScript::const_iterator iter = script.begin();
        CScript::const_iterator end_iter = script.end();
        opcodetype tmp_opcode;
        valtype tmp_vchPushValue;
        int nb_step{0};
        while (iter < end_iter && script.GetOp(iter, tmp_opcode, tmp_vchPushValue))
        {
            ++nb_step;
            //if (tmp_opcode == OP_RETURN)
            //    break;
        }
        return nb_step;
    }

    int help_FindFirstCheckSigPos(const CScript &script)
    {
        const std::vector<opcodetype> checksig_ops{ OP_CHECKSIG , OP_CHECKSIGVERIFY , OP_CHECKMULTISIG , OP_CHECKMULTISIGVERIFY };
        return private_FindFirstOpPos(script, checksig_ops);
    }

    int help_FindFirstReturnPos(const CScript &script)
    {
        const std::vector<opcodetype> return_ops{ OP_RETURN };
        return private_FindFirstOpPos(script, return_ops);
    }

    std::pair<CScript, CScript> help_SplitCheckSigScript(const IncrementalEvalInput& eval_input)
    {
        const CScript &script = eval_input.single_script;
        const auto checksig_op_pos = help_FindFirstCheckSigPos(script);
        if(checksig_op_pos > -1)
        {
            CScript::const_iterator start_iter = script.begin();
            CScript::const_iterator iter = script.begin();
            CScript::const_iterator end_iter = script.end();
            opcodetype tmp_opcode;
            valtype tmp_vchPushValue;
            int script_op_pos{0};

            // Truncate the script to get the script up to before the checksig op
            while(script_op_pos < checksig_op_pos && (iter < end_iter && script.GetOp(iter, tmp_opcode, tmp_vchPushValue)))
            {
                // Run the iter through the position just before the checksig op
                ++script_op_pos;
            }
            const CScript truncated_scriptBeforeCheckSigOp(start_iter, iter);

            const Config& config = GlobalConfig::GetConfig();
            LimitedStack scriptSig_stack(config.GetMaxStackMemoryUsage(eval_input.flag & SCRIPT_UTXO_AFTER_GENESIS, true));
            const bs::SignatureCheckSkipper check_skipper;
            ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};

            auto eval_res = EvalScript(config, true, task::CCancellationSource::Make()->GetToken(), scriptSig_stack, truncated_scriptBeforeCheckSigOp, eval_input.flag, check_skipper, &err);
            if (!eval_res.has_value() || !eval_res.value() || err != SCRIPT_ERR_OK)
                throw std::runtime_error("help_SplitCheckSigScript error when evaluate truncated_scriptBeforeCheckSigOp");

            /// Calculate scriptSig and ScriptPubKey
            CScript scriptSig;
            CScript::const_iterator start_scriptPubKey_iter = script.begin();
            const size_t stack_size = scriptSig_stack.size();
            //LimitedVector& stacktop(int index);
            for (size_t iPos = 0; iPos < stack_size; ++iPos)
            {
                const int top_id = (int)iPos - (int)stack_size;
                scriptSig << scriptSig_stack.stacktop(top_id).GetElement();

                const bool is_iter_ok = start_scriptPubKey_iter < end_iter;
                const bool is_getop_ok = script.GetOp(start_scriptPubKey_iter, tmp_opcode, tmp_vchPushValue);
                if (!is_iter_ok || !is_getop_ok)
                    throw std::runtime_error("help_SplitCheckSigScript error when calculating scriptSig and ScriptPubKey ");
            }

            const CScript scriptPubKey(start_scriptPubKey_iter, end_iter);
            return std::make_pair(scriptSig, scriptPubKey);
        }

        return std::make_pair(script, CScript());
    }

    std::vector<CScript> BuildIncrementalScript(const CScript& script)
    {
        std::vector<CScript> script_list;
        CScript::const_iterator start_iter = script.begin();
        CScript::const_iterator iter = script.begin();
        CScript::const_iterator end_iter = script.end();
        opcodetype tmp_opcode;
        valtype tmp_vchPushValue;
        while (iter < end_iter && script.GetOp(iter, tmp_opcode, tmp_vchPushValue))
        {
            script_list.emplace_back(start_iter, iter);
        }
        return script_list;
    }

    void IncrementalScriptEval(const IncrementalEvalInput& input, IncrementalEvalOutput& output)
    {
        int flags = input.flag;
        const Amount& nValue = input.nValue;
        const CScript& full_script = input.single_script;
        const Config& config = GlobalConfig::GetConfig();

        const std::vector<CScript> inc_script_list = BuildIncrementalScript(input.single_script);
        for(size_t i=0;i< inc_script_list.size();++i)
            output.incremental_stacks.emplace_back(config.GetMaxStackMemoryUsage(flags & SCRIPT_UTXO_AFTER_GENESIS, true));

        for (size_t iStep=0; iStep < inc_script_list.size();++iStep)
        {
            const CScript& script = inc_script_list[iStep];
            // If script has checksig op, then it will need to split the script to scriptSig and scriptPubKey to check and verify
            const auto checksig_pos = help_FindFirstCheckSigPos(script);
            const bool script_has_checksig = (checksig_pos > -1);
            std::pair<CScript, CScript> script_pair = help_SplitCheckSigScript(input);
            const CScript& scriptSig    = script_pair.first;
            const CScript& scriptPubKey = script_pair.second;
            ScriptError serror{ SCRIPT_ERR_UNKNOWN_ERROR };
            const bool consensus = true;
            const task::CCancellationToken token = task::CCancellationSource::Make()->GetToken();

            CMutableTransaction txCredit = bs::help_BuildCreditingTransaction(scriptPubKey, nValue);
            CMutableTransaction tx = bs::help_BuildSpendingTransaction(scriptSig, txCredit);
            CMutableTransaction tx2 = tx;
            std::unique_ptr<BaseSignatureChecker> checker_ptr;
            script_has_checksig
                ? checker_ptr = std::make_unique<MutableTransactionSignatureChecker>(&tx, 0, txCredit.vout[0].nValue)
                : checker_ptr = std::make_unique<SignatureCheckSkipper>();

            ///////////////    first chunk in VerifyScript ///////////////////////
            ///// Eval the script with 2 go, similar to VerifyScript
            ///// If FORKID is enabled, we also ensure strict encoding.
            if (flags & SCRIPT_ENABLE_SIGHASH_FORKID) { flags |= SCRIPT_VERIFY_STRICTENC; }

            if ((flags & SCRIPT_VERIFY_SIGPUSHONLY) != 0 && !scriptSig.IsPushOnly()) {
                //return set_error(serror, SCRIPT_ERR_SIG_PUSHONLY);
                continue;
            }

            LimitedStack stack(config.GetMaxStackMemoryUsage(flags & SCRIPT_UTXO_AFTER_GENESIS, consensus));
            LimitedStack stackCopy(config.GetMaxStackMemoryUsage(flags & SCRIPT_UTXO_AFTER_GENESIS, consensus));
            if (scriptSig.size() > 0)
            {
                if (auto res = EvalScript(config, consensus, token, stack, scriptSig, flags, *checker_ptr.get(), &serror); !res.has_value() || !res.value())
                {
                    //return res;
                    output.incremental_stacks[iStep] = stack.makeRootStackCopy();
                    continue;
                }
            }
            if (scriptPubKey.size() > 0)
            {
                if ((flags & SCRIPT_VERIFY_P2SH) && !(flags & SCRIPT_UTXO_AFTER_GENESIS)) {
                    stackCopy = stack.makeRootStackCopy();
                }
                if (auto res = EvalScript(config, consensus, token, stack, scriptPubKey, flags, *checker_ptr.get(), &serror); !res.has_value() || !res.value())
                {
                    //return res;
                    output.incremental_stacks[iStep] = stack.makeRootStackCopy();
                    continue;
                }
            }
            output.incremental_stacks[iStep] = stack.makeRootStackCopy();
            ///////////////    first chunk in VerifyScript ///////////////////////
        }

        /// Eval fully the full input script, and set the output return and error
        private_EvalScriptFull(input, output);
    }
}// namespace bs