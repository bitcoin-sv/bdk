/// test_chronicle.cpp
///
/// Boost.Test suite verifying the behavioral changes introduced by the BSV
/// Chronicle protocol upgrade (mainnet activation block 943,816).
///
/// Each test creates real-world transactions (a crediting tx whose output is
/// the UTXO, and a spending tx that unlocks it) wrapped into a
/// CMutableTransactionExtended, then calls se.VerifyScript() under both
/// pre-Chronicle and post-Chronicle conditions to highlight the change.
///
/// File layout
///   1. Includes / module name
///   2. Block-height constants
///   3. Helper forward declarations   ← skim once; details at end of file
///   4. Test cases                    ← main reading area
///   5. Helper definitions            ← implementation details, bottom of file

#ifdef NDEBUG
#define BOOST_TEST_MODULE test_chronicle
#else
#define BOOST_TEST_MODULE test_chronicled
#endif

#include <array>
#include <vector>

#include <boost/test/unit_test.hpp>

#include "base58.h"
#include "chainparams.h"
#include "key.h"
#include "script/opcodes.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/script_flags.h"
#include "script/script_num.h"
#include "script/sign.h"
#include "script/sighashtype.h"
#include "util.h"

#include "extendedTx.hpp"
#include "txvalidator.hpp"

// ═══════════════════════════════════════════════════════════════════════════
// Block-height constants  (mainnet)
// ═══════════════════════════════════════════════════════════════════════════

/// Genesis activated at block 620,538.  A UTXO mined at height ≥ 620,539
/// has SCRIPT_UTXO_AFTER_GENESIS set; required to allow large script numbers
/// and other Genesis opcodes.
static constexpr int32_t GENESIS_HEIGHT          = 620538;

/// Chronicle activated at block 943,816.
/// • Spending block height ≥ 943,817 → SCRIPT_CHRONICLE is set in block flags
///   (affects SIGHASH_CHRONICLE validation, push-only scriptSig rule).
/// • UTXO mined at height ≥ 943,817 → SCRIPT_UTXO_AFTER_CHRONICLE is set
///   (gates opcode reactivation per input).
static constexpr int32_t CHRONICLE_HEIGHT        = 943816;

static constexpr int32_t PRE_GENESIS_UTXO_HEIGHT    = GENESIS_HEIGHT  - 1;
static constexpr int32_t POST_GENESIS_UTXO_HEIGHT   = GENESIS_HEIGHT  + 1;   // 620,539
static constexpr int32_t PRE_CHRONICLE_UTXO_HEIGHT  = CHRONICLE_HEIGHT - 1;  // 943,815
static constexpr int32_t POST_CHRONICLE_UTXO_HEIGHT = CHRONICLE_HEIGHT + 1;  // 943,817

static constexpr int32_t PRE_CHRONICLE_BLOCK_HEIGHT  = CHRONICLE_HEIGHT - 1; // 943,815
static constexpr int32_t POST_CHRONICLE_BLOCK_HEIGHT = 950000;

/// UTXO value used in every test transaction (needed for BIP143 sighash).
static const Amount UTXO_AMOUNT{1'000'000};   // 0.01 BSV

/// WIF private key shared across all tests.
static const std::string WIF_KEY = "5HxWvvfubhXpYYpS3tJkw6fq9jE9j18THftkZjHHfmFiWtmAbrj";

// ═══════════════════════════════════════════════════════════════════════════
// Policy-path flags for PostGenesis / pre-Chronicle era
// ═══════════════════════════════════════════════════════════════════════════
//
// These are the exact flags that GetScriptVerifyFlags(PostGenesis) returns
// (= StandardScriptVerifyFlags(PostGenesis)) combined with the per-input
// flags from InputScriptVerifyFlags(PostGenesis era, PostGenesis utxo).
//
// Notably this set includes SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS, which
// is absent from GetBlockScriptFlags (the consensus path) for the same era.
// Passing these flags via VerifyWithFlags (which uses customFlags + consensus=true)
// exercises the interpreter's DISCOURAGE check without triggering the
// standardness pre-filter that would normally run for non-standard scripts.
//
// Source:
//   src/policy/policy.h  : PRE_CHRONICLE_STANDARD_SCRIPT_VERIFY_FLAGS
//   src/policy/policy.h  : StandardScriptVerifyFlags()
//   src/policy/policy.h  : InputScriptVerifyFlags()
static constexpr uint32_t POLICY_FLAGS_PRE_CHRONICLE =
    SCRIPT_VERIFY_P2SH                       |  // (1U <<  0) mandatory
    SCRIPT_VERIFY_STRICTENC                  |  // (1U <<  1) mandatory
    SCRIPT_VERIFY_DERSIG                     |  // (1U <<  2) standard
    SCRIPT_VERIFY_LOW_S                      |  // (1U <<  3) mandatory
    SCRIPT_VERIFY_NULLDUMMY                  |  // (1U <<  4) standard
    SCRIPT_VERIFY_SIGPUSHONLY                |  // (1U <<  5) Genesis per-input
    SCRIPT_VERIFY_MINIMALDATA                |  // (1U <<  6) standard
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS |  // (1U <<  7) standard (key flag)
    SCRIPT_VERIFY_CLEANSTACK                 |  // (1U <<  8) standard
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY        |  // (1U <<  9) standard
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY        |  // (1U << 10) standard
    SCRIPT_VERIFY_NULLFAIL                   |  // (1U << 14) mandatory
    SCRIPT_ENABLE_SIGHASH_FORKID             |  // (1U << 16) mandatory
    SCRIPT_GENESIS                           |  // (1U << 18) Genesis block flag
    SCRIPT_UTXO_AFTER_GENESIS;                  // (1U << 19) Genesis UTXO flag

// ═══════════════════════════════════════════════════════════════════════════
// Helper forward declarations
// ═══════════════════════════════════════════════════════════════════════════

static CKey LoadKey();

static CMutableTransaction BuildCreditingTx(const CScript& scriptPubKey);

// Build a spending tx skeleton: input references credit tx output 0,
// scriptSig is left empty for the caller to fill, output is anyone-can-spend.
// txVersion controls nVersion on the spending tx.
static CMutableTransaction BuildSpendingTx(const CMutableTransaction& txCredit,
                                            int32_t txVersion = 1);

// Compute DER sig + sighash byte for input 0 of txSpend.
static std::vector<uint8_t> Sign(const CScript& scriptPubKey,
                                  CMutableTransaction& txSpend,
                                  const CKey& key,
                                  SigHashType sigHashType = SigHashType().withForkId());

