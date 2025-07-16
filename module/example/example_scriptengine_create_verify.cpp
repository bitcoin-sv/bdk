/// Use it as an example how to add a example

#include <cassert>
#include <iostream>
#include <sstream>

#include "base58.h"
#include "chainparams.h"
#include "config.h"
#include "core_io.h"
#include "key.h"
#include "script/script_num.h"
#include "univalue/include/univalue.h"

#include "keystore.h"
#include "script/script_num.h"
#include "script/sign.h"

#include "extendedTx.hpp"
#include "assembler.h"
#include "utilstrencodings.h"
#include "scriptengine.hpp"

const std::string strSecret1 = "5HxWvvfubhXpYYpS3tJkw6fq9jE9j18THftkZjHHfmFiWtmAbrj";

CMutableTransaction BuildCreditingTransaction(const CScript& scriptPubKey, const Amount nValue)
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

CMutableTransaction BuildSpendingTransaction(const CScript& scriptSig, const CMutableTransaction& txCredit)
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

std::vector<uint8_t> MakeSig(CScript& scriptPubkey,
                             const CKey& key,
                             CMutableTransaction& spendTx,
                             Amount amount,
                             SigHashType sigHashType = SigHashType().withForkId())
{
    uint256 hash = SignatureHash(scriptPubkey, CTransaction(spendTx), 0, sigHashType, amount);
    std::vector<uint8_t> vchSig;
    if (!key.Sign(hash, vchSig)) {
        throw std::runtime_error("ERROR signing");
    }

    vchSig.push_back(static_cast<uint8_t>(sigHashType.getRawSigHashType()));
    return vchSig;
}

CScript PushAll(const std::vector<valtype>& values) {
    CScript result;
    for (const valtype& v : values) {
        if (v.size() == 0) {
            result << OP_0;
        }
        else if (v.size() == 1 && v[0] >= 1 && v[0] <= 16) {
            result << EncodeOP_N(v[0]);
        }
        else {
            result << v;
        }
    }

    return result;
}

// This helper is to play and modify script pubkey (append op_code to that)
// Here is the example of using OP_RETURN, but we can do other things
bool ModifyScriptPubKey(CScript& script) {
    std::vector<uint8_t> data(1000, 1);
    script << OP_FALSE << OP_RETURN << data;
    return true;
}

int main(int argc, char* argv[])
{
    const auto network = CBaseChainParams::MAIN;
    bsv::CScriptEngine se(network);

    SelectParams(network); // must select params to set secret string
    const std::string strSecret1 = "5HxWvvfubhXpYYpS3tJkw6fq9jE9j18THftkZjHHfmFiWtmAbrj";
    CBitcoinSecret bsecret1;
    bsecret1.SetString(strSecret1);

    CKey key1 = bsecret1.GetKey();
    CPubKey pubkey1 = key1.GetPubKey();

    CBasicKeyStore keystore;// Might not need the keystore as we use the private key we created
    keystore.AddKey(key1);

    if(key1.VerifyPubKey(pubkey1))
        std::cout << "PubKey verified" << std::endl;

    // Construct a tx
    const Amount amount{1000000};
    CMutableTransaction txFrom = BuildCreditingTransaction(GetScriptForDestination(key1.GetPubKey().GetID()), amount);
    CMutableTransaction txTo = BuildSpendingTransaction(CScript(), txFrom);

    CScript& scriptPubKey = txFrom.vout[0].scriptPubKey;
    ModifyScriptPubKey(scriptPubKey);

    std::vector<uint8_t> vchSig = MakeSig(scriptPubKey, key1, txTo, amount);
    std::vector<valtype> sigResult;
    sigResult.push_back(vchSig);
    sigResult.push_back(ToByteVector(pubkey1));
    txTo.vin[0].scriptSig = PushAll(sigResult);

    bsv::CMutableTransactionExtended eTX;
    eTX.vutxo = txFrom.vout;
    eTX.mtx = txTo;

    CDataStream out_stream(SER_NETWORK, PROTOCOL_VERSION);
    out_stream << eTX;
    const std::vector<uint8_t> etxBin(out_stream.begin(), out_stream.end());
    std::array<int32_t, 1> utxoArray = { 820539 };
    const int32_t blockHeight = 820540;

    const ScriptError ret = se.VerifyScript(etxBin, std::span<const int32_t>(utxoArray) , blockHeight, false);

    const std::string etxHex = bsv::Bin2Hex(etxBin);
    std::cout << std::endl << std::endl << "ExtendedTX Hex" << std::endl << std::endl << etxHex << std::endl << std::endl;
    if (ret != SCRIPT_ERR_OK) {
        throw std::runtime_error("ERROR verify script for tx " + etxHex);
    }

    return 0;
}
