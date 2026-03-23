/// example_special_opcode_verify.cpp
///
/// Verifies all 15 opcodes that were re-enabled in BSV (Genesis / Chronicle)
/// using *real-world* transactions: each UTXO is locked with both an opcode
/// check and a P2PKH signature requirement, and the spending input carries a
/// genuine ECDSA signature.
///
/// ─────────────────────────────────────────────────────────────────────────
/// GENERIC TRANSACTION STRUCTURE
/// ─────────────────────────────────────────────────────────────────────────
///
///   scriptPubKey  (in the crediting tx output / UTXO)
///   ┌──────────────────────────────────────────────────────────────────┐
///   │  <special-opcode>  <expected-result>  OP_EQUALVERIFY            │
///   │  OP_DUP  OP_HASH160  <pubKeyHash>  OP_EQUALVERIFY  OP_CHECKSIG  │
///   │                                                                  │
///   │  Two-part lock:                                                  │
///   │   1. The opcode must produce the expected result (EQUALVERIFY).  │
///   │   2. The spender must own the private key for <pubKeyHash>       │
///   │      and provide a valid signature (P2PKH).                      │
///   └──────────────────────────────────────────────────────────────────┘
///
///   scriptSig  (in the spending tx input)
///   ┌──────────────────────────────────────────────────────────────────┐
///   │  <sig>  <pubkey>  <operand A>  <operand B>  …                   │
///   │                                                                  │
///   │  Only push opcodes are allowed here (post-Genesis consensus      │
///   │  enforces SCRIPT_VERIFY_SIGPUSHONLY on scriptSig).               │
///   │                                                                  │
///   │  Stack layout after scriptSig runs (bottom → top):              │
///   │    [ sig | pubkey | operand_A | operand_B | … ]                 │
///   │                                                                  │
///   │  The scriptPubKey opcode consumes the operands from the top,     │
///   │  leaving sig and pubkey at the bottom for CHECKSIG.              │
///   └──────────────────────────────────────────────────────────────────┘
///
/// Signing process:
///   1. Build the full scriptPubKey (opcode check + P2PKH).
///   2. Build the crediting transaction; its output is the UTXO.
///   3. Build the spending transaction template with an empty scriptSig.
///   4. Compute the BIP143 sighash (SIGHASH_ALL | SIGHASH_FORKID) over
///      the spending transaction, referencing the scriptPubKey and amount.
///   5. Sign the hash with the private key → DER sig + sighash byte.
///   6. Assemble the final scriptSig: <sig> <pubkey> <operands>.
///   7. Set the scriptSig on the spending transaction.
///   8. Embed the UTXO into a CMutableTransactionExtended and verify.
///
/// ─────────────────────────────────────────────────────────────────────────
/// OPCODE ACTIVATION TIMELINE (mainnet)
/// ─────────────────────────────────────────────────────────────────────────
///   Monolith  (May 2018, block ~530,359) : OP_CAT, OP_SPLIT,
///                                          OP_NUM2BIN, OP_BIN2NUM
///   Magnetic  (Nov 2018, block ~556,767) : OP_MUL, OP_INVERT,
///                                          OP_LSHIFT, OP_RSHIFT
///   Genesis   (Feb 2020, block  620,538) : OP_AND, OP_OR, OP_XOR,
///                                          OP_DIV, OP_MOD  (+big-number
///                                          support for all the above)
///   Chronicle (block 943,816)            : OP_2MUL, OP_2DIV
///
/// All 15 cases are executed under a single post-Chronicle context so the
/// same UTXO height / block height pair covers every opcode.

#include <array>
#include <cassert>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

#include "base58.h"
#include "chainparams.h"
#include "key.h"
#include "script/script_num.h"
#include "script/sign.h"
#include "util.h"

#include "assembler.h"
#include "extendedTx.hpp"
#include "scriptengine.hpp"
#include "utilstrencodings.h"

// ═══════════════════════════════════════════════════════════════════════════
// Block-height constants  (mainnet)
// ═══════════════════════════════════════════════════════════════════════════

/// Genesis activated at block 620,538.  UTXOs mined at height ≥ 620,539
/// are "after-Genesis": script element size limits are lifted and big-number
/// arithmetic is enabled.
static constexpr int32_t GENESIS_HEIGHT = 620538;   // src/chainparams.cpp: GENESIS_ACTIVATION_MAIN

/// Chronicle activated at block 943,816.  UTXOs mined at height ≥ 943,817
/// are "after-Chronicle": OP_2MUL and OP_2DIV become available.
static constexpr int32_t CHRONICLE_HEIGHT = 943816; // src/chainparams.cpp: CHRONICLE_ACTIVATION_MAIN

/// UTXO height for all test cases — one block after Chronicle activation.
static constexpr int32_t UTXO_HEIGHT  = CHRONICLE_HEIGHT + 1;   // 943,817

