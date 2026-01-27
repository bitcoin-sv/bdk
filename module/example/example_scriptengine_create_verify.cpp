/// Use it as an example how to add a example

#include <cassert>
#include <iostream>
#include <sstream>

#include "base58.h"
#include "chainparams.h"
#include "util.h" // SelectParams
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

// Build a crediting transaction with P2SH output
// This creates a transaction with a Pay-to-Script-Hash output that locks coins
// to a specific redeem script
static CMutableTransaction
BuildCreditingTransactionWithP2SHOutput(const CScript &redeemScript, const Amount& nValue)
{
    CMutableTransaction txCredit;
    txCredit.nVersion = 1;
    txCredit.nLockTime = 0;
    txCredit.vin.resize(1);
    txCredit.vout.resize(1);
    txCredit.vin[0].prevout = COutPoint();
    txCredit.vin[0].scriptSig = CScript() << CScriptNum(0) << CScriptNum(0);
    txCredit.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    
    // Create P2SH scriptPubKey from the redeem script
    // Format: OP_HASH160 <20-byte-hash-of-redeemScript> OP_EQUAL
    txCredit.vout[0].scriptPubKey = CScript() << OP_HASH160 
                                               << ToByteVector(CScriptID(redeemScript)) 
                                               << OP_EQUAL;
    txCredit.vout[0].nValue = nValue;

    return txCredit;
}

// Improved version 1: Build P2SH scriptSig explicitly
static CScript BuildP2SHScriptSig(const CScript& dataScript, const CScript& redeemScript) {
    // dataScript should contain the signatures/data needed to satisfy redeemScript
    // This function appends the redeemScript at the end
    CScript result = dataScript;
    result << ToByteVector(redeemScript);
    return result;
}