// Standard "opcode + P2PKH" extended transaction builder (mirrors
// example_special_opcode_verify.cpp MakeSignedExtendedTx).
//
// scriptPubKey assembled as:  opcodeCheck  OP_DUP OP_HASH160 <keyID>
//                              OP_EQUALVERIFY OP_CHECKSIG
// scriptSig assembled as:     <sig> <pubkey> [operandPushes]
//
// Operands for the opcode must be in operandPushes in bottom-to-top order
// (they end up on top of the stack when scriptPubKey begins executing).
static bsv::CMutableTransactionExtended MakeSignedExtendedTx(
    const CScript& opcodeCheck,
    const CScript& operandPushes,
    const CKey& key,
    int32_t txVersion = 1);

// Serialize eTX to binary (BIP-239 extended format) and call VerifyScript.
static TxError Verify(const bsv::CMutableTransactionExtended& eTX,
                      int32_t utxoHeight,
                      int32_t blockHeight,
                      bool consensus,
                      bsv::CTxValidator& se);

// Verify eTX with an explicit script flags word, bypassing both the automatic
// flag-computation logic and the standardness pre-check.
static TxError VerifyWithFlags(const bsv::CMutableTransactionExtended& eTX,
                               int32_t blockHeight,
                               uint32_t flags,
                               bsv::CTxValidator& se);

// Returns true if r carries the given ScriptError_t (OK maps to domain OK).
static bool txErrorIsScript(TxError r, ScriptError_t e);

// ═══════════════════════════════════════════════════════════════════════════
// Test suite
// ═══════════════════════════════════════════════════════════════════════════

// CTxValidator("main") initialises its own internal CChainParams but does NOT
// set the global chain-params singleton used by CBitcoinSecret / CKey address
// functions (Params()).  The fixture calls SelectParams once before each test
// so that LoadKey() and any address-related helpers can access global params.
struct ChainParamsFixture
{
    ChainParamsFixture() { SelectParams(CBaseChainParams::MAIN, std::nullopt); }
};

BOOST_FIXTURE_TEST_SUITE(test_chronicle, ChainParamsFixture)

// ── Test 1 ─────────────────────────────────────────────────────────────────
// Push-only scriptSig rule — Chronicle makes it version-dependent
// ───────────────────────────────────────────────────────────────────────────
// The UTXO is locked with a simple equality check: the spender must present
// a value equal to 5 on the stack.  The spending scriptSig computes that
// value using OP_ADD (a non-push opcode): <2> <3> OP_ADD → 5.
//
// Pre-Chronicle (any tx version):
//   SCRIPT_CHRONICLE is absent → push-only is always enforced →
//   OP_ADD in scriptSig → SCRIPT_ERR_SIG_PUSHONLY.
//
// Post-Chronicle, tx version = 1 (non-malleable):
//   SCRIPT_CHRONICLE is set but version == 1 → push-only still enforced →
//   SCRIPT_ERR_SIG_PUSHONLY.
//
// Post-Chronicle, tx version = 2 (malleable):
//   SCRIPT_CHRONICLE is set and version > 1 → push-only NOT enforced →
//   OP_ADD runs in scriptSig → stack = [5] → OP_5 OP_EQUAL → PASS.
//
// Applies to: both policy and consensus (tested here under consensus = true).
// Reference: src/script/interpreter.cpp:2306-2319
BOOST_AUTO_TEST_CASE(test_push_only_scriptsig)
{
    bsv::CTxValidator se("main");
    const CKey key = LoadKey();

    // Lock the UTXO: top of stack must equal 5.
    CScript scriptPubKey;
    scriptPubKey << CScriptNum(5) << OP_EQUAL;

    // Non-push scriptSig: computes 2 + 3 = 5.
    CScript scriptSig;
    scriptSig << CScriptNum(2) << CScriptNum(3) << OP_ADD;

    CMutableTransaction txCredit = BuildCreditingTx(scriptPubKey);
    // version = 1 (non-malleable) for pre-Chronicle and the first post-Chronicle sub-case
    CMutableTransaction txSpendV1 = BuildSpendingTx(txCredit, 1);
    txSpendV1.vin[0].scriptSig = scriptSig;
    // version = 2 (malleable) for the permissive post-Chronicle sub-case
    CMutableTransaction txSpendV2 = BuildSpendingTx(txCredit, 2);
    txSpendV2.vin[0].scriptSig = scriptSig;

    bsv::CMutableTransactionExtended eTX_V1, eTX_V2;
    eTX_V1.vutxo = txCredit.vout;  eTX_V1.mtx = txSpendV1;
    eTX_V2.vutxo = txCredit.vout;  eTX_V2.mtx = txSpendV2;

    // UTXO is post-Genesis so OP_ADD is allowed at the consensus level.
    const int32_t utxoH = POST_GENESIS_UTXO_HEIGHT;

    // Pre-Chronicle block: push-only always enforced regardless of tx version.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX_V1, utxoH, PRE_CHRONICLE_BLOCK_HEIGHT, /*consensus=*/true, se),
        SCRIPT_ERR_SIG_PUSHONLY));
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX_V2, utxoH, PRE_CHRONICLE_BLOCK_HEIGHT, /*consensus=*/true, se),
        SCRIPT_ERR_SIG_PUSHONLY));

    // Post-Chronicle block, version = 1: push-only still enforced.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX_V1, utxoH, POST_CHRONICLE_BLOCK_HEIGHT, /*consensus=*/true, se),
        SCRIPT_ERR_SIG_PUSHONLY));

    // Post-Chronicle block, version = 2: push-only lifted for malleable txns.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX_V2, utxoH, POST_CHRONICLE_BLOCK_HEIGHT, /*consensus=*/true, se),
        SCRIPT_ERR_OK));
}