/// Spending block height — well inside the post-Chronicle era.
static constexpr int32_t BLOCK_HEIGHT = 950000;

/// Value of every test UTXO (needed for the BIP143 sighash computation).
static const Amount UTXO_AMOUNT{1'000'000};   // 0.01 BSV

/// WIF private key used to sign and unlock all test transactions.
/// (same key used across the BDK example suite)
static const std::string WIF_KEY = "5HxWvvfubhXpYYpS3tJkw6fq9jE9j18THftkZjHHfmFiWtmAbrj";

// ═══════════════════════════════════════════════════════════════════════════
// Key helpers
// ═══════════════════════════════════════════════════════════════════════════

static CKey LoadKey()
{
    CBitcoinSecret secret;
    if (!secret.SetString(WIF_KEY))
        throw std::runtime_error("Failed to decode WIF key");
    return secret.GetKey();
}

// ═══════════════════════════════════════════════════════════════════════════
// Transaction-building helpers
// ═══════════════════════════════════════════════════════════════════════════

/// Create the *crediting* transaction — the UTXO that will be spent.
/// Its single output carries the scriptPubKey (opcode check + P2PKH).
static CMutableTransaction BuildCreditingTransaction(const CScript& scriptPubKey)
{
    CMutableTransaction tx;
    tx.nVersion  = 1;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vin[0].prevout   = COutPoint();                           // coinbase-style origin
    tx.vin[0].scriptSig = CScript() << CScriptNum(0) << CScriptNum(0);
    tx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    tx.vout[0].scriptPubKey = scriptPubKey;
    tx.vout[0].nValue       = UTXO_AMOUNT;
    return tx;
}

/// Create the *spending* transaction skeleton — input references the UTXO,
/// scriptSig is empty at this point (filled after signing).
static CMutableTransaction BuildSpendingTransaction(const CMutableTransaction& txCredit)
{
    CMutableTransaction tx;
    tx.nVersion  = 1;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vin[0].prevout   = COutPoint(txCredit.GetId(), 0);
    tx.vin[0].scriptSig = CScript();                             // filled after signing
    tx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    tx.vout[0].scriptPubKey = CScript();                         // anyone-can-spend output
    tx.vout[0].nValue       = UTXO_AMOUNT;
    return tx;
}

/// Sign the spending transaction and return the DER-encoded signature with
/// the sighash-type byte appended  (SIGHASH_ALL | SIGHASH_FORKID = 0x41).
static std::vector<uint8_t> SignSpendingTx(const CScript&              scriptPubKey,
                                            CMutableTransaction&         txSpend,
                                            const CKey&                  key)
{
    const SigHashType sigHashType = SigHashType().withForkId();
    const uint256 hash = SignatureHash(
        scriptPubKey, CTransaction(txSpend), /*nIn=*/0, sigHashType, UTXO_AMOUNT);

    std::vector<uint8_t> sig;
    if (!key.Sign(hash, sig))
        throw std::runtime_error("ECDSA signing failed");

    sig.push_back(static_cast<uint8_t>(sigHashType.getRawSigHashType()));
    return sig;
}

/// Full pipeline: build scriptPubKey, crediting tx, sign spending tx,
/// assemble scriptSig, return the wrapped extended transaction.
///
/// @param opcodeCheck   The opcode portion of scriptPubKey, ending with
///                      OP_EQUALVERIFY (consumes the opcode operands and
///                      verifies the result before the sig check runs).
/// @param operandPushes A pre-built CScript of push-only ops that supply
///                      the operands for the opcode, in bottom-to-top order.
///                      These are appended to the <sig> <pubkey> prefix in
///                      the final scriptSig.
static bsv::CMutableTransactionExtended MakeSignedExtendedTx(
    const CScript& opcodeCheck,
    const CScript& operandPushes,
    const CKey&    key)
{
    const CPubKey pubkey = key.GetPubKey();
    const CKeyID  keyID  = pubkey.GetID();

    // ── 1. Full scriptPubKey ──────────────────────────────────────────────
    // Opcode check (consumes operands, verifies result) followed by the
    // standard P2PKH signature check (verifies key ownership).
    CScript scriptPubKey = opcodeCheck;
    scriptPubKey << OP_DUP
                 << OP_HASH160    << ToByteVector(keyID)
                 << OP_EQUALVERIFY
                 << OP_CHECKSIG;

    // ── 2. Crediting transaction (the UTXO) ───────────────────────────────
    CMutableTransaction txCredit = BuildCreditingTransaction(scriptPubKey);

    // ── 3. Spending transaction skeleton (empty scriptSig) ────────────────
    CMutableTransaction txSpend = BuildSpendingTransaction(txCredit);

    // ── 4. Sign the spending transaction ──────────────────────────────────
    // The sighash covers scriptPubKey, the spending tx, input index 0, and
    // the UTXO amount (BIP143 / SIGHASH_ALL | SIGHASH_FORKID).
    const std::vector<uint8_t> sig = SignSpendingTx(scriptPubKey, txSpend, key);

    // ── 5. Assemble scriptSig: <sig> <pubkey> <operands> ──────────────────
    // Push order (bottom → top): sig, pubkey, then the opcode operands.
    // The scriptPubKey opcodes consume the operands first (from the top),
    // leaving sig and pubkey at the bottom for CHECKSIG.
    CScript scriptSig;
    scriptSig << sig << ToByteVector(pubkey);
    scriptSig.insert(scriptSig.end(), operandPushes.begin(), operandPushes.end());

    txSpend.vin[0].scriptSig = scriptSig;

    // ── 6. Wrap into extended transaction ─────────────────────────────────
    bsv::CMutableTransactionExtended eTX;
    eTX.vutxo = txCredit.vout;   // embed UTXO so VerifyScript can read it
    eTX.mtx   = txSpend;
    return eTX;
}