// Build a spending transaction that spends from a P2SH output and creates n outputs
// The scriptSig should contain the redeem script and any data needed to satisfy it
// The input value is divided equally among the n outputs
static CMutableTransaction
BuildSpendingTransactionMultipleOutputs(const CScript &scriptSig,
                                        const CMutableTransaction &txCredit,
                                        size_t numOutputs,
                                        int32_t nVersion = 1)
{
    CMutableTransaction txSpend;
    txSpend.nVersion = nVersion;
    txSpend.nLockTime = 0;
    txSpend.vin.resize(1);
    txSpend.vout.resize(numOutputs);
    
    txSpend.vin[0].prevout = COutPoint(txCredit.GetId(), 0);
    txSpend.vin[0].scriptSig = scriptSig;  // Just assign, don't modify
    txSpend.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    
    // Divide value among outputs
    Amount totalValue = txCredit.vout[0].nValue;
    Amount valuePerOutput = Amount(totalValue.GetSatoshis() / numOutputs);
    Amount remainder = Amount(totalValue.GetSatoshis() % numOutputs);
    
    for (size_t i = 0; i < numOutputs; ++i) {
        txSpend.vout[i].scriptPubKey = CScript();
        txSpend.vout[i].nValue = valuePerOutput;
        if (i == 0) {
            txSpend.vout[i].nValue = valuePerOutput + remainder;
        }
    }

    return txSpend;
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

// Custom signing function with FORKID support for BSV
// This is the modified variant of sign_multisig with injected forkid
CScript sign_multisig_forkid(const CScript& scriptPubKey,
    const std::vector<CKey>& keys,
    const CTransaction& transaction,
    const Amount& amount)
{
    // Use SIGHASH_ALL with FORKID
    SigHashType sigHashType = SigHashType().withForkId();

    uint256 hash = SignatureHash(scriptPubKey, transaction, 0, sigHashType, amount);

    CScript result;
    // CHECKMULTISIG requires OP_0 due to off-by-one bug
    result << OP_0;

    for (const CKey& key : keys) {
        std::vector<uint8_t> vchSig;
        key.Sign(hash, vchSig);
        // Append SIGHASH_ALL | SIGHASH_FORKID (0x41)
        vchSig.push_back(uint8_t(sigHashType.getRawSigHashType()));
        result << vchSig;
    }

    return result;
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

bsv::CMutableTransactionExtended BuildReguarTransaction(const std::string& network);
bsv::CMutableTransactionExtended BuildP2SHTransaction(const std::string& network);

// This helper is to play and modify script pubkey (append op_code to that)
// Here is the example of using OP_RETURN, but we can do other things
bool AppendScriptPubKey(CScript& script) {
    //std::vector<uint8_t> data(1000, 1);
    //script << OP_FALSE << OP_RETURN << data;
    return true;
}

int main(int argc, char* argv[])
{
    const auto network = CBaseChainParams::MAIN;
    bsv::CScriptEngine se(network);

    //const bsv::CMutableTransactionExtended eTX = BuildReguarTransaction(network);
    const bsv::CMutableTransactionExtended eTX = BuildP2SHTransaction(network);
    
    CDataStream out_stream(SER_NETWORK, PROTOCOL_VERSION);
    out_stream << eTX;
    const std::vector<uint8_t> etxBin(out_stream.begin(), out_stream.end());
    std::array<int32_t, 1> utxoArray = { 520539 };
    const int32_t blockHeight = 820540;

    const ScriptError ret = se.VerifyScript(etxBin, std::span<const int32_t>(utxoArray) , blockHeight, false);

    const std::string etxHex = bsv::Bin2Hex(etxBin);
    std::cout << std::endl << std::endl << "ExtendedTX Hex" << std::endl << std::endl << etxHex << std::endl << std::endl;
    if (ret != SCRIPT_ERR_OK) {
        throw std::runtime_error("ERROR verify script for tx " + etxHex);
    }

    return 0;
}


bsv::CMutableTransactionExtended BuildReguarTransaction(const std::string& network){
    SelectParams(network, std::nullopt);
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
    txTo.vout[0].scriptPubKey = GetScriptForDestination(key1.GetPubKey().GetID());

    CScript& scriptPubKey = txFrom.vout[0].scriptPubKey;
    AppendScriptPubKey(scriptPubKey);

    std::vector<uint8_t> vchSig = MakeSig(scriptPubKey, key1, txTo, amount);
    std::vector<valtype> sigResult;
    sigResult.push_back(vchSig);
    sigResult.push_back(ToByteVector(pubkey1));
    txTo.vin[0].scriptSig = PushAll(sigResult);

    bsv::CMutableTransactionExtended eTX;
    eTX.vutxo = txFrom.vout;
    eTX.mtx = txTo;
    return eTX;
}

bsv::CMutableTransactionExtended BuildP2SHTransaction(const std::string& network){


    // Example: Create a 2-of-3 multisig P2SH transaction and spend it with 3 outputs
    // Step 1: Create 3 keys for the 2-of-3 multisig
    // Setup network
    SelectParams(network, std::nullopt);

    // Create 3 keys from WIF
    const std::string strSecret1 = "5HxWvvfubhXpYYpS3tJkw6fq9jE9j18THftkZjHHfmFiWtmAbrj";
    const std::string strSecret2 = "5KC4ejrDjv152FGwP386VD1i2NYc5KkfSMyv1nGy1VGDxGHqVY3";
    const std::string strSecret3 = "5J3mBbAH58CpQ3Y5RNJpUKPE62SQ5tfcvU2JpbnkeyhfsYB1Jcn";

    CBitcoinSecret bsecret1, bsecret2, bsecret3;
    bsecret1.SetString(strSecret1);
    bsecret2.SetString(strSecret2);
    bsecret3.SetString(strSecret3);

    CKey key1 = bsecret1.GetKey();
    CKey key2 = bsecret2.GetKey();
    CKey key3 = bsecret3.GetKey();

    // Build 2-of-3 multisig redeem script
    CScript redeemScript;
    redeemScript << OP_2 
                << ToByteVector(key1.GetPubKey()) 
                << ToByteVector(key2.GetPubKey()) 
                << ToByteVector(key3.GetPubKey()) 
                << OP_3 
                << OP_CHECKMULTISIG;

    // Create P2SH crediting transaction
    Amount nValue = Amount(1500000);
    CMutableTransaction txP2SH = BuildCreditingTransactionWithP2SHOutput(redeemScript, nValue);

    // Create UNSIGNED spending transaction template for signing
    CMutableTransaction txSpend = BuildSpendingTransactionMultipleOutputs(
        CScript(),  // Empty scriptSig initially
        txP2SH,
        3,  // 3 outputs
        1
    );

    txSpend.vout[0].scriptPubKey = GetScriptForDestination(key1.GetPubKey().GetID());
    txSpend.vout[1].scriptPubKey = GetScriptForDestination(key2.GetPubKey().GetID());
    txSpend.vout[2].scriptPubKey = GetScriptForDestination(key3.GetPubKey().GetID());

    // Sign with 2 of the 3 keys
    std::vector<CKey> signingKeys;
    signingKeys.push_back(key1);
    signingKeys.push_back(key3);
    CScript signatures = sign_multisig_forkid(redeemScript, signingKeys, CTransaction(txSpend), nValue);

    // Build complete P2SH scriptSig: <signatures> <redeemScript>
    CScript completeScriptSig = BuildP2SHScriptSig(signatures, redeemScript);

    // Create final spending transaction with complete scriptSig
    // Update the scriptSig on the EXISTING transaction (don't create a new one!)
    txSpend.vin[0].scriptSig = completeScriptSig;

    bsv::CMutableTransactionExtended eTX;
    eTX.vutxo = txP2SH.vout;
    eTX.mtx = txSpend;
    return std::move(eTX);
}