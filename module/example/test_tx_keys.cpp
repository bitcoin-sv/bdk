/// Use it as an example how to add a example

#include <iostream>
#include <sstream>
// bitcoin headers
#include <base58.h>
#include <cassert>
#include <chainparams.h>
#include <config.h>
#include <core_io.h>
#include <key.h>
#include <script/script_num.h>
#include <script_engine.h>
#include <univalue/include/univalue.h>

const std::string strSecret1 = "5HxWvvfubhXpYYpS3tJkw6fq9jE9j18THftkZjHHfmFiWtmAbrj";

// lifted from Nakasendo (conversions.h / conversions.cpp)
constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

std::string binTohexStr(const std::unique_ptr<unsigned char[]>& data, int len)
{
    std::string s(len * 2, ' ');
    for(int i = 0; i < len; ++i)
    {
        s[2 * i] = hexmap[(data.get()[i] & 0xF0) >> 4];
        s[2 * i + 1] = hexmap[data.get()[i] & 0x0F];
    }
    return s;
}

std::string UintToHex(const std::vector<uint8_t>& UintRep)
{
    std::unique_ptr<unsigned char[]> uncharRep(new unsigned char[UintRep.size()]);
    int index(0);
    for(std::vector<uint8_t>::const_iterator iter = UintRep.begin(); iter != UintRep.end(); ++iter)
    {
        uncharRep[index++] = *iter;
    }
    return (binTohexStr(uncharRep, UintRep.size()));
}

void NegateSignatureS(std::vector<uint8_t>& vchSig)
{
    // Parse the signature.
    std::vector<uint8_t> r, s;
    r = std::vector<uint8_t>(vchSig.begin() + 4, vchSig.begin() + 4 + vchSig[3]);
    s = std::vector<uint8_t>(vchSig.begin() + 6 + vchSig[3],
                             vchSig.begin() + 6 + vchSig[3] + vchSig[5 + vchSig[3]]);

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
                             SigHashType sigHashType = SigHashType(),
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

    // Set up for Bitcoin libs
    const GlobalConfig& testConfig = GlobalConfig::GetConfig();
    SelectParams(CBaseChainParams::TESTNET);
    ECCVerifyHandle globalVerifyHandle;
    ECC_Start();

    std::cout << "Starting..." << std::endl;
    CBitcoinSecret bsecret1;
    bsecret1.SetString(strSecret1);

    CKey key1 = bsecret1.GetKey();
    CPubKey pubkey1 = key1.GetPubKey();

    if(key1.VerifyPubKey(pubkey1))
        std::cout << "PubKey verified" << std::endl;

    std::string strMsg = strprintf("Very secret message %i: 11", 0);
    uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());
    // std::cout << keys.key0 << std::endl;
    std::vector<uint8_t> sign1;
    key1.Sign(hashMsg, sign1);

    if(pubkey1.Verify(hashMsg, sign1))
        std::cout << "Hashed message verified" << std::endl;

    // COnstruct a tx
    CScript scriptSig;
    scriptSig << OP_DUP << OP_HASH160 << ToByteVector(pubkey1.GetID()) << OP_EQUALVERIFY << OP_CHECKSIGVERIFY;

    CScript Pubkey;
    Pubkey << ToByteVector(pubkey1);

    CScript scriptToHash;
    scriptToHash = Pubkey + scriptSig;

    CMutableTransaction creditTx = BuildCreditingTransaction(scriptSig, Amount(10));

    CMutableTransaction spendTx = BuildSpendingTransaction(CScript(), creditTx);

    CTransaction ctx(creditTx);
    CTransaction stx(spendTx);
    std::cout << "Credit TX " << ctx.ToString() << std::endl;
    std::cout << "Spending TX " << stx.ToString() << std::endl;

    std::vector<uint8_t> sig = MakeSig(scriptToHash, key1, spendTx);

    std::cout << EncodeHexTx(stx) << std::endl;
    const std::string& hextx = EncodeHexTx(stx, 0);

    CMutableTransaction rebuilttx;

    if(DecodeHexTx(rebuilttx, hextx))
    {

        CTransaction t(rebuilttx);

        uint256 hash = SignatureHash(scriptToHash, stx, 0, SigHashType(), Amount(10), nullptr,
                                     SCRIPT_ENABLE_SIGHASH_FORKID);

        if(pubkey1.Verify(hash, sig))
            std::cout << "Sig hash verified - value of hash is " << hash.ToString() << std::endl;
    }
    ECC_Stop();

    CScript finalScript;
    finalScript << sig << ToByteVector(pubkey1);

    finalScript += scriptSig;

    Amount a(10);
    int64_t amt = a.GetSatoshis();
    std::cout << "Amount is: " << amt << std::endl;
    const std::string& scriptStr = FormatScript(finalScript);

    std::string scr = FormatScript(finalScript);
    std::cout << scr << std::endl;
    if(bsv::evaluate(scr, true, 0, hextx, 0, amt))
    {
        std::cout << "Successfully executed script with a checksig" << std::endl;
    }

    std::cout << "Finishing" << std::endl;
    return 0;
}