// ═══════════════════════════════════════════════════════════════════════════
// Opcode test-case descriptor
// ═══════════════════════════════════════════════════════════════════════════

struct OpcodeCase
{
    std::string key;            // CLI selector, e.g. "cat"
    std::string name;           // e.g. "OP_CAT"
    uint8_t     opcode_byte;    // e.g. 0x7E
    std::string description;    // one-liner of what the opcode does
    std::string script_comment; // documents scriptPubKey / scriptSig layout

    // Returns {opcodeCheck, operandPushes}.
    //
    // opcodeCheck   — the opcode + expected-result + OP_EQUALVERIFY fragment
    //                 that is prepended to the P2PKH check in scriptPubKey.
    // operandPushes — push-only CScript supplying the opcode operands,
    //                 appended after <sig> <pubkey> in scriptSig.
    std::function<std::pair<CScript, CScript>()> build_parts;
};

// ═══════════════════════════════════════════════════════════════════════════
// Script builders — one per opcode
// Each function returns {opcodeCheck, operandPushes}.
// ═══════════════════════════════════════════════════════════════════════════

// ── OP_CAT (0x7E) ──────────────────────────────────────────────────────────
// Pops two byte arrays, pushes their concatenation.
//
//   scriptSig    : <sig>  <pubkey>  <"BSV">  <"!">
//   scriptPubKey : OP_CAT  <"BSV!">  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace (scriptPubKey execution):
//     entry        : [sig, pubkey, "BSV", "!"]      ("!" on top)
//     OP_CAT       : [sig, pubkey, "BSV!"]
//     push "BSV!"  : [sig, pubkey, "BSV!", "BSV!"]
//     EQUALVERIFY  : [sig, pubkey]
//     ... P2PKH ...→ [1]   → valid
static std::pair<CScript, CScript> BuildOpCat()
{
    const std::vector<uint8_t> a{'B','S','V'};
    const std::vector<uint8_t> b{'!'};
    const std::vector<uint8_t> expected{'B','S','V','!'};

    CScript opcodeCheck;
    opcodeCheck << OP_CAT << expected << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << a << b;

    return {opcodeCheck, operandPushes};
}

// ── OP_SPLIT (0x7F) ────────────────────────────────────────────────────────
// Pops a byte array x and position n (top), pushes x[0..n) then x[n..).
//
//   scriptSig    : <sig>  <pubkey>  <{0xAB,0xCD}>  <1>
//   scriptPubKey : OP_SPLIT  <{0xCD}>  OP_EQUALVERIFY  <{0xAB}>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry           : [sig, pubkey, {0xAB,0xCD}, 1]   (1 on top)
//     OP_SPLIT pos=1  : [sig, pubkey, {0xAB}, {0xCD}]   ({0xCD} on top)
//     push {0xCD}     : [sig, pubkey, {0xAB}, {0xCD}, {0xCD}]
//     EQUALVERIFY     : [sig, pubkey, {0xAB}]
//     push {0xAB}     : [sig, pubkey, {0xAB}, {0xAB}]
//     EQUALVERIFY     : [sig, pubkey]
//     ... P2PKH ...  → [1]   → valid
static std::pair<CScript, CScript> BuildOpSplit()
{
    const std::vector<uint8_t> data {0xAB, 0xCD};
    const std::vector<uint8_t> left {0xAB};
    const std::vector<uint8_t> right{0xCD};

    CScript opcodeCheck;
    opcodeCheck << OP_SPLIT << right << OP_EQUALVERIFY << left << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << data << CScriptNum(1);   // split position on top

    return {opcodeCheck, operandPushes};
}