// ── Test 2 ─────────────────────────────────────────────────────────────────
// SIGHASH_CHRONICLE — illegal before Chronicle, allowed after
// ───────────────────────────────────────────────────────────────────────────
// A standard P2PKH UTXO is created and spent with a signature that carries
// the SIGHASH_CHRONICLE modifier (0x20).  The engine checks this modifier
// against the SCRIPT_CHRONICLE block flag before validating the signature.
//
// Pre-Chronicle block (SCRIPT_CHRONICLE absent):
//   → SCRIPT_ERR_ILLEGAL_CHRONICLE
//
// Post-Chronicle block (SCRIPT_CHRONICLE present):
//   → SCRIPT_ERR_OK
//
// Applies to: both policy and consensus (tested here under consensus = true).
// Reference: src/script/interpreter.cpp:293-300, src/script/sighashtype.h:58-61
BOOST_AUTO_TEST_CASE(test_sighash_chronicle)
{
    bsv::CTxValidator se("main");
    const CKey key    = LoadKey();
    const CPubKey pub = key.GetPubKey();

    // Standard P2PKH locking script.
    CScript scriptPubKey;
    scriptPubKey << OP_DUP << OP_HASH160
                 << ToByteVector(pub.GetID())
                 << OP_EQUALVERIFY << OP_CHECKSIG;

    CMutableTransaction txCredit = BuildCreditingTx(scriptPubKey);
    CMutableTransaction txSpend  = BuildSpendingTx(txCredit);

    // Sign with SIGHASH_ALL | SIGHASH_FORKID | SIGHASH_CHRONICLE (= 0x61).
    const SigHashType chronicleSigHash = SigHashType().withForkId().withChronicle();
    const auto sig = Sign(scriptPubKey, txSpend, key, chronicleSigHash);

    txSpend.vin[0].scriptSig = CScript() << sig << ToByteVector(pub);

    bsv::CMutableTransactionExtended eTX;
    eTX.vutxo = txCredit.vout;
    eTX.mtx   = txSpend;

    // UTXO is post-Genesis (chronicle modifier does not depend on UTXO era).
    const int32_t utxoH = POST_GENESIS_UTXO_HEIGHT;

    // Pre-Chronicle block: SCRIPT_CHRONICLE not set → modifier rejected.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, utxoH, PRE_CHRONICLE_BLOCK_HEIGHT, /*consensus=*/true, se),
        SCRIPT_ERR_ILLEGAL_CHRONICLE));

    // Post-Chronicle block: SCRIPT_CHRONICLE set → modifier accepted.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, utxoH, POST_CHRONICLE_BLOCK_HEIGHT, /*consensus=*/true, se),
        SCRIPT_ERR_OK));
}

// ── Test 3 ─────────────────────────────────────────────────────────────────
// Script number length — consensus limit changes across protocol eras
// ───────────────────────────────────────────────────────────────────────────
// The consensus limit for CScriptNum is:
//   pre-Genesis  →  4 bytes     (classic Bitcoin limit)
//   post-Genesis → 750 KB       (raised by Genesis upgrade)
//   post-Chronicle → 32 MB      (raised by Chronicle upgrade)
//
// This test demonstrates the pre-Genesis vs post-Genesis boundary using
// a 5-byte operand, which exceeds the 4-byte pre-Genesis limit but is well
// within the 750 KB post-Genesis limit.  Testing at consensus level
// (consensus = true) avoids the standardness checks of the policy path and
// directly exercises the limit enforced inside the script interpreter.
//
// The transaction locks coins behind a simple arithmetic check:
//   scriptSig:    <5-byte CScriptNum N>
//   scriptPubKey: OP_1ADD OP_DROP OP_1
// OP_1ADD tries to read N as a CScriptNum.  If N exceeds the consensus limit
// the engine returns SCRIPT_ERR_SCRIPTNUM_OVERFLOW; otherwise it computes N+1,
// drops the result, and leaves 1 (truthy) on the stack.
//
// Pre-Genesis UTXO (consensus limit = 4 bytes): 5-byte N rejected.
// Post-Genesis UTXO (consensus limit = 750 KB):  5-byte N accepted.
//
// Applies to: both policy and consensus.  By default the policy limit equals
// the consensus limit for any given era (maxScriptNumLengthPolicy is unset),
// so the era-based boundary demonstrated here applies equally in both paths.
// The test uses consensus = true to avoid the standardness pre-filter, which
// would reject the non-standard scriptPubKey before the interpreter runs.
// Reference: src/configscriptpolicy.cpp:77-108, src/consensus/consensus.h:62-66
BOOST_AUTO_TEST_CASE(test_script_num_length)
{
    bsv::CTxValidator se("main");

    // 5-byte minimal positive CScriptNum encoding.
    // Little-endian sign-magnitude: {0x00×4, 0x01} = value 2^32.
    // Minimal because the last byte (0x01) has non-zero magnitude bits (0x01 & 0x7f != 0).
    // 5 bytes > 4-byte pre-Genesis consensus limit → overflow pre-Genesis.
    // 5 bytes << 750 KB post-Genesis consensus limit → accepted post-Genesis.
    const std::vector<uint8_t> fiveByteNum{0x00, 0x00, 0x00, 0x00, 0x01};

    // OP_1ADD interprets the top of stack as a CScriptNum (triggers the limit check),
    // then increments.  OP_DROP discards the result.  OP_1 leaves true on the stack.
    CScript scriptPubKey;
    scriptPubKey << OP_1ADD << OP_DROP << OP_1;

    CScript scriptSig;
    scriptSig << fiveByteNum;

    CMutableTransaction txCredit = BuildCreditingTx(scriptPubKey);
    CMutableTransaction txSpend  = BuildSpendingTx(txCredit);
    txSpend.vin[0].scriptSig     = scriptSig;

    bsv::CMutableTransactionExtended eTX;
    eTX.vutxo = txCredit.vout;
    eTX.mtx   = txSpend;

    // Block height post-Genesis so SCRIPT_GENESIS is set in block flags.
    const int32_t blockH = POST_GENESIS_UTXO_HEIGHT;

    // Pre-Genesis UTXO: consensus limit = 4 bytes; 5-byte operand rejected.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, PRE_GENESIS_UTXO_HEIGHT, blockH, /*consensus=*/true, se),
        SCRIPT_ERR_SCRIPTNUM_OVERFLOW));

    // Post-Genesis UTXO: consensus limit = 750 KB; 5-byte operand accepted.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, POST_GENESIS_UTXO_HEIGHT, blockH, /*consensus=*/true, se),
        SCRIPT_ERR_OK));
}

