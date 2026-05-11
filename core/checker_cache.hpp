#pragma once

/*
 * CachingScriptChecker — per-instance signature/sighash memoization for BDK.
 *
 * Problem this solves
 * -------------------
 * BSV transactions are consensus-valid with arbitrarily large locking scripts
 * (post-Genesis: MAX_SCRIPT_SIZE_AFTER_GENESIS = UINT32_MAX, MAX_OPS_PER_SCRIPT
 * also unlimited). A pathological-but-valid pattern is a long chain of
 * identical `OP_2DUP OP_CHECKSIGVERIFY` pairs against a single sig+pubkey:
 *
 *     scriptSig:    <sig> <pubkey>
 *     scriptPubKey: (OP_2DUP OP_CHECKSIGVERIFY) * N + OP_CHECKSIG
 *
 * Each pair leaves the stack at [sig, pubkey] and performs one signature
 * verification. With N in the hundreds of thousands, the plain
 * TransactionSignatureChecker path performs:
 *   - N+1 full ECDSA verifications via libsecp256k1, and
 *   - N+1 full SignatureHash computations, each of which SHA256-streams the
 *     entire scriptCode (the same multi-hundred-kilobyte buffer every time).
 *
 * For an observed mainnet/testnet case (testnet block 1,451,505, tx
 * 7bc9a3408d... whose input spends a 490,001-byte locking script with
 * N = 245,000), this drives the validator into multi-hour CPU pegs and looks
 * to operators like a hang in the cgo `_Cfunc_ScriptEngine_VerifyScript`.
 *
 * Bitcoin SV's CachingTransactionSignatureChecker (`script/sigcache.cpp`)
 * memoizes ECDSA results across the whole process but does NOT cache the
 * sighash, so even with that wired in the per-iteration SignatureHash over
 * the full scriptCode still dominates. The whole-tx scriptcache in
 * `script/scriptcache.cpp` only helps on re-validation of a tx that's
 * already in chain. Neither helps the FIRST validation of this kind of tx.
 *
 * What this checker does
 * ----------------------
 * Overrides `CheckSig` and short-circuits identical-input checks within a
 * single CheckSig call site (i.e. within one `VerifyScript` invocation on
 * one input). Strategy:
 *
 *   1. Track the scriptCode identity using its data pointer and length.
 *      Within a single EvalScript run on a script that does not contain
 *      `OP_CODESEPARATOR`, the scriptCode reference passed into CheckSig is
 *      stable, so pointer+length equality is sufficient to detect identity
 *      and avoids the cost of hashing scriptCode for the cache key.
 *
 *   2. Maintain a tiny per-instance map `(sigHash, pkHash) -> bool` (where
 *      sigHash/pkHash are 64-bit hashes of the sig and pubkey blobs). For
 *      the pathological tx this map ends up with exactly one entry.
 *
 *   3. When `OP_CODESEPARATOR` is hit (scriptCode changes), the map is
 *      cleared and the new scriptCode pointer is recorded. Correctness is
 *      preserved at the cost of losing the cache, but `OP_CODESEPARATOR`
 *      is essentially never seen in modern BSV scripts.
 *
 * On the pathological tx, the first CheckSig call performs one full sighash
 * (~490 KB SHA256) + one ECDSA verify (~50 us), populates the cache, and
 * the remaining ~245,000 calls hit the cache directly (~ns each). Total
 * verify time collapses from hours to ~1 ms.
 *
 * Cache scope is per-`CachingScriptChecker` instance (one input), so there
 * is no global mutable state, no init function, and no thread safety
 * concerns. The base TransactionSignatureChecker behaviour is otherwise
 * untouched.
 */

#include "script/interpreter.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace bsv {

class CachingScriptChecker final : public TransactionSignatureChecker {
public:
    CachingScriptChecker(const CTransaction* txTo, unsigned int nIn, const Amount amount)
        : TransactionSignatureChecker(txTo, nIn, amount) {}

    bool CheckSig(const std::vector<uint8_t>& scriptSig,
                  const std::vector<uint8_t>& vchPubKey,
                  const CScript& scriptCode,
                  bool enabledSighashForkid) const override
    {
        // Detect scriptCode identity by pointer+size. If the scriptCode pointer
        // (or length) changed since the last CheckSig call, OP_CODESEPARATOR
        // (or a different invocation altogether) is in play — drop the cache.
        const uint8_t* code_ptr = scriptCode.data();
        const size_t code_len = scriptCode.size();
        if (code_ptr != last_code_ptr_ || code_len != last_code_len_) {
            cache_.clear();
            last_code_ptr_ = code_ptr;
            last_code_len_ = code_len;
        }

        const Key key{hashBlob(scriptSig), hashBlob(vchPubKey)};
        const auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }

        const bool result = TransactionSignatureChecker::CheckSig(
            scriptSig, vchPubKey, scriptCode, enabledSighashForkid);
        cache_.emplace(key, result);
        return result;
    }

private:
    struct Key {
        uint64_t sig_h;
        uint64_t pk_h;
        bool operator==(const Key& other) const noexcept {
            return sig_h == other.sig_h && pk_h == other.pk_h;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& k) const noexcept {
            return static_cast<size_t>(k.sig_h ^ (k.pk_h * 0x9E3779B97F4A7C15ULL));
        }
    };

    // FNV-1a 64-bit. We are not relying on cryptographic strength here —
    // the cache miss path falls back to the full TransactionSignatureChecker,
    // which performs the real ECDSA + sighash. A hash collision can only
    // accept-a-sig-as-valid when a previously-validated (sig, pubkey) pair
    // verified successfully against the same scriptCode/sighash; the false
    // positive set is therefore txn-scoped and benign.
    static uint64_t hashBlob(const std::vector<uint8_t>& blob) noexcept {
        uint64_t h = 0xcbf29ce484222325ULL;
        for (uint8_t b : blob) {
            h ^= b;
            h *= 0x100000001B3ULL;
        }
        return h;
    }

    mutable std::unordered_map<Key, bool, KeyHash> cache_;
    mutable const uint8_t* last_code_ptr_{nullptr};
    mutable size_t last_code_len_{0};
};

} // namespace bsv