// ── OP_NUM2BIN (0x80) ──────────────────────────────────────────────────────
// Pops a target byte-width w (top) and a number n, pushes n zero-padded to
// w bytes in sign-magnitude little-endian encoding.
//
//   scriptSig    : <sig>  <pubkey>  <7>  <4>
//   scriptPubKey : OP_NUM2BIN  <{0x07,0x00,0x00,0x00}>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry           : [sig, pubkey, 7, 4]   (4 = target width, on top)
//     OP_NUM2BIN      : [sig, pubkey, {0x07,0x00,0x00,0x00}]
//     push expected   : [sig, pubkey, {0x07,…}, {0x07,…}]
//     EQUALVERIFY     : [sig, pubkey]
//     ... P2PKH ...  → [1]   → valid
static std::pair<CScript, CScript> BuildOpNum2Bin()
{
    // 7 in 4-byte little-endian: positive value, sign byte = 0x00
    const std::vector<uint8_t> expected{0x07, 0x00, 0x00, 0x00};

    CScript opcodeCheck;
    opcodeCheck << OP_NUM2BIN << expected << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << CScriptNum(7) << CScriptNum(4);   // width on top

    return {opcodeCheck, operandPushes};
}

// ── OP_BIN2NUM (0x81) ──────────────────────────────────────────────────────
// Pops a byte array, interprets it as sign-magnitude little-endian, pushes
// the minimally-encoded CScriptNum equivalent.
//
//   scriptSig    : <sig>  <pubkey>  <{0x07}>
//   scriptPubKey : OP_BIN2NUM  <7>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry          : [sig, pubkey, {0x07}]
//     OP_BIN2NUM     : [sig, pubkey, 7]   ({0x07} is already minimal)
//     push 7         : [sig, pubkey, 7, 7]
//     EQUALVERIFY    : [sig, pubkey]
//     ... P2PKH ... → [1]   → valid
static std::pair<CScript, CScript> BuildOpBin2Num()
{
    const std::vector<uint8_t> data{0x07};   // 7 in minimal little-endian

    CScript opcodeCheck;
    opcodeCheck << OP_BIN2NUM << CScriptNum(7) << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << data;

    return {opcodeCheck, operandPushes};
}

// ── OP_INVERT (0x83) ───────────────────────────────────────────────────────
// Pops a byte array, flips every bit, pushes the result.
//
//   scriptSig    : <sig>  <pubkey>  <{0xFF}>
//   scriptPubKey : OP_INVERT  <{0x00}>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry          : [sig, pubkey, {0xFF}]
//     OP_INVERT      : [sig, pubkey, {0x00}]   (~0xFF = 0x00)
//     push {0x00}    : [sig, pubkey, {0x00}, {0x00}]
//     EQUALVERIFY    : [sig, pubkey]
//     ... P2PKH ... → [1]   → valid
static std::pair<CScript, CScript> BuildOpInvert()
{
    const std::vector<uint8_t> input   {0xFF};
    const std::vector<uint8_t> expected{0x00};

    CScript opcodeCheck;
    opcodeCheck << OP_INVERT << expected << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << input;

    return {opcodeCheck, operandPushes};
}

// ── OP_AND (0x84) ──────────────────────────────────────────────────────────
// Pops two equal-length byte arrays, pushes their bitwise AND.
//
//   scriptSig    : <sig>  <pubkey>  <{0xFF}>  <{0x0F}>
//   scriptPubKey : OP_AND  <{0x0F}>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry          : [sig, pubkey, {0xFF}, {0x0F}]
//     OP_AND         : [sig, pubkey, {0x0F}]   (0xFF & 0x0F = 0x0F)
//     push {0x0F}    : [sig, pubkey, {0x0F}, {0x0F}]
//     EQUALVERIFY    : [sig, pubkey]
//     ... P2PKH ... → [1]   → valid
static std::pair<CScript, CScript> BuildOpAnd()
{
    const std::vector<uint8_t> a       {0xFF};
    const std::vector<uint8_t> b       {0x0F};
    const std::vector<uint8_t> expected{0x0F};   // 0xFF & 0x0F = 0x0F

    CScript opcodeCheck;
    opcodeCheck << OP_AND << expected << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << a << b;

    return {opcodeCheck, operandPushes};
}

// ── OP_OR (0x85) ───────────────────────────────────────────────────────────
// Pops two equal-length byte arrays, pushes their bitwise OR.
//
//   scriptSig    : <sig>  <pubkey>  <{0xF0}>  <{0x0F}>
//   scriptPubKey : OP_OR  <{0xFF}>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry          : [sig, pubkey, {0xF0}, {0x0F}]
//     OP_OR          : [sig, pubkey, {0xFF}]   (0xF0 | 0x0F = 0xFF)
//     push {0xFF}    : [sig, pubkey, {0xFF}, {0xFF}]
//     EQUALVERIFY    : [sig, pubkey]
//     ... P2PKH ... → [1]   → valid
static std::pair<CScript, CScript> BuildOpOr()
{
    const std::vector<uint8_t> a       {0xF0};
    const std::vector<uint8_t> b       {0x0F};
    const std::vector<uint8_t> expected{0xFF};   // 0xF0 | 0x0F = 0xFF

    CScript opcodeCheck;
    opcodeCheck << OP_OR << expected << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << a << b;

    return {opcodeCheck, operandPushes};
}