// ── Test 4 ─────────────────────────────────────────────────────────────────
// OP_VER (0x62) — Chronicle reactivates it to push the transaction version
// ───────────────────────────────────────────────────────────────────────────
// The UTXO is locked so the spender must prove the spending transaction has
// version 1.  OP_VER pushes the version as a 4-byte little-endian integer;
// the locking script then compares it with the hardcoded expected bytes.
//
//   scriptSig:    <sig> <pubkey>           (no extra operands; OP_VER uses none)
//   scriptPubKey: OP_VER <{01,00,00,00}> OP_EQUALVERIFY
//                 OP_DUP OP_HASH160 <keyID> OP_EQUALVERIFY OP_CHECKSIG
//
// Pre-Chronicle UTXO (SCRIPT_UTXO_AFTER_CHRONICLE absent):
//   OP_VER → SCRIPT_ERR_BAD_OPCODE
//
// Post-Chronicle UTXO (SCRIPT_UTXO_AFTER_CHRONICLE present):
//   OP_VER pushes {0x01, 0x00, 0x00, 0x00} → EQUALVERIFY passes → CHECKSIG → OK
//
// Applies to: both policy and consensus (tested here under consensus = true).
// Reference: src/script/interpreter.cpp:596-606
BOOST_AUTO_TEST_CASE(test_op_ver)
{
    bsv::CTxValidator se("main");
    const CKey key = LoadKey();

    // Expected 4-byte little-endian encoding of tx version 1.
    const std::vector<uint8_t> version1LE{0x01, 0x00, 0x00, 0x00};

    CScript opcodeCheck;
    opcodeCheck << OP_VER << version1LE << OP_EQUALVERIFY;

    // No operand pushes needed: OP_VER takes nothing from the stack.
    const CScript noOperands;
    const bsv::CMutableTransactionExtended eTX =
        MakeSignedExtendedTx(opcodeCheck, noOperands, key, /*txVersion=*/1);

    // Pre-Chronicle UTXO: OP_VER is a bad opcode.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, PRE_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_BAD_OPCODE));

    // Post-Chronicle UTXO: OP_VER pushes the tx version → comparison passes.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_OK));
}

// ── Test 5 ─────────────────────────────────────────────────────────────────
// OP_VERIF (0x65) — uniquely invalid even in dead branches before Chronicle
// ───────────────────────────────────────────────────────────────────────────
// Part A — Functional use: OP_VERIF as a conditional branching on tx version.
//
//   scriptSig:    <sig> <pubkey> <{01,00,00,00}>   (version 1 as 4-byte LE)
//   scriptPubKey: OP_VERIF OP_1 OP_ELSE OP_0 OP_ENDIF OP_VERIFY
//                 OP_DUP OP_HASH160 <keyID> OP_EQUALVERIFY OP_CHECKSIG
//
//   OP_VERIF pops the top of stack, compares it to the spending tx version.
//   If equal → IF branch executes (OP_1 pushed, OP_VERIFY passes).
//
// Part B — Dead branch uniqueness: OP_VERIF's dead-branch behavior changed
//   between the pre-Genesis and post-Genesis eras, then again at Chronicle.
//
//   scriptPubKey: OP_0 OP_IF OP_VERIF OP_ENDIF OP_1
//   scriptSig:    (empty)
//
//   Pre-Genesis UTXO (utxo_after_genesis = false):
//     OP_VERIF is unconditionally invalid even in dead branches.
//     → SCRIPT_ERR_BAD_OPCODE
//
//   Post-Genesis, pre-Chronicle UTXO (utxo_after_genesis = true,
//                                      utxo_after_chronicle = false):
//     Genesis relaxed the dead-branch rule: OP_VERIF acts as NOP
//     (does NOT push to vfExec).  The IF/ENDIF pair balances.
//     → SCRIPT_ERR_OK
//
//   Note: the spec summary table marks PostGenesis/pre-Chronicle as
//   "Invalid (all branches)", but the interpreter (confirmed by
//   src/test/opcode_tests.cpp) gives NOP in dead branches for post-Genesis
//   UTXOs.  This test exercises the actual implementation.
//
// Applies to: both policy and consensus (tested here under consensus = true).
// Reference: src/script/interpreter.cpp:771-779
BOOST_AUTO_TEST_CASE(test_op_verif)
{
    bsv::CTxValidator se("main");
    const CKey key = LoadKey();

    // ── Part A: functional conditional ────────────────────────────────────
    {
        // Self-contained script — no external signing needed.
        //   scriptPubKey: <{01,00,00,00}> OP_VERIF OP_ENDIF OP_1
        //   scriptSig:    (empty)
        //
        // Execution (post-Chronicle UTXO):
        //   push {01,00,00,00} → OP_VERIF pops it, compares to tx version 1
        //   → match → vfExec.push_back(true) → OP_ENDIF pops vfExec → OP_1 → OK.
        //
        // Execution (pre-Chronicle UTXO):
        //   push {01,00,00,00} → OP_VERIF: fExec=true, !utxo_after_chronicle
        //   → BAD_OPCODE (not in dead branch, so NOP exemption does not apply).
        const std::vector<uint8_t> version1LE{0x01, 0x00, 0x00, 0x00};

        CScript scriptPubKey;
        scriptPubKey << version1LE << OP_VERIF << OP_ENDIF << OP_1;

        CMutableTransaction txCredit = BuildCreditingTx(scriptPubKey);
        CMutableTransaction txSpend  = BuildSpendingTx(txCredit, /*txVersion=*/1);
        txSpend.vin[0].scriptSig = CScript();

        bsv::CMutableTransactionExtended eTX;
        eTX.vutxo = txCredit.vout;
        eTX.mtx   = txSpend;

        // Pre-Chronicle UTXO: OP_VERIF is unconditionally bad (fExec=true).
        BOOST_CHECK(txErrorIsScript(
            Verify(eTX, PRE_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
                   /*consensus=*/true, se),
            SCRIPT_ERR_BAD_OPCODE));

        // Post-Chronicle UTXO: OP_VERIF compares version (1==1) → OK.
        BOOST_CHECK(txErrorIsScript(
            Verify(eTX, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
                   /*consensus=*/true, se),
            SCRIPT_ERR_OK));
    }

    // ── Part B: dead branch — Genesis relaxed OP_VERIF's dead-branch behavior ─
    // OP_0 OP_IF pushes a false condition, so the IF body is never executed.
    // Standard opcodes inside a dead branch are safely skipped in all eras.
    // OP_VERIF is special:
    //
    // Pre-Genesis UTXO (utxo_after_genesis = false):
    //   OP_VERIF is unconditionally invalid in ANY branch, even dead ones.
    //   → SCRIPT_ERR_BAD_OPCODE.
    //
    // Post-Genesis, pre-Chronicle UTXO (utxo_after_genesis = true,
    //                                   utxo_after_chronicle = false):
    //   Genesis relaxed this: OP_VERIF in a dead branch acts as a NOP and
    //   does NOT push to vfExec.  The IF/ENDIF pair properly balances.
    //   → SCRIPT_ERR_OK.
    //
    // (Post-Chronicle UTXOs reactivate OP_VERIF as a full IF-type opcode,
    //  which DOES push to vfExec even in dead branches.  That behaviour
    //  requires a matching extra OP_ENDIF and is not shown with this script.)
    {
        CScript scriptPubKey;
        scriptPubKey << OP_0 << OP_IF << OP_VERIF << OP_ENDIF << OP_1;

        CMutableTransaction txCredit = BuildCreditingTx(scriptPubKey);
        CMutableTransaction txSpend  = BuildSpendingTx(txCredit);
        // Empty scriptSig — the script is self-contained and requires no inputs.
        txSpend.vin[0].scriptSig = CScript();

        bsv::CMutableTransactionExtended eTX;
        eTX.vutxo = txCredit.vout;
        eTX.mtx   = txSpend;

        // Pre-Genesis UTXO: OP_VERIF in dead branch → BAD_OPCODE
        // (utxo_after_genesis = false → no NOP exemption).
        BOOST_CHECK(txErrorIsScript(
            Verify(eTX, PRE_GENESIS_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
                   /*consensus=*/true, se),
            SCRIPT_ERR_BAD_OPCODE));

        // Post-Genesis, pre-Chronicle UTXO: Genesis NOP exemption applies →
        // OP_VERIF in dead branch is skipped → OP_1 → OK.
        BOOST_CHECK(txErrorIsScript(
            Verify(eTX, PRE_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
                   /*consensus=*/true, se),
            SCRIPT_ERR_OK));
    }
}

