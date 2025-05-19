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

#include "interpreter_bdk.hpp"
#include "extendedTx.hpp"
#include "assembler.h"
#include "utilstrencodings.h"
#include "scriptengine.hpp"

const std::string strSecret1 = "5HxWvvfubhXpYYpS3tJkw6fq9jE9j18THftkZjHHfmFiWtmAbrj";

void NegateSignatureS(std::vector<uint8_t>& vchSig)
{
    // Parse the signature.
    std::vector<uint8_t> r(vchSig.begin() + 4, vchSig.begin() + 4 + vchSig[3]);
    std::vector<uint8_t> s(vchSig.begin() + 6 + vchSig[3],
                           vchSig.begin() + 6 + vchSig[3] +
                               vchSig[5 + vchSig[3]]);

    // Really ugly to implement mod-n negation here, but it would be feature
    // creep to expose such functionality from libsecp256k1.
    static const uint8_t order[33] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xBA, 0xAE, 0xDC, 0xE6, 0xAF,
                                      0x48, 0xA0, 0x3B, 0xBF, 0xD2, 0x5E, 0x8C, 0xD0, 0x36, 0x41, 0x41};
    while(s.size() < 33)
    {
        s.insert(s.begin(), 0x00);
    }

    int carry = 0;
    for(int p = 32; p >= 1; p--)
    {
        int n = (int)order[p] - s[p] - carry;
        s[p] = (n + 256) & 0xFF;
        carry = (n < 0);
    }

    assert(carry == 0);
    if(s.size() > 1 && s[0] == 0 && s[1] < 0x80)
    {
        s.erase(s.begin());
    }

    // Reconstruct the signature.
    vchSig.clear();
    vchSig.push_back(0x30);
    vchSig.push_back(4 + r.size() + s.size());
    vchSig.push_back(0x02);
    vchSig.push_back(r.size());
    vchSig.insert(vchSig.end(), r.begin(), r.end());
    vchSig.push_back(0x02);
    vchSig.push_back(s.size());
    vchSig.insert(vchSig.end(), s.begin(), s.end());
}

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

std::vector<uint8_t> MakeSig(CScript& script,
                             const CKey& key,
                             CMutableTransaction& spendTx,
                             SigHashType sigHashType = SigHashType().withForkId(),
                             unsigned int lenR = 32,
                             unsigned int lenS = 32,
                             Amount amount = Amount(0),
                             uint32_t flags = SCRIPT_ENABLE_SIGHASH_FORKID)
{
    uint256 hash = SignatureHash(script, CTransaction(spendTx), 0, sigHashType, amount, nullptr, flags);
    std::vector<uint8_t> vchSig, r, s;
    uint32_t iter = 0;
    do
    {
        key.Sign(hash, vchSig, iter++);
        if((lenS == 33) != (vchSig[5 + vchSig[3]] == 33))
        {
            NegateSignatureS(vchSig);
        }

        r = std::vector<uint8_t>(vchSig.begin() + 4, vchSig.begin() + 4 + vchSig[3]);
        s = std::vector<uint8_t>(vchSig.begin() + 6 + vchSig[3],
                                 vchSig.begin() + 6 + vchSig[3] + vchSig[5 + vchSig[3]]);
    } while(lenR != r.size() || lenS != s.size());

    vchSig.push_back(static_cast<uint8_t>(sigHashType.getRawSigHashType()));
    return vchSig;
}

int main(int argc, char* argv[])
{
    bsv::CScriptEngine se("main");
    //std::string errStr;
    //bool ok = se.SetMaxStackMemoryUsage(272, 4, &errStr);
    //if (!(ok && errStr.empty())) {
    //    throw std::runtime_error("unable to set SetMaxStackMemoryUsage. Error : " + errStr);
    //}

    CBasicKeyStore keystore;
    std::vector<CKey> keys;
    std::vector<CPubKey> pubkeys;
    for (int i = 0; i < 1; i++) {
        CKey key;
        key.MakeNewKey(i % 2 == 1);
        keys.push_back(key);
        pubkeys.push_back(key.GetPubKey());
        keystore.AddKey(key);
    }

    auto key1 = keys[0];
    auto pubkey1 = pubkeys[0];
    if(key1.VerifyPubKey(pubkey1))
        std::cout << "PubKey verified" << std::endl;

    // Construct a tx
    const Amount amount{1000000};
    CMutableTransaction txFrom = BuildCreditingTransaction(GetScriptForDestination(keys[0].GetPubKey().GetID()), amount);
    CMutableTransaction txTo = BuildSpendingTransaction(CScript(), txFrom);

    // Append the part to create a non-standard utxo
    // CScript& scriptPubKey = txFrom.vout[0].scriptPubKey;
    //std::vector<uint8_t> arg0(MAX_SCRIPT_NUM_LENGTH_AFTER_GENESIS, 42);
    //CScript extendScript = CScript() << arg0 << OP_DUP << OP_NUMEQUALVERIFY;
    //scriptPubKey << ToByteVector(extendScript);

    // Single signature case:
    const bool ok = SignSignature(se.GetGlobalConfig(), keystore, ProtocolEra::PostGenesis, ProtocolEra::PostGenesis, CTransaction(txFrom), txTo, 0,
        SigHashType().withForkId()); // changes scriptSig

    bsv::CMutableTransactionExtended eTX;
    eTX.vutxo = txFrom.vout;
    eTX.mtx = txTo;

    CDataStream out_stream(SER_NETWORK, PROTOCOL_VERSION);
    out_stream << eTX;
    const std::vector<uint8_t> etxBin(out_stream.begin(), out_stream.end());
    std::array<int32_t, 1> utxoArray = { 820539 };
    const int32_t blockHeight = 820540;

    const ScriptError ret = se.VerifyScript(etxBin, std::span<const int32_t>(utxoArray) , blockHeight, true);

    const std::string etxHex = bsv::Bin2Hex(etxBin);
    if (ret != SCRIPT_ERR_OK) {
        throw std::runtime_error("ERROR verify script for tx " + etxHex);
    }

    return 0;
}