// ── OP_XOR (0x86) ──────────────────────────────────────────────────────────
// Pops two equal-length byte arrays, pushes their bitwise XOR.
//
//   scriptSig    : <sig>  <pubkey>  <{0xFF}>  <{0x0F}>
//   scriptPubKey : OP_XOR  <{0xF0}>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry          : [sig, pubkey, {0xFF}, {0x0F}]
//     OP_XOR         : [sig, pubkey, {0xF0}]   (0xFF ^ 0x0F = 0xF0)
//     push {0xF0}    : [sig, pubkey, {0xF0}, {0xF0}]
//     EQUALVERIFY    : [sig, pubkey]
//     ... P2PKH ... → [1]   → valid
static std::pair<CScript, CScript> BuildOpXor()
{
    const std::vector<uint8_t> a       {0xFF};
    const std::vector<uint8_t> b       {0x0F};
    const std::vector<uint8_t> expected{0xF0};   // 0xFF ^ 0x0F = 0xF0

    CScript opcodeCheck;
    opcodeCheck << OP_XOR << expected << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << a << b;

    return {opcodeCheck, operandPushes};
}

// ── OP_LSHIFT (0x98) ───────────────────────────────────────────────────────
// Pops a shift count n (CScriptNum, top) and a byte array x, pushes x
// shifted left by n bits.  Overflow bits are discarded; vacated LSBs = 0.
//
//   scriptSig    : <sig>  <pubkey>  <{0x01}>  <4>
//   scriptPubKey : OP_LSHIFT  <{0x10}>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry          : [sig, pubkey, {0x01}, 4]   (count 4 on top)
//     OP_LSHIFT      : [sig, pubkey, {0x10}]      (0x01 << 4 = 0x10)
//     push {0x10}    : [sig, pubkey, {0x10}, {0x10}]
//     EQUALVERIFY    : [sig, pubkey]
//     ... P2PKH ... → [1]   → valid
static std::pair<CScript, CScript> BuildOpLShift()
{
    const std::vector<uint8_t> data    {0x01};
    const std::vector<uint8_t> expected{0x10};   // 0x01 << 4 = 0x10

    CScript opcodeCheck;
    opcodeCheck << OP_LSHIFT << expected << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << data << CScriptNum(4);   // count on top

    return {opcodeCheck, operandPushes};
}

// ── OP_RSHIFT (0x99) ───────────────────────────────────────────────────────
// Pops a shift count n (CScriptNum, top) and a byte array x, pushes x
// shifted right by n bits.  Overflow bits are discarded; vacated MSBs = 0.
//
//   scriptSig    : <sig>  <pubkey>  <{0x10}>  <4>
//   scriptPubKey : OP_RSHIFT  <{0x01}>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry          : [sig, pubkey, {0x10}, 4]   (count 4 on top)
//     OP_RSHIFT      : [sig, pubkey, {0x01}]      (0x10 >> 4 = 0x01)
//     push {0x01}    : [sig, pubkey, {0x01}, {0x01}]
//     EQUALVERIFY    : [sig, pubkey]
//     ... P2PKH ... → [1]   → valid
static std::pair<CScript, CScript> BuildOpRShift()
{
    const std::vector<uint8_t> data    {0x10};
    const std::vector<uint8_t> expected{0x01};   // 0x10 >> 4 = 0x01

    CScript opcodeCheck;
    opcodeCheck << OP_RSHIFT << expected << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << data << CScriptNum(4);   // count on top

    return {opcodeCheck, operandPushes};
}

// ── OP_MUL (0x95) ──────────────────────────────────────────────────────────
// Pops two CScriptNums, pushes their product.
//
//   scriptSig    : <sig>  <pubkey>  <3>  <4>
//   scriptPubKey : OP_MUL  <12>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry       : [sig, pubkey, 3, 4]
//     OP_MUL      : [sig, pubkey, 12]
//     push 12     : [sig, pubkey, 12, 12]
//     EQUALVERIFY : [sig, pubkey]
//     ... P2PKH →   [1]   → valid
static std::pair<CScript, CScript> BuildOpMul()
{
    CScript opcodeCheck;
    opcodeCheck << OP_MUL << CScriptNum(12) << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << CScriptNum(3) << CScriptNum(4);

    return {opcodeCheck, operandPushes};
}