// ── Test 6 ─────────────────────────────────────────────────────────────────
// OP_SUBSTR (0xb3) — NOP4 before Chronicle, substring extraction after
// ───────────────────────────────────────────────────────────────────────────
// The UTXO is locked so the spender must prove that bytes [1..2] of "ABCD"
// equal "BC".  The spending scriptSig supplies the data, offset, and length;
// the scriptPubKey applies OP_SUBSTR and verifies the result.
//
//   scriptSig:    <sig> <pubkey> <"ABCD"> <1> <2>   (length on top)
//   scriptPubKey: OP_SUBSTR <"BC"> OP_EQUALVERIFY
//                 OP_DUP OP_HASH160 <keyID> OP_EQUALVERIFY OP_CHECKSIG
//
// Pre-Chronicle UTXO, policy path (SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS
//   set): OP_SUBSTR is treated as NOP4; with the DISCOURAGE flag active the
//   interpreter returns SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS immediately,
//   before any stack operations take place.
//   → SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS
//
// Pre-Chronicle UTXO, consensus path (DISCOURAGE absent from block flags):
//   OP_SUBSTR acts as NOP4; the three operands remain on the stack.
//   The subsequent EQUALVERIFY compares the wrong values →
//   SCRIPT_ERR_EQUALVERIFY.
//
// Post-Chronicle UTXO: OP_SUBSTR extracts the substring → EQUALVERIFY passes
//   → CHECKSIG → OK
//
// Applies to: both policy and consensus.
// Reference: src/script/interpreter.cpp:607-639
BOOST_AUTO_TEST_CASE(test_op_substr)
{
    bsv::CTxValidator se("main");
    const CKey key = LoadKey();

    // ── Policy path: DISCOURAGE fires immediately, no stack changes needed ──
    {
        // Minimal script: OP_SUBSTR (NOP4 pre-Chronicle) followed by OP_1.
        // If the NOP path were taken (no DISCOURAGE), OP_1 leaves a truthy
        // value on the stack.  With DISCOURAGE the error fires first.
        CScript scriptPubKey;
        scriptPubKey << OP_SUBSTR << OP_1;

        CMutableTransaction txCredit = BuildCreditingTx(scriptPubKey);
        CMutableTransaction txSpend  = BuildSpendingTx(txCredit);
        txSpend.vin[0].scriptSig = CScript();   // empty — DISCOURAGE fires before operands matter

        bsv::CMutableTransactionExtended discourageTX;
        discourageTX.vutxo = txCredit.vout;
        discourageTX.mtx   = txSpend;

        // POLICY_FLAGS_PRE_CHRONICLE matches GetScriptVerifyFlags(PostGenesis)
        // | InputScriptVerifyFlags(PostGenesis, PostGenesis): includes DISCOURAGE.
        BOOST_CHECK(txErrorIsScript(
            VerifyWithFlags(discourageTX, POST_CHRONICLE_BLOCK_HEIGHT,
                            POLICY_FLAGS_PRE_CHRONICLE, se),
            SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS));
    }

    // ── Consensus path: NOP leaves stack unchanged; EQUALVERIFY exposes it ──
    const std::vector<uint8_t> data    {'A','B','C','D'};
    const std::vector<uint8_t> expected{'B','C'};

    CScript opcodeCheck;
    opcodeCheck << OP_SUBSTR << expected << OP_EQUALVERIFY;

    // Stack layout when scriptPubKey starts: [sig, pubkey, "ABCD", 1, 2]
    // OP_SUBSTR pops length(2), offset(1), data("ABCD") → pushes "BC".
    CScript operandPushes;
    operandPushes << data << CScriptNum(1) << CScriptNum(2);

    const bsv::CMutableTransactionExtended eTX =
        MakeSignedExtendedTx(opcodeCheck, operandPushes, key);

    // Pre-Chronicle, consensus: NOP leaves operands on stack; EQUALVERIFY fails.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, PRE_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_EQUALVERIFY));

    // Post-Chronicle: OP_SUBSTR extracts "BC" → EQUALVERIFY passes → OK.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_OK));
}

