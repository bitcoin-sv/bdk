#ifndef SCRIPT_HELPER_HPP
#define SCRIPT_HELPER_HPP
///////////////////////////////////////////////////////////////////
//  Date             10/02/2020                                  //
//  Author           Chi Thanh NGUYEN                            //
//                                                               //
//  Copyright (c) 2020 nChain Limited. All rights reserved       //
///////////////////////////////////////////////////////////////////

// ScriptHelper implement some additional tool wrapper to the native bitcoin source code
//
#include <interpreter.h>  // bitcoin code

namespace bs
{
    /// \class SignatureCheckSkipper
    /// \brief SignatureCheckSkipper implement a 'fake' signature checker that return always true for any check
    ///        It is use to accompy the function EvalScript where no signature check is needed
    class SignatureCheckSkipper : public BaseSignatureChecker
    {
    public:
        virtual bool CheckSig() const;

        virtual bool CheckLockTime(const CScriptNum &nLockTime) const;

        virtual bool CheckSequence(const CScriptNum &nSequence) const;
    };
    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    CMutableTransaction help_BuildCreditingTransaction(const CScript &scriptPubKey, const Amount nValue);

    CMutableTransaction help_BuildSpendingTransaction(const CScript &scriptSig, const CMutableTransaction &txCredit);

    //! return number of elements in term of atomic execution step in EvalScript
    //! Note that the case where there are OP_RETURN, all the trailing data in CScript is considered as one additional step
    int help_GetNumberStep(const CScript &script);

    //! return position of the first checksig op if exists. If not return -1
    //  Note that the concept of position here is not the trivial position. It is the number of step that CScript::GetOp is evaluated
    int help_FindFirstCheckSigPos(const CScript &script);

    //! return position of the first RETURN op if exists. If not return -1
    //  Note that the concept of position here is not the trivial position. It is the number of step that CScript::GetOp is evaluated
    int help_FindFirstReturnPos(const CScript &script);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////

    //! From a initial script, pop its elements and get the list of resulting scripts
    //  Example
    //    Initial script   : [A B C D]
    //    Incremental List : [A] [A B] [A B C] [A B C D]
    //  Note that scripts might not differ exactly by one op code, but it differ by one atomic execution step of the script
    //  Think about push data opcode, the difference might be the push opcode and the data
    std::vector<CScript> BuildIncrementalScript(const CScript& script);

    struct IncrementalEvalInput
    {
        unsigned int flag;
        Amount nValue;         // optional, will be used if op checksig appear
        CScript single_script; // will be split to scriptSig and scriptPubKey if op checksig occurs
    };

    struct IncrementalEvalOutput
    {
        std::optional<bool> final_status; // Hold the final status of the incremental script eval
        ScriptError_t       final_error ; // Hold the final script error of the incremental script eval
        std::vector<LimitedStack> incremental_stacks; // Incremental list of stack result corresponding to the incremental list of script
    };

    //! return a pair of <scriptSig,scriptPubKey> if any check sig op is present in the script.
    //  If not, the scriptPubKey will be empty and the input script will be copied to the scriptSig. TODO : not sure if that's ok
    std::pair<CScript, CScript> help_SplitCheckSigScript(const IncrementalEvalInput& eval_input);

    //!
    void IncrementalScriptEval(const IncrementalEvalInput& input, IncrementalEvalOutput& output);
}// namespace bs

#endif /* SCRIPT_HELPER_HPP */