// ── OP_DIV (0x96) ──────────────────────────────────────────────────────────
// Pops two CScriptNums a (second) and b (top), pushes a / b truncated toward 0.
//
//   scriptSig    : <sig>  <pubkey>  <12>  <3>
//   scriptPubKey : OP_DIV  <4>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry       : [sig, pubkey, 12, 3]   (3 = divisor, on top)
//     OP_DIV      : [sig, pubkey, 4]       12 / 3 = 4
//     push 4      : [sig, pubkey, 4, 4]
//     EQUALVERIFY : [sig, pubkey]
//     ... P2PKH →   [1]   → valid
static std::pair<CScript, CScript> BuildOpDiv()
{
    CScript opcodeCheck;
    opcodeCheck << OP_DIV << CScriptNum(4) << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << CScriptNum(12) << CScriptNum(3);   // divisor on top

    return {opcodeCheck, operandPushes};
}

// ── OP_MOD (0x97) ──────────────────────────────────────────────────────────
// Pops two CScriptNums a (second) and b (top), pushes a mod b.
//
//   scriptSig    : <sig>  <pubkey>  <13>  <5>
//   scriptPubKey : OP_MOD  <3>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry       : [sig, pubkey, 13, 5]   (5 = divisor, on top)
//     OP_MOD      : [sig, pubkey, 3]       13 % 5 = 3
//     push 3      : [sig, pubkey, 3, 3]
//     EQUALVERIFY : [sig, pubkey]
//     ... P2PKH →   [1]   → valid
static std::pair<CScript, CScript> BuildOpMod()
{
    CScript opcodeCheck;
    opcodeCheck << OP_MOD << CScriptNum(3) << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << CScriptNum(13) << CScriptNum(5);   // divisor on top

    return {opcodeCheck, operandPushes};
}

// ── OP_2MUL (0x8D) ─────────────────────────────────────────────────────────
// Pops a CScriptNum, pushes its value multiplied by 2.
// Requires post-Chronicle UTXO (Chronicle mainnet activation: block 943,816).
//
//   scriptSig    : <sig>  <pubkey>  <5>
//   scriptPubKey : OP_2MUL  <10>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry       : [sig, pubkey, 5]
//     OP_2MUL     : [sig, pubkey, 10]
//     push 10     : [sig, pubkey, 10, 10]
//     EQUALVERIFY : [sig, pubkey]
//     ... P2PKH →   [1]   → valid
static std::pair<CScript, CScript> BuildOp2Mul()
{
    CScript opcodeCheck;
    opcodeCheck << OP_2MUL << CScriptNum(10) << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << CScriptNum(5);

    return {opcodeCheck, operandPushes};
}

// ── OP_2DIV (0x8E) ─────────────────────────────────────────────────────────
// Pops a CScriptNum, pushes its value divided by 2 (truncated toward zero).
// Requires post-Chronicle UTXO (Chronicle mainnet activation: block 943,816).
//
//   scriptSig    : <sig>  <pubkey>  <10>
//   scriptPubKey : OP_2DIV  <5>  OP_EQUALVERIFY
//                  OP_DUP  OP_HASH160  <hash>  OP_EQUALVERIFY  OP_CHECKSIG
//
//   Stack trace:
//     entry       : [sig, pubkey, 10]
//     OP_2DIV     : [sig, pubkey, 5]
//     push 5      : [sig, pubkey, 5, 5]
//     EQUALVERIFY : [sig, pubkey]
//     ... P2PKH →   [1]   → valid
static std::pair<CScript, CScript> BuildOp2Div()
{
    CScript opcodeCheck;
    opcodeCheck << OP_2DIV << CScriptNum(5) << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << CScriptNum(10);

    return {opcodeCheck, operandPushes};
}

// ═══════════════════════════════════════════════════════════════════════════
// Test-case table
// ═══════════════════════════════════════════════════════════════════════════