// ── Test 7 ─────────────────────────────────────────────────────────────────
// OP_LEFT (0xb4) — NOP5 before Chronicle, left-byte extraction after
// ───────────────────────────────────────────────────────────────────────────
// The UTXO requires the spender to prove that the first 2 bytes of "ABCD"
// are "AB".  OP_LEFT pops (data, length) and pushes data[0..length).
//
//   scriptSig:    <sig> <pubkey> <"ABCD"> <2>
//   scriptPubKey: OP_LEFT <"AB"> OP_EQUALVERIFY
//                 OP_DUP OP_HASH160 <keyID> OP_EQUALVERIFY OP_CHECKSIG
//
// Pre-Chronicle UTXO, policy path: DISCOURAGE flag → SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS
// Pre-Chronicle UTXO, consensus path: NOP5 — operands remain on stack →
//   EQUALVERIFY fails → SCRIPT_ERR_EQUALVERIFY
// Post-Chronicle UTXO: OP_LEFT extracts "AB" → OK
//
// Applies to: both policy and consensus.
// Reference: src/script/interpreter.cpp:640-664
BOOST_AUTO_TEST_CASE(test_op_left)
{
    bsv::CTxValidator se("main");
    const CKey key = LoadKey();

    // ── Policy path: DISCOURAGE fires immediately ──
    {
        CScript scriptPubKey;
        scriptPubKey << OP_LEFT << OP_1;

        CMutableTransaction txCredit = BuildCreditingTx(scriptPubKey);
        CMutableTransaction txSpend  = BuildSpendingTx(txCredit);
        txSpend.vin[0].scriptSig = CScript();

        bsv::CMutableTransactionExtended discourageTX;
        discourageTX.vutxo = txCredit.vout;
        discourageTX.mtx   = txSpend;

        BOOST_CHECK(txErrorIsScript(
            VerifyWithFlags(discourageTX, POST_CHRONICLE_BLOCK_HEIGHT,
                            POLICY_FLAGS_PRE_CHRONICLE, se),
            SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS));
    }

    // ── Consensus path: NOP leaves stack unchanged; EQUALVERIFY exposes it ──
    const std::vector<uint8_t> data    {'A','B','C','D'};
    const std::vector<uint8_t> expected{'A','B'};

    CScript opcodeCheck;
    opcodeCheck << OP_LEFT << expected << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << data << CScriptNum(2); // length on top

    const bsv::CMutableTransactionExtended eTX =
        MakeSignedExtendedTx(opcodeCheck, operandPushes, key);

    // Pre-Chronicle, consensus: NOP leaves operands on stack; EQUALVERIFY fails.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, PRE_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_EQUALVERIFY));

    // Post-Chronicle: OP_LEFT extracts "AB" → EQUALVERIFY passes → OK.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_OK));
}

// ── Test 8 ─────────────────────────────────────────────────────────────────
// OP_RIGHT (0xb5) — NOP6 before Chronicle, right-byte extraction after
// ───────────────────────────────────────────────────────────────────────────
// The UTXO requires the spender to prove that the last 2 bytes of "ABCD"
// are "CD".  OP_RIGHT pops (data, length) and pushes the trailing `length`
// bytes of data.
//
//   scriptSig:    <sig> <pubkey> <"ABCD"> <2>
//   scriptPubKey: OP_RIGHT <"CD"> OP_EQUALVERIFY
//                 OP_DUP OP_HASH160 <keyID> OP_EQUALVERIFY OP_CHECKSIG
//
// Pre-Chronicle UTXO, policy path: DISCOURAGE flag → SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS
// Pre-Chronicle UTXO, consensus path: NOP6 — operands remain on stack →
//   EQUALVERIFY fails → SCRIPT_ERR_EQUALVERIFY
// Post-Chronicle UTXO: OP_RIGHT extracts "CD" → OK
//
// Applies to: both policy and consensus.
// Reference: src/script/interpreter.cpp:665-689
BOOST_AUTO_TEST_CASE(test_op_right)
{
    bsv::CTxValidator se("main");
    const CKey key = LoadKey();

    // ── Policy path: DISCOURAGE fires immediately ──
    {
        CScript scriptPubKey;
        scriptPubKey << OP_RIGHT << OP_1;

        CMutableTransaction txCredit = BuildCreditingTx(scriptPubKey);
        CMutableTransaction txSpend  = BuildSpendingTx(txCredit);
        txSpend.vin[0].scriptSig = CScript();

        bsv::CMutableTransactionExtended discourageTX;
        discourageTX.vutxo = txCredit.vout;
        discourageTX.mtx   = txSpend;

        BOOST_CHECK(txErrorIsScript(
            VerifyWithFlags(discourageTX, POST_CHRONICLE_BLOCK_HEIGHT,
                            POLICY_FLAGS_PRE_CHRONICLE, se),
            SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS));
    }

    // ── Consensus path: NOP leaves stack unchanged; EQUALVERIFY exposes it ──
    const std::vector<uint8_t> data    {'A','B','C','D'};
    const std::vector<uint8_t> expected{'C','D'};

    CScript opcodeCheck;
    opcodeCheck << OP_RIGHT << expected << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << data << CScriptNum(2); // length on top

    const bsv::CMutableTransactionExtended eTX =
        MakeSignedExtendedTx(opcodeCheck, operandPushes, key);

    // Pre-Chronicle, consensus: NOP leaves operands on stack; EQUALVERIFY fails.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, PRE_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_EQUALVERIFY));

    // Post-Chronicle: OP_RIGHT extracts "CD" → EQUALVERIFY passes → OK.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_OK));
}

// ── Test 9 ─────────────────────────────────────────────────────────────────
// OP_LSHIFTNUM (0xb6) — NOP7 before Chronicle, numeric left-shift after
// ───────────────────────────────────────────────────────────────────────────
// The UTXO is locked so the spender must prove that 3 << 2 == 12.
// OP_LSHIFTNUM pops (x, n) and pushes x << n (as CScriptNum arithmetic).
//
//   scriptSig:    <sig> <pubkey> <3> <2>   (shift amount on top)
//   scriptPubKey: OP_LSHIFTNUM <12> OP_EQUALVERIFY
//                 OP_DUP OP_HASH160 <keyID> OP_EQUALVERIFY OP_CHECKSIG
//
// Pre-Chronicle UTXO, policy path: DISCOURAGE flag → SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS
// Pre-Chronicle UTXO, consensus path: NOP7 — operands remain on stack →
//   EQUALVERIFY fails → SCRIPT_ERR_EQUALVERIFY
// Post-Chronicle UTXO: 3 << 2 = 12 → EQUALVERIFY passes → OK
//
// Applies to: both policy and consensus.
// Reference: src/script/interpreter.cpp:690-725
BOOST_AUTO_TEST_CASE(test_op_lshiftnum)
{
    bsv::CTxValidator se("main");
    const CKey key = LoadKey();

    // ── Policy path: DISCOURAGE fires immediately ──
    {
        CScript scriptPubKey;
        scriptPubKey << OP_LSHIFTNUM << OP_1;

        CMutableTransaction txCredit = BuildCreditingTx(scriptPubKey);
        CMutableTransaction txSpend  = BuildSpendingTx(txCredit);
        txSpend.vin[0].scriptSig = CScript();

        bsv::CMutableTransactionExtended discourageTX;
        discourageTX.vutxo = txCredit.vout;
        discourageTX.mtx   = txSpend;

        BOOST_CHECK(txErrorIsScript(
            VerifyWithFlags(discourageTX, POST_CHRONICLE_BLOCK_HEIGHT,
                            POLICY_FLAGS_PRE_CHRONICLE, se),
            SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS));
    }

    // ── Consensus path: NOP leaves stack unchanged; EQUALVERIFY exposes it ──
    CScript opcodeCheck;
    opcodeCheck << OP_LSHIFTNUM << CScriptNum(12) << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << CScriptNum(3) << CScriptNum(2); // shift amount on top

    const bsv::CMutableTransactionExtended eTX =
        MakeSignedExtendedTx(opcodeCheck, operandPushes, key);

    // Pre-Chronicle, consensus: NOP leaves operands on stack; EQUALVERIFY fails.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, PRE_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_EQUALVERIFY));

    // Post-Chronicle: 3 << 2 = 12 → EQUALVERIFY passes → OK.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_OK));
}

// ── Test 10 ────────────────────────────────────────────────────────────────
// OP_RSHIFTNUM (0xb7) — NOP8 before Chronicle, numeric right-shift after
// ───────────────────────────────────────────────────────────────────────────
// The UTXO is locked so the spender must prove that 12 >> 2 == 3.
// OP_RSHIFTNUM pops (x, n) and pushes x >> n (truncated toward zero).
//
//   scriptSig:    <sig> <pubkey> <12> <2>  (shift amount on top)
//   scriptPubKey: OP_RSHIFTNUM <3> OP_EQUALVERIFY
//                 OP_DUP OP_HASH160 <keyID> OP_EQUALVERIFY OP_CHECKSIG
//
// Pre-Chronicle UTXO, policy path: DISCOURAGE flag → SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS
// Pre-Chronicle UTXO, consensus path: NOP8 — operands remain on stack →
//   EQUALVERIFY fails → SCRIPT_ERR_EQUALVERIFY
// Post-Chronicle UTXO: 12 >> 2 = 3 → EQUALVERIFY passes → OK
//
// Applies to: both policy and consensus.
// Reference: src/script/interpreter.cpp:726-761
BOOST_AUTO_TEST_CASE(test_op_rshiftnum)
{
    bsv::CTxValidator se("main");
    const CKey key = LoadKey();

    // ── Policy path: DISCOURAGE fires immediately ──
    {
        CScript scriptPubKey;
        scriptPubKey << OP_RSHIFTNUM << OP_1;

        CMutableTransaction txCredit = BuildCreditingTx(scriptPubKey);
        CMutableTransaction txSpend  = BuildSpendingTx(txCredit);
        txSpend.vin[0].scriptSig = CScript();

        bsv::CMutableTransactionExtended discourageTX;
        discourageTX.vutxo = txCredit.vout;
        discourageTX.mtx   = txSpend;

        BOOST_CHECK(txErrorIsScript(
            VerifyWithFlags(discourageTX, POST_CHRONICLE_BLOCK_HEIGHT,
                            POLICY_FLAGS_PRE_CHRONICLE, se),
            SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS));
    }

    // ── Consensus path: NOP leaves stack unchanged; EQUALVERIFY exposes it ──
    CScript opcodeCheck;
    opcodeCheck << OP_RSHIFTNUM << CScriptNum(3) << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << CScriptNum(12) << CScriptNum(2); // shift amount on top

    const bsv::CMutableTransactionExtended eTX =
        MakeSignedExtendedTx(opcodeCheck, operandPushes, key);

    // Pre-Chronicle, consensus: NOP leaves operands on stack; EQUALVERIFY fails.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, PRE_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_EQUALVERIFY));

    // Post-Chronicle: 12 >> 2 = 3 → EQUALVERIFY passes → OK.
    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_OK));
}

// ── Test 11 ────────────────────────────────────────────────────────────────
// OP_2MUL (0x8d) — disabled before Chronicle UTXO, multiply-by-2 after
// ───────────────────────────────────────────────────────────────────────────
// OP_2MUL is gated by IsOpcodeDisabled() which returns true whenever the UTXO
// was *not* created after Chronicle activation.  This is different from the
// NOP pattern (tests 6-10): there is no NOP fallback — the opcode was simply
// disabled and returns SCRIPT_ERR_DISABLED_OPCODE immediately.
//
// The UTXO requires the spender to prove that 5 × 2 == 10.
//
//   scriptSig:    <sig> <pubkey> <5>
//   scriptPubKey: OP_2MUL <10> OP_EQUALVERIFY
//                 OP_DUP OP_HASH160 <keyID> OP_EQUALVERIFY OP_CHECKSIG
//
// Pre-Chronicle UTXO: IsOpcodeDisabled() → SCRIPT_ERR_DISABLED_OPCODE
// Post-Chronicle UTXO: 5 * 2 = 10 → EQUALVERIFY passes → OK
//
// Applies to: both policy and consensus (tested here under consensus = true).
// Reference: src/script/interpreter.cpp:362-365
BOOST_AUTO_TEST_CASE(test_op_2mul)
{
    bsv::CTxValidator se("main");
    const CKey key = LoadKey();

    CScript opcodeCheck;
    opcodeCheck << OP_2MUL << CScriptNum(10) << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << CScriptNum(5);

    const bsv::CMutableTransactionExtended eTX =
        MakeSignedExtendedTx(opcodeCheck, operandPushes, key);

    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, PRE_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_DISABLED_OPCODE));

    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_OK));
}