static const std::vector<OpcodeCase> kCases = {
    // ── String / byte-array opcodes ─────────────────────────────────────
    {"cat",    "OP_CAT",    0x7E, "Concatenate two byte arrays",
     "scriptSig: <sig> <pk> <\"BSV\"> <\"!\">  |  scriptPubKey: OP_CAT <\"BSV!\"> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpCat},

    {"split",  "OP_SPLIT",  0x7F, "Split byte array at position n",
     "scriptSig: <sig> <pk> <{AB,CD}> <1>  |  scriptPubKey: OP_SPLIT <{CD}> EQUALVERIFY <{AB}> EQUALVERIFY ... OP_CHECKSIG",
     BuildOpSplit},

    {"num2bin","OP_NUM2BIN",0x80, "Encode integer as fixed-width little-endian bytes",
     "scriptSig: <sig> <pk> <7> <4>  |  scriptPubKey: OP_NUM2BIN <{07,00,00,00}> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpNum2Bin},

    {"bin2num","OP_BIN2NUM",0x81, "Decode little-endian byte array to minimal CScriptNum",
     "scriptSig: <sig> <pk> <{07}>  |  scriptPubKey: OP_BIN2NUM <7> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpBin2Num},

    // ── Bitwise opcodes ──────────────────────────────────────────────────
    {"invert", "OP_INVERT", 0x83, "Bitwise NOT of byte array",
     "scriptSig: <sig> <pk> <{FF}>  |  scriptPubKey: OP_INVERT <{00}> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpInvert},

    {"and",    "OP_AND",    0x84, "Bitwise AND of two equal-length byte arrays",
     "scriptSig: <sig> <pk> <{FF}> <{0F}>  |  scriptPubKey: OP_AND <{0F}> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpAnd},

    {"or",     "OP_OR",     0x85, "Bitwise OR of two equal-length byte arrays",
     "scriptSig: <sig> <pk> <{F0}> <{0F}>  |  scriptPubKey: OP_OR <{FF}> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpOr},

    {"xor",    "OP_XOR",    0x86, "Bitwise XOR of two equal-length byte arrays",
     "scriptSig: <sig> <pk> <{FF}> <{0F}>  |  scriptPubKey: OP_XOR <{F0}> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpXor},

    {"lshift", "OP_LSHIFT", 0x98, "Shift byte array left by n bits",
     "scriptSig: <sig> <pk> <{01}> <4>  |  scriptPubKey: OP_LSHIFT <{10}> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpLShift},

    {"rshift", "OP_RSHIFT", 0x99, "Shift byte array right by n bits",
     "scriptSig: <sig> <pk> <{10}> <4>  |  scriptPubKey: OP_RSHIFT <{01}> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpRShift},

    // ── Arithmetic opcodes ───────────────────────────────────────────────
    {"mul",    "OP_MUL",    0x95, "Multiply two integers",
     "scriptSig: <sig> <pk> <3> <4>  |  scriptPubKey: OP_MUL <12> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpMul},

    {"div",    "OP_DIV",    0x96, "Integer division (truncated toward zero)",
     "scriptSig: <sig> <pk> <12> <3>  |  scriptPubKey: OP_DIV <4> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpDiv},

    {"mod",    "OP_MOD",    0x97, "Integer modulo",
     "scriptSig: <sig> <pk> <13> <5>  |  scriptPubKey: OP_MOD <3> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOpMod},

    // ── Chronicle-only opcodes ───────────────────────────────────────────
    {"2mul",   "OP_2MUL",   0x8D, "Multiply integer by 2  [Chronicle, block 943,816]",
     "scriptSig: <sig> <pk> <5>  |  scriptPubKey: OP_2MUL <10> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOp2Mul},

    {"2div",   "OP_2DIV",   0x8E, "Integer divide by 2  [Chronicle, block 943,816]",
     "scriptSig: <sig> <pk> <10>  |  scriptPubKey: OP_2DIV <5> OP_EQUALVERIFY ... OP_CHECKSIG",
     BuildOp2Div},
};

// Index kCases by key for O(1) lookup when a single opcode is requested.
static std::map<std::string, const OpcodeCase*> BuildCaseIndex()
{
    std::map<std::string, const OpcodeCase*> idx;
    for (const auto& tc : kCases)
        idx[tc.key] = &tc;
    return idx;
}
static const std::map<std::string, const OpcodeCase*> kCaseIndex = BuildCaseIndex();

// ═══════════════════════════════════════════════════════════════════════════
// RunCase — executes exactly one opcode test case
// ═══════════════════════════════════════════════════════════════════════════
// This is the single unit of work, shared identically by both the "all"
// loop and a single-opcode run.  Returns true on VALID, false on INVALID.

static bool RunCase(const OpcodeCase&               tc,
                    size_t                           index,
                    size_t                           total,
                    const CKey&                      key,
                    const bsv::CScriptEngine&        se,
                    const std::array<int32_t, 1>&    utxoHeights)
{
    std::cout << "[" << std::setw(2) << index << "/" << total << "]  "
              << tc.name << "  (0x"
              << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(tc.opcode_byte)
              << std::dec << std::setfill(' ') << ")\n"
              << "       " << tc.description << "\n"
              << "       " << tc.script_comment << "\n";

    // Build opcode-specific script fragments
    auto [opcodeCheck, operandPushes] = tc.build_parts();

    // Assemble, sign, and wrap into an extended transaction
    const bsv::CMutableTransactionExtended eTX =
        MakeSignedExtendedTx(opcodeCheck, operandPushes, key);

    // Serialise to raw bytes
    CDataStream out(SER_NETWORK, PROTOCOL_VERSION);
    out << eTX;
    const std::vector<uint8_t> etxBin(out.begin(), out.end());

    // Verify in post-Chronicle consensus context
    const ScriptError ret = se.VerifyScript(
        etxBin,
        std::span<const int32_t>(utxoHeights),
        BLOCK_HEIGHT,
        /*consensus=*/true);

    if (ret == SCRIPT_ERR_OK)
    {
        const std::string etxHex = bsv::Bin2Hex(etxBin);
        std::cout << "       Result           : VALID (SCRIPT_ERR_OK)\n"
                  << "       ETX size         : " << etxBin.size() << " bytes\n"
                  << "       ETX hex          : " << etxHex << "\n\n";
        return true;
    }

    std::cout << "       Result           : INVALID (ScriptError = "
              << static_cast<int>(ret) << ")\n\n";
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[])
{
    // ── CLI ──────────────────────────────────────────────────────────────
    // Build the list of valid keys for the help string.
    std::string keyList = "all";
    for (const auto& tc : kCases)
        keyList += ", " + tc.key;

    std::string opcodeKey = "all";

    po::options_description desc("Special opcode transaction verifier");
    desc.add_options()
        ("help,h", "Show this help message")
        ("opcode,o",
            po::value<std::string>(&opcodeKey)->default_value("all"),
            ("Opcode to test (default: all).  Valid values: " + keyList).c_str());

    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) { std::cout << desc << "\n"; return 0; }
        po::notify(vm);
    }
    catch (const po::error& e)
    {
        std::cerr << "Argument error: " << e.what() << "\n" << desc << "\n";
        return 1;
    }

    // Validate the selector (unless it is "all")
    if (opcodeKey != "all" && kCaseIndex.find(opcodeKey) == kCaseIndex.end())
    {
        std::cerr << "Unknown opcode key '" << opcodeKey << "'.\n" << desc << "\n";
        return 1;
    }

    // ── Setup ─────────────────────────────────────────────────────────────
    const std::string network = CBaseChainParams::MAIN;
    SelectParams(network, std::nullopt);

    const CKey key = LoadKey();
    const bsv::CScriptEngine se(network);
    const std::array<int32_t, 1> utxoHeights = {UTXO_HEIGHT};

    // ── Header ───────────────────────────────────────────────────────────
    const bool runAll = (opcodeKey == "all");
    const size_t total = runAll ? kCases.size() : 1;

    std::cout << std::string(70, '=') << "\n"
              << "  Special Opcode Verification — real-world signed transactions\n"
              << std::string(70, '=') << "\n"
              << "  Selection            : " << (runAll ? "all (" + std::to_string(total) + " opcodes)" : opcodeKey) << "\n"
              << "  Genesis activation   : block " << GENESIS_HEIGHT  << " (mainnet)\n"
              << "  Chronicle activation : block " << CHRONICLE_HEIGHT << " (mainnet)\n"
              << "  UTXO height          : " << UTXO_HEIGHT  << "  (post-Chronicle)\n"
              << "  Block height         : " << BLOCK_HEIGHT << "  (post-Chronicle)\n"
              << "  Consensus mode       : true  (block-validation, not mempool)\n"
              << "  Network              : " << network << "\n"
              << "  Sighash type         : SIGHASH_ALL | SIGHASH_FORKID (0x41)\n"
              << "  Signing key (WIF)    : " << WIF_KEY << "\n"
              << std::string(70, '=') << "\n\n";

    // ── Execute ───────────────────────────────────────────────────────────
    int passed = 0;
    int failed = 0;

    if (runAll)
    {
        // Loop through every case; each iteration calls RunCase identically
        // to how a single-opcode run would call it.
        for (size_t i = 0; i < kCases.size(); ++i)
        {
            if (RunCase(kCases[i], i + 1, total, key, se, utxoHeights))
                ++passed;
            else
                ++failed;
        }
    }
    else
    {
        // Single-opcode run — same RunCase call, index shown as 1/1.
        if (RunCase(*kCaseIndex.at(opcodeKey), 1, 1, key, se, utxoHeights))
            ++passed;
        else
            ++failed;
    }

    // ── Summary ──────────────────────────────────────────────────────────
    std::cout << std::string(70, '=') << "\n"
              << "  Summary\n"
              << std::string(70, '=') << "\n"
              << "  Passed : " << passed << " / " << total << "\n"
              << "  Failed : " << failed << " / " << total << "\n"
              << "  UTXO height  : " << UTXO_HEIGHT
              << "  (post-Genesis: "   << (UTXO_HEIGHT > GENESIS_HEIGHT   ? "yes" : "no") << ", "
              << "post-Chronicle: "    << (UTXO_HEIGHT > CHRONICLE_HEIGHT ? "yes" : "no") << ")\n"
              << "  Block height : " << BLOCK_HEIGHT
              << "  (post-Chronicle: " << (BLOCK_HEIGHT > CHRONICLE_HEIGHT ? "yes" : "no") << ")\n"
              << "  Consensus    : true\n"
              << "  Network      : " << network << "\n"
              << std::string(70, '=') << "\n";

    return (failed == 0) ? 0 : 1;
}