// ── Test 12 ────────────────────────────────────────────────────────────────
// OP_2DIV (0x8e) — disabled before Chronicle UTXO, divide-by-2 after
// ───────────────────────────────────────────────────────────────────────────
// Same gating mechanism as OP_2MUL.  OP_2DIV pops a CScriptNum and pushes
// its value divided by 2 (truncated toward zero).
//
// The UTXO requires the spender to prove that 10 / 2 == 5.
//
//   scriptSig:    <sig> <pubkey> <10>
//   scriptPubKey: OP_2DIV <5> OP_EQUALVERIFY
//                 OP_DUP OP_HASH160 <keyID> OP_EQUALVERIFY OP_CHECKSIG
//
// Pre-Chronicle UTXO: IsOpcodeDisabled() → SCRIPT_ERR_DISABLED_OPCODE
// Post-Chronicle UTXO: 10 / 2 = 5 → EQUALVERIFY passes → OK
//
// Applies to: both policy and consensus (tested here under consensus = true).
// Reference: src/script/interpreter.cpp:362-365
BOOST_AUTO_TEST_CASE(test_op_2div)
{
    bsv::CTxValidator se("main");
    const CKey key = LoadKey();

    CScript opcodeCheck;
    opcodeCheck << OP_2DIV << CScriptNum(5) << OP_EQUALVERIFY;

    CScript operandPushes;
    operandPushes << CScriptNum(10);

    const bsv::CMutableTransactionExtended eTX =
        MakeSignedExtendedTx(opcodeCheck, operandPushes, key);

    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, PRE_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_DISABLED_OPCODE));

    BOOST_CHECK(txErrorIsScript(
        Verify(eTX, POST_CHRONICLE_UTXO_HEIGHT, POST_CHRONICLE_BLOCK_HEIGHT,
               /*consensus=*/true, se),
        SCRIPT_ERR_OK));
}

BOOST_AUTO_TEST_SUITE_END()

// ═══════════════════════════════════════════════════════════════════════════
// Helper definitions
// ═══════════════════════════════════════════════════════════════════════════

static CKey LoadKey()
{
    CBitcoinSecret secret;
    if (!secret.SetString(WIF_KEY))
        throw std::runtime_error("Failed to decode WIF key");
    return secret.GetKey();
}

static CMutableTransaction BuildCreditingTx(const CScript& scriptPubKey)
{
    CMutableTransaction tx;
    tx.nVersion  = 1;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vin[0].prevout   = COutPoint();  // coinbase-style: no real previous output
    tx.vin[0].scriptSig = CScript() << CScriptNum(0) << CScriptNum(0);
    tx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    tx.vout[0].scriptPubKey = scriptPubKey;
    tx.vout[0].nValue       = UTXO_AMOUNT;
    return tx;
}

static CMutableTransaction BuildSpendingTx(const CMutableTransaction& txCredit,
                                            int32_t txVersion)
{
    CMutableTransaction tx;
    tx.nVersion  = txVersion;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vin[0].prevout   = COutPoint(txCredit.GetId(), 0);
    tx.vin[0].scriptSig = CScript();  // caller fills this in
    tx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    tx.vout[0].scriptPubKey = CScript();  // anyone-can-spend output
    tx.vout[0].nValue       = UTXO_AMOUNT;
    return tx;
}

static std::vector<uint8_t> Sign(const CScript& scriptPubKey,
                                  CMutableTransaction& txSpend,
                                  const CKey& key,
                                  SigHashType sigHashType)
{
    const uint256 hash = SignatureHash(
        scriptPubKey, CTransaction(txSpend), /*nIn=*/0, sigHashType, UTXO_AMOUNT);

    std::vector<uint8_t> sig;
    if (!key.Sign(hash, sig))
        throw std::runtime_error("ECDSA signing failed");

    sig.push_back(static_cast<uint8_t>(sigHashType.getRawSigHashType()));
    return sig;
}

static bsv::CMutableTransactionExtended MakeSignedExtendedTx(
    const CScript& opcodeCheck,
    const CScript& operandPushes,
    const CKey&    key,
    int32_t        txVersion)
{
    const CPubKey pubkey = key.GetPubKey();
    const CKeyID  keyID  = pubkey.GetID();

    // Full scriptPubKey: opcode check (consuming operands from the stack top)
    // followed by standard P2PKH ownership check.
    CScript scriptPubKey = opcodeCheck;
    scriptPubKey << OP_DUP
                 << OP_HASH160 << ToByteVector(keyID)
                 << OP_EQUALVERIFY
                 << OP_CHECKSIG;

    CMutableTransaction txCredit = BuildCreditingTx(scriptPubKey);
    CMutableTransaction txSpend  = BuildSpendingTx(txCredit, txVersion);

    // Sign the spending tx using the full scriptPubKey and default sighash type.
    const auto sig = Sign(scriptPubKey, txSpend, key);

    // ScriptSig layout (bottom → top): sig, pubkey, then opcode operands.
    // The opcode in scriptPubKey consumes the operands first (from the top),
    // leaving sig and pubkey for CHECKSIG.
    CScript scriptSig;
    scriptSig << sig << ToByteVector(pubkey);
    scriptSig.insert(scriptSig.end(), operandPushes.begin(), operandPushes.end());

    txSpend.vin[0].scriptSig = scriptSig;

    bsv::CMutableTransactionExtended eTX;
    eTX.vutxo = txCredit.vout;
    eTX.mtx   = txSpend;
    return eTX;
}

static TxError Verify(const bsv::CMutableTransactionExtended& eTX,
                      int32_t utxoHeight,
                      int32_t blockHeight,
                      bool consensus,
                      bsv::CTxValidator& se)
{
    CDataStream out(SER_NETWORK, PROTOCOL_VERSION);
    out << eTX;
    const std::vector<uint8_t> etxBin(out.begin(), out.end());

    std::array<int32_t, 1> utxoHeights{utxoHeight};
    return se.VerifyScript(
        etxBin,
        std::span<const int32_t>(utxoHeights),
        blockHeight,
        consensus);
}

static TxError VerifyWithFlags(const bsv::CMutableTransactionExtended& eTX,
                               int32_t blockHeight,
                               uint32_t flags,
                               bsv::CTxValidator& se)
{
    CDataStream out(SER_NETWORK, PROTOCOL_VERSION);
    out << eTX;
    const std::vector<uint8_t> etxBin(out.begin(), out.end());

    std::array<int32_t, 1> dummyHeights{0};
    std::array<uint32_t, 1> customFlags{flags};
    return se.VerifyScript(
        etxBin,
        std::span<const int32_t>(dummyHeights),
        blockHeight,
        /*consensus=*/true,
        std::span<const uint32_t>(customFlags));
}

static bool txErrorIsScript(TxError r, ScriptError_t e) {
    if (e == SCRIPT_ERR_OK) return r.domain == TX_ERR_DOMAIN_OK;
    return r.domain == TX_ERR_DOMAIN_SCRIPT && r.code == static_cast<int32_t>(e);
}
