#include <set>

#include <base58.h>
#include <chainparams.h>
#include <core_io.h>
#include <protocol_era.h>
#include <policy/policy.h>
#include <verify_script_flags.h>
#include <script/script_flags.h>
#include <script/standard.h>
#include <script/interpreter.h>

#include <chainparams_bdk.hpp>
#include <extendedTx.hpp>
#include <txvalidator.hpp>
#include <internal/txvalidator_internal.hpp>
#include <sha256_backend.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Builds without threads (WASM without pthreads) verify inputs serially only.
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#define BDK_PARALLEL_INPUTS 0
#else
#define BDK_PARALLEL_INPUTS 1
#endif

#if BDK_PARALLEL_INPUTS
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <system_error>
#include <thread>
#if !defined(_WIN32)
#include <pthread.h>
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  File-local helper: CachingScriptChecker                                   //
//                                                                            //
//  Per-instance signature/sighash memoization used by implVerifyScript.      //
//  Kept in this translation unit (anonymous namespace, internal linkage)     //
//  because it overrides one consensus-critical method with a per-input      //
//  invariant that callers outside this file cannot be trusted to preserve   //
//  (one instance per VerifyScript call, paired with the right scriptCode    //
//  flow). Everything below this banner is private to txvalidator.cpp.       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
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
 * Empirically observed on BSV testnet, tx
 * 7bc9a3408dd0c87b835c887a0bce22c20788fc3c4b953929d4367656d80acab5,
 * whose input spends a 490,001-byte locking script of
 * `(OP_2DUP OP_CHECKSIGVERIFY) * 245,000 + OP_CHECKSIG`. Without this
 * caching strategy, validating that single transaction takes multiple
 * hours of CPU time and looks to operators like a hang in the cgo
 * `_Cfunc_ScriptEngine_VerifyScript` shim. With the cache in place the
 * same transaction validates in a few minutes — the per-iteration cost
 * drops from "full sighash + ECDSA verify" to "one memcmp over the
 * 490 KB scriptCode + one byte-equality compare against the cached
 * entry".
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
 *   1. Track scriptCode identity by COPY + memcmp of its raw bytes. The
 *      BSV interpreter (`script/interpreter.cpp`) reconstructs scriptCode as
 *      `CScript scriptCode {pbegincodehash, pend};` on every OP_CHECKSIG /
 *      OP_CHECKSIGVERIFY iteration, so the scriptCode object handed to
 *      CheckSig is freshly allocated each call and its `data()` pointer is
 *      not stable across iterations. Pointer-identity caching does not
 *      work; memcmp of the underlying bytes is the cheapest correct test.
 *
 *   2. Maintain a tiny per-instance list of `(sig bytes, pubkey bytes,
 *      result)` entries. A cache hit requires BYTEWISE EQUALITY of both
 *      blobs against the entry — see the SECURITY section below for why
 *      hash-based keying is not acceptable here.
 *
 *   3. When the bytewise scriptCode comparison fails (a different scriptCode
 *      is being verified — typically after `OP_CODESEPARATOR`) the list is
 *      cleared and the new scriptCode bytes are stashed. Correctness is
 *      preserved at the cost of losing the cache.
 *
 * SECURITY — why we do not key the cache on a hash
 * ------------------------------------------------
 * `CheckSig` is consensus-critical: its boolean return decides whether the
 * surrounding tx is accepted. If two distinct `(sig, pubkey)` pairs ever
 * shared the same cache key, and the first verified true, the second would
 * be accepted from the cache without ECDSA ever running — i.e. an invalid
 * signature accepted as valid. BDK and the upstream BSV node would then
 * disagree on tx validity, which is a consensus divergence: the worst
 * class of bug we can ship from this codepath.
 *
 * A non-cryptographic 64-bit hash (FNV-1a or similar) over the sig and
 * pubkey blobs is NOT adequate. The hash function is public, the input is
 * fully attacker-controlled, and 64-bit collisions are reachable offline
 * with a meet-in-the-middle search. An attacker only needs to find a
 * genuinely-valid `(sig1, pk1)` and a genuinely-invalid `(sig2, pk2)`
 * whose hashes collide, and place both inside one script spending one
 * input — the second check would hit the cache and return true.
 *
 * We sidestep the entire question by keying on the raw bytes. A cache
 * hit is therefore equivalent to `std::vector<uint8_t>::operator==` on
 * both blobs — exact equality, no probabilistic accept, no consensus
 * divergence risk introduced by this optimization.
 *
 * Performance — worst case (the motivating pathological tx)
 * ---------------------------------------------------------
 * The first CheckSig call performs one full sighash (~490 KB SHA256) +
 * one ECDSA verify (~50 us) and populates the list with one entry.
 * Every subsequent call does one memcmp over the scriptCode bytes plus
 * a single byte-equality compare against that one cached entry (~71 B
 * sig + ~33 B pubkey, sub-100 ns). The ECDSA verify and the SHA256 over
 * the multi-hundred-kilobyte scriptCode — the work that dominated wall-
 * clock time without the cache — drop out of every iteration after the
 * first.
 *
 * Empirically: the testnet tx
 * 7bc9a3408dd0c87b835c887a0bce22c20788fc3c4b953929d4367656d80acab5
 * goes from validating in multiple hours to validating in a few minutes
 * — roughly two orders of magnitude faster. The remaining cost is
 * dominated by the `N * memcmp(scriptCode)` work (245,000 iterations
 * times 490 KB is ~120 GB of memory traffic) plus the EvalScript
 * interpreter loop overhead. "A few minutes" is well within "the
 * validator looks responsive" territory and keeps the cgo goroutine
 * bounded, which is the property we needed.
 *
 * Performance — common-case tax (what every normal tx pays)
 * ---------------------------------------------------------
 * A typical 1-input P2PKH "Alice -> Bob + change" tx hits CheckSig
 * exactly once per input, so the cache never delivers a hit. The added
 * work on that path is:
 *   - one ~25 B copy of scriptCode into `last_code_bytes_` (1 small alloc),
 *   - one linear scan over an empty vector (zero work),
 *   - one push_back of an Entry{sig, pubkey, result} that heap-copies the
 *     ~71 B sig and ~33 B pubkey (2 small allocs + 2 small memcpys),
 *   - one destructor pass freeing the three vectors at end of scope.
 *
 * Estimated overhead: ~300-500 ns + ~3 small heap allocations per input,
 * against a real CheckSig cost of ~30-100 us (SignatureHash + libsecp256k1
 * ECDSA verify). That is 0.5-2% of one CheckSig — well under 1% of a
 * whole-tx validation once tx I/O and other checks are included.
 *
 * These numbers are arithmetic estimates, not measured. If exact figures
 * matter for a decision, the cheapest experiment is to wrap
 * `implVerifyScript` with `std::chrono::steady_clock` around a typical
 * mempool batch with and without the cache wired in.
 *
 * The trade-off is heavily asymmetric: >1000x speedup on the worst case
 * against <2% tax on the common case. This is cheap insurance against
 * the validator hanging on a consensus-valid abuse pattern.
 *
 * Performance — why linear scan and not a hash map
 * ------------------------------------------------
 * The working set is empirically tiny: exactly one entry on the
 * pathological tx, low single digits on any realistic multi-key contract
 * script we have seen. At those sizes a linear scan over a
 * `std::vector<Entry>` is faster than hashing two `std::vector<uint8_t>`
 * from scratch on every call, on top of being collision-free by
 * construction (see SECURITY above). If a future workload pushes the
 * cache to dozens of entries the structure can be swapped to an
 * `unordered_map` keyed on `std::pair<vector,vector>` without changing
 * the correctness contract — the byte-equality check is what closes the
 * door on false-positive accepts; the container choice is just CPU.
 *
 * Cache scope is per-`CachingScriptChecker` instance (one input), so there
 * is no global mutable state, no init function, and no thread safety
 * concerns. The base TransactionSignatureChecker behaviour is otherwise
 * untouched.
 *
 * Shared sighash precompute (per transaction, not per input)
 * ----------------------------------------------------------
 * The per-input cache above does not cover the BIP143 midstate hashes:
 * hashPrevouts, hashSequence and hashOutputs each hash data from the whole
 * transaction. Without shared data the base class recomputes them on every
 * FORKID signature check that misses the per-input cache: a tx with N
 * inputs costs O(N^2), and one input with K distinct signatures (multisig,
 * contracts with several CHECKSIGs) costs K x O(N). Every caller therefore
 * builds one PrecomputedTransactionData per transaction, as SV Node's
 * validation does, and passes it in: implVerifyScript shares it across all
 * inputs, VerifySpend across the checks of its one input. The checker only
 * forwards it to TransactionSignatureChecker; SV Node's SignatureHashBIP143
 * decides, per field and per sighash type, which aggregate a FORKID check
 * reads:
 *   - hashPrevouts unless ANYONECANPAY,
 *   - hashSequence unless ANYONECANPAY, SINGLE or NONE,
 *   - hashOutputs unless SINGLE or NONE (SINGLE hashes its own output, NONE
 *     uses zero).
 * So ALL|ANYONECANPAY still reads hashOutputs and NONE or SINGLE without
 * ANYONECANPAY still reads hashPrevouts; only NONE|ANYONECANPAY and
 * SINGLE|ANYONECANPAY read none of them. Non-FORKID and Chronicle signatures
 * go to SignatureHashOriginal, which ignores the precompute. When no check
 * reads an aggregate, building it is one wasted O(N) pass, the same as SV
 * Node's unconditional precompute.
 */
namespace {

class CachingScriptChecker final : public TransactionSignatureChecker {
public:
    // txdata must be built from *txTo and outlive this checker: the base class
    // keeps a pointer to it. Binding a temporary, const or not, is rejected at
    // compile time: an rvalue prefers the deleted const&& overload.
    CachingScriptChecker(const CTransaction* txTo, unsigned int nIn, const Amount amount,
                         const PrecomputedTransactionData& txdata)
        : TransactionSignatureChecker(txTo, nIn, amount, txdata) {}
    CachingScriptChecker(const CTransaction* txTo, unsigned int nIn, const Amount amount,
                         const PrecomputedTransactionData&& txdata) = delete;

    bool CheckSig(const std::vector<uint8_t>& scriptSig,
                  const std::vector<uint8_t>& vchPubKey,
                  const CScript& scriptCode,
                  bool enabledSighashForkid) const override
    {
        // EvalScript builds scriptCode = `CScript{pbegincodehash, pend}` afresh
        // on every CheckSig invocation, so its data() pointer is NOT stable
        // across iterations. Detect identity by bytewise comparison; reset the
        // cache when content differs (typically after OP_CODESEPARATOR).
        const size_t code_len = scriptCode.size();
        const bool same_code =
            code_len == last_code_bytes_.size() &&
            (code_len == 0 ||
             std::memcmp(scriptCode.data(), last_code_bytes_.data(), code_len) == 0);
        if (!same_code) {
            cache_.clear();
            last_code_bytes_.assign(scriptCode.begin(), scriptCode.end());
        }

        // Linear scan with bytewise equality. A hit therefore means "the
        // exact same (sig, pubkey) was just verified against the exact same
        // scriptCode" — no hash-collision false positive is possible. See
        // the SECURITY section in the file header for why this is the bar
        // we have to clear, and why hash-based keying was rejected.
        //
        // Working set is empirically 1 entry on the pathological tx and low
        // single digits on multi-key contract scripts, so the scan is faster
        // than hashing two std::vector<uint8_t> on every call.
        for (const Entry& e : cache_) {
            if (e.sig == scriptSig && e.pubkey == vchPubKey) {
                return e.result;
            }
        }

        const bool result = TransactionSignatureChecker::CheckSig(
            scriptSig, vchPubKey, scriptCode, enabledSighashForkid);
        cache_.push_back(Entry{scriptSig, vchPubKey, result});
        return result;
    }

private:
    struct Entry {
        std::vector<uint8_t> sig;
        std::vector<uint8_t> pubkey;
        bool result;
    };

    mutable std::vector<Entry> cache_;
    mutable std::vector<uint8_t> last_code_bytes_;
};

/*
 * Parallel input verification
 * ---------------------------
 * implVerifyScript verifies the inputs of a transaction with at least
 * parallelInputThreshold inputs on the calling thread plus idle helpers from
 * one process-wide pool. The result is the serial result: the outcome of the
 * lowest-index input that fails, whether a TxError or an exception.
 *
 * Every participant claims the next input index from one shared counter, so
 * inputs are verified in index order and every index below the lowest known
 * failure is claimed, and finished before the caller returns. Indices above it
 * are skipped. The calling thread works from the start and only ever waits for
 * helpers that have joined and are verifying an input; a helper that reaches a
 * closed job drops it. Nested use (a helper's work calling back in) therefore
 * cannot deadlock.
 *
 * Oversubscription: busyThreads counts every VerifyScript, ValidateTransaction
 * or VerifySpend caller after its entry poll, plus every helper on a job.
 * Deserialization, hashing and transaction checks count against that budget. A helper joins only while that count
 * is below the core count, and checks it again before claiming each input,
 * leaving the job once the count exceeds the core count. So a helper holds a
 * core for at most one more input once the machine is oversubscribed. Callers
 * are never held back: when callers alone occupy every core a large tx's job
 * is queued without waking anyone and its caller starts alone; a helper joins
 * when a core frees. The core count is
 * EffectiveCpuCount(): on Linux it respects the affinity mask and a
 * container's cgroup CPU limit, rounded as Go sets GOMAXPROCS.
 *
 * Helper limit: SetParallelInputHelpers caps the helpers on jobs at once,
 * process-wide. A helper takes a place under the limit before it takes a
 * core, whichever wake brought it, and gives it back when it leaves the job.
 * Before each claim a helper also checks the places against the limit, and
 * while more are held than it allows, gives its own back and leaves, so a
 * lowered limit holds from each helper's next input. A limit of 0 never
 * creates the pool.
 *
 * Cancellation: only the calling thread polls the caller's token, between its
 * input claims and in its interpreter, publishing observed cancellation to a
 * job flag. Each participant reuses a source combining that flag with
 * lowestFailure < its current input index; helpers never read the caller token.
 * Higher inputs stop at their next interpreter poll. After joined helpers drain,
 * return a real failure only if every lower input completed, or success if all
 * inputs passed. Necessary work abandoned due to cancellation returns CANCELLED.
 * There is no post-drain token poll, so late cancellation keeps a completed
 * verdict and a caller-thread CPU budget excludes helper CPU.
 *
 * Shared by all participants, read only: the CTransaction, its
 * PrecomputedTransactionData, the previous outputs, heights and flags, and the
 * CTxValidator's settings. Each input gets its own CachingScriptChecker.
 */

// The default threshold, bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD, comes
// from the threshold_sweep benchmark in test_txvalidator (SIGHASH_ALL|FORKID
// P2PKH, about 20-50 us per input): at 16 inputs parallel was 1.56x-1.84x
// faster on ubuntu-22.04 (x86-64, 4 vCPU), ubuntu-22.04-arm (4 cores),
// macos-15 CI (3 vCPU) and an Apple M3 Max; at 2-4 inputs the macos-15 runner
// was slower.
static_assert(bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD == 16);
std::atomic<size_t> parallelInputThreshold{ bsv::detail::DEFAULT_PARALLEL_INPUT_THRESHOLD };
// Test-only counters, written only while inputCountersEnabled (off by
// default); participation counters read the flag outside the input loop.
std::atomic<bool> inputCountersEnabled{ false };
std::atomic<uint64_t> helperVerifiedInputs{ 0 };
std::atomic<uint64_t> jobVerifiedInputs{ 0 };  // caller and helpers, inside jobs
std::atomic<uint64_t> helperJobParticipations{ 0 };  // helper Work() calls that verified an input
std::atomic<uint64_t> helperWakesWithoutCore{ 0 };
std::atomic<uint64_t> helperReleaseNotifications{ 0 };
std::atomic<uint64_t> inputCancellationSources{ 0 };
std::atomic<bsv::detail::InputHook> inputHook{ nullptr };  // tests only
std::atomic<bsv::detail::InputSourceAllocationHook> inputSourceAllocationHook{ nullptr };  // tests only
std::atomic<bsv::detail::ValidationStageHook> validationStageHook{ nullptr };  // tests only
std::atomic<bsv::detail::HelperReleasedCoreHook> helperReleasedCoreHook{ nullptr };  // tests only
std::atomic<bsv::detail::InputPoolHook> inputPoolHook{ nullptr };  // tests only
std::atomic<size_t> inputPoolCoresCap{ 0 };  // 0: no cap (tests only)
std::atomic<int64_t> parallelInputHelpers{ -1 };  // SetParallelInputHelpers; < 0: automatic

#if BDK_PARALLEL_INPUTS

// Each extra participant is worth at least this many inputs (fewer when the
// threshold is lower, so a tx at the threshold is offered a helper).
constexpr size_t MIN_INPUTS_PER_PARTICIPANT = 8;

std::atomic<size_t> busyThreads{ 0 };
thread_local bool threadIsBusy{ false };

// The most helpers that may be on jobs at once, from SetParallelInputHelpers.
size_t HelperLimit()
{
    const int64_t helpers = parallelInputHelpers.load(std::memory_order_relaxed);
    if (helpers < 0)
        return std::numeric_limits<size_t>::max();
    return static_cast<size_t>(std::min<uint64_t>(static_cast<uint64_t>(helpers), std::numeric_limits<size_t>::max()));
}

// One transaction's inputs [0, n), shared by its caller and the helpers that join.
class InputJob {
public:
    using VerifyInput = std::function<TxError(size_t, const task::CCancellationToken&)>;

    InputJob(size_t n, VerifyInput verify, const task::CCancellationToken& callerToken)
        : n_{ n }, verify_{ std::move(verify) }, callerToken_{ callerToken },
          lowestFailure_{ n }, lowestCancelled_{ n } {}

    // What a helper's Work() call checks before each claim.
    struct HelperTerms {
        size_t cores;                     // leave once more threads than this are busy
        std::atomic<size_t>* places;      // helpers holding a place under HelperLimit()
        std::atomic<uint64_t>* inputs;    // counts each input verified, or nullptr
        bool placeReturned = false;       // set when the helper left over the limit
    };

    // Verifies inputs until caller cancellation is observed, no input below
    // the lowest known failure is left, or,
    // for a helper, until more threads than cores are busy or more helpers
    // hold places than the helper limit allows; then the helper gives its
    // place back itself, so exactly the excess leaves. Both are checked
    // before claiming, never after, so leaving never skips an index this
    // thread claimed (claims at or above n_, or above the lowest failure, are
    // skipped as always). Returns how many inputs this call verified, or nullopt
    // if participant setup allocation failed before any input was claimed.
    std::optional<size_t> Work(HelperTerms* helper = nullptr)
    {
        size_t verified = 0;
        const bool count = inputCountersEnabled.load(std::memory_order_relaxed);
        const auto pollHook = inputHook.load(std::memory_order_relaxed);
        std::shared_ptr<bsv::detail::InputCancellationSource> cancellation;
        std::optional<task::CCancellationToken> inputToken;
        if (!HasWork() || (helper != nullptr && HelperShouldLeave(*helper)))
            return 0;
        // Participant setup must not become an input failure or consume an
        // index. A helper can leave; the caller drains then verifies serially.
        try {
            if (const auto hook = inputSourceAllocationHook.load(std::memory_order_relaxed))
                hook(helper != nullptr);
            cancellation = std::make_shared<bsv::detail::InputCancellationSource>(
                lowestFailure_, callerCancelled_, helper == nullptr ? &callerToken_ : nullptr, pollHook);
            if (count)
                inputCancellationSources.fetch_add(1, std::memory_order_relaxed);
            inputToken.emplace(cancellation->GetToken());
        } catch (const std::bad_alloc&) {
            // Do not spend helper CPU retrying allocation on this job. The
            // caller and participants already admitted can finish its inputs.
            if (helper != nullptr)
                helpersUnavailable_.store(true, std::memory_order_relaxed);
            return std::nullopt;
        }
        for (;;) {
            if (helper != nullptr && HelperShouldLeave(*helper))
                return verified;
            // Do not observe a late cancellation after all necessary work was
            // claimed or a lower failure already made this work unnecessary.
            if (!HasWork())
                return verified;
            if (helper == nullptr && callerToken_.IsCanceled()) {
                callerCancelled_.store(true, std::memory_order_relaxed);
                if (pollHook)
                    pollHook(next_.load(std::memory_order_relaxed), bsv::detail::InputEvent::CallerCancelled);
            }
            if (callerCancelled_.load(std::memory_order_relaxed))
                return verified;
            const size_t index = next_.fetch_add(1, std::memory_order_relaxed);
            if (index >= n_ || index > lowestFailure_.load(std::memory_order_relaxed))
                return verified;
            if (const auto hook = inputHook.load(std::memory_order_relaxed))
                hook(index, bsv::detail::InputEvent::Claimed);
            ++verified;
            try {
                cancellation->SetIndex(index);
                const TxError result = verify_(index, *inputToken);
                if (pollHook)
                    pollHook(index, bsv::detail::InputEvent::Verified);
                if (!bsv::TxErrorIsOk(result))
                    Fail(index, result, nullptr);
            } catch (...) {
                Fail(index, bsv::TxErrorException(), std::current_exception());
            }
            if (helper != nullptr && helper->inputs != nullptr)
                helper->inputs->fetch_add(1, std::memory_order_relaxed);
        }
    }

    size_t UnclaimedInputs() const
    {
        const size_t next = next_.load(std::memory_order_relaxed);
        return !callerCancelled_.load(std::memory_order_relaxed)
            && next < n_ && next <= lowestFailure_.load(std::memory_order_relaxed) ? n_ - next : 0;
    }

    bool HasWork() const { return UnclaimedInputs() != 0; }

    size_t HelperUnclaimedInputs() const
    {
        return helpersUnavailable_.load(std::memory_order_relaxed) ? 0 : UnclaimedInputs();
    }

    // A helper joins only an open job; the caller waits for every joined helper.
    bool TryJoin()
    {
        std::lock_guard lock{ mutex_ };
        if (closed_ || helpersUnavailable_.load(std::memory_order_relaxed))
            return false;
        ++joined_;
        return true;
    }

    void Leave()
    {
        std::lock_guard lock{ mutex_ };
        if (--joined_ == 0 && closed_)
            left_.notify_one();
    }

    // Called once the caller stops claiming. Drain joined helpers before
    // deciding whether all inputs needed for a verdict completed.
    void CloseAndWait()
    {
        std::unique_lock lock{ mutex_ };
        closed_ = true;
        if (const auto hook = inputPoolHook.load(std::memory_order_relaxed))
            hook(bsv::detail::InputPoolEvent::JobClosed);
        left_.wait(lock, [this] { return joined_ == 0; });
    }

    // A completed verdict, or cancellation if necessary inputs were abandoned.
    TxError Result()
    {
        std::lock_guard lock{ mutex_ };
        // Claims form a contiguous prefix, and every joined participant has
        // drained. A real failure is definitive only if no lower claim was
        // abandoned. Higher cancellations cannot replace that verdict.
        if (lowestFailure_.load(std::memory_order_relaxed) < lowestCancelled_) {
            if (failureException_)
                std::rethrow_exception(failureException_);
            return failure_;
        }
        if (lowestCancelled_ < n_ || next_.load(std::memory_order_relaxed) < n_)
            return bsv::TxErrorCancelled();
        return bsv::TxErrorOk();
    }

private:
    static bool HelperShouldLeave(HelperTerms& helper)
    {
        if (busyThreads.load(std::memory_order_relaxed) > helper.cores)
            return true;
        const size_t limit = HelperLimit();
        size_t onJobs = helper.places->load(std::memory_order_relaxed);
        while (onJobs > limit) {
            if (helper.places->compare_exchange_weak(onJobs, onJobs - 1, std::memory_order_relaxed)) {
                helper.placeReturned = true;
                return true;
            }
        }
        return false;
    }

    void Fail(size_t index, const TxError& result, std::exception_ptr exception)
    {
        if (const auto hook = inputHook.load(std::memory_order_relaxed))
            hook(index, bsv::detail::InputEvent::Failed);
        std::lock_guard lock{ mutex_ };
        if (result.domain == TX_ERR_DOMAIN_CANCELLED) {
            lowestCancelled_ = std::min(lowestCancelled_, index);
            return;
        }
        if (index >= lowestFailure_.load(std::memory_order_relaxed))
            return;
        lowestFailure_.store(index, std::memory_order_relaxed);
        failure_ = result;
        failureException_ = std::move(exception);
    }

    const size_t n_;
    const VerifyInput verify_;
    const task::CCancellationToken& callerToken_;
    std::atomic<bool> callerCancelled_{ false };  // only the caller polls its token
    std::atomic<bool> helpersUnavailable_{ false };  // setup allocation failed
    std::atomic<size_t> next_{ 0 };
    std::atomic<size_t> lowestFailure_;  // n_ while nothing failed; written under mutex_
    size_t lowestCancelled_;  // abandoned claims, under mutex_; never a real failure

    std::mutex mutex_;
    std::condition_variable left_;
    size_t joined_{ 0 };
    bool closed_{ false };
    TxError failure_{ bsv::TxErrorOk() };
    std::exception_ptr failureException_;
};

// Process-wide helper threads. Created on first use and never destroyed: a
// destructor joining them at exit could hang while other threads are still
// inside BDK.
class InputVerifyPool {
public:
    // nullptr when the machine has a single core or no helper could be started.
    static InputVerifyPool* Get()
    {
        static InputVerifyPool* const pool = Create();
        return pool;
    }

    // The pool if Get() already created it; never creates one.
    static InputVerifyPool* Existing() { return created_.load(std::memory_order_acquire); }

    size_t Helpers() const { return helpers_; }

    // Helpers that could join a job now: idle cores under the global budget,
    // and free places under the helper limit.
    size_t AvailableHelpers(size_t limit) const
    {
        const size_t busy = busyThreads.load(std::memory_order_seq_cst);
        const size_t onJobs = helpersOnJobs_.load(std::memory_order_relaxed);
        if (busy >= cores_ || onJobs >= limit)
            return 0;
        return std::min({ helpers_, cores_ - busy, limit - onJobs });
    }

    void Offer(const std::shared_ptr<InputJob>& job, size_t maxHelpers)
    {
        {
            std::lock_guard lock{ mutex_ };
            jobs_.push_back(job);
            openJobs_.fetch_add(1, std::memory_order_seq_cst);
        }
        // Publish before the decisive capacity read. A release that saw no
        // open job is visible here; a later release sees the published job
        // and notifies through ReleasedCore. Do not wake a genuinely full pool.
        const size_t helpers = std::min(AvailableHelpers(HelperLimit()), maxHelpers);
        for (size_t i = 0; i < helpers; ++i)
            wake_.notify_one();
    }

    void WakeHelpersForTesting(bool all)
    {
        std::lock_guard lock{ mutex_ };
        if (all) wake_.notify_all();
        else wake_.notify_one();
    }

    void Withdraw(const std::shared_ptr<InputJob>& job)
    {
        std::lock_guard lock{ mutex_ };
        const auto it = std::find(jobs_.begin(), jobs_.end(), job);
        if (it != jobs_.end()) {
            jobs_.erase(it);
            openJobs_.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    size_t QueuedJobs()
    {
        std::lock_guard lock{ mutex_ };
        return jobs_.size();
    }

    // A busy thread left, leaving `busyAfter` busy: a helper over budget may
    // now join. Nothing to do while the count is still at or above the core
    // count; the decrement that takes it below does the check. The leaving thread's
    // seq_cst decrement of busyThreads and this seq_cst load pair with Offer's
    // increment of openJobs_ and its subsequent load of busyThreads, so either
    // this sees the job or Offer sees the freed core and wakes a helper.
    // At the helper cap no notification can admit another helper. A temporary
    // reservation that made the cap look full is returned with a seq_cst core
    // recheck in Run(), so a skipped notification cannot lose that capacity.
    void ReleasedCore(size_t busyAfter)
    {
        if (busyAfter >= cores_ || openJobs_.load(std::memory_order_seq_cst) == 0
            || helpersOnJobs_.load(std::memory_order_seq_cst) >= HelperLimit())
            return;
        if (inputCountersEnabled.load(std::memory_order_relaxed))
            helperReleaseNotifications.fetch_add(1, std::memory_order_relaxed);
        { std::lock_guard lock{ mutex_ }; }  // a helper checking the budget is now waiting
        wake_.notify_one();
    }

private:
    InputVerifyPool(size_t cores) : cores_{ cores } {}

    static InputVerifyPool* Create()
    {
        // hardware_concurrency(), lowered on Linux to the container's cgroup
        // CPU limit and to the affinity mask of the thread that creates the
        // pool, i.e. of the first caller with a large tx; the size is kept for
        // the life of the process.
        size_t cores = 0;
        try {
            cores = bsv::detail::EffectiveCpuCount();
        } catch (const std::exception&) {
            // e.g. bad_alloc reading the cgroup files. Get() keeps this
            // nullptr, so the process stays serial: a lost speed-up, never a
            // failed call.
            return nullptr;
        }
        if (const size_t cap = inputPoolCoresCap.load(); cap != 0)
            cores = std::min(cores, cap);
        if (cores < 2)
            return nullptr;
        auto* pool = new (std::nothrow) InputVerifyPool(cores);
        if (pool == nullptr)
            return nullptr;
        for (size_t i = 0; i + 1 < cores && StartHelper(pool); ++i)
            ++pool->helpers_;
        if (pool->helpers_ == 0) {
            delete pool;
            return nullptr;
        }
        created_.store(pool, std::memory_order_release);
        return pool;
    }

    // Starts one detached helper thread running pool->Run(). On POSIX it gets
    // an INPUT_HELPER_STACK_BYTES stack: a caller's main thread usually has
    // 8 MiB, while a secondary thread's default can be far smaller (512 KiB
    // on macOS), and inputs used to run on the caller's stack.
    static bool StartHelper(InputVerifyPool* pool)
    {
#if defined(_WIN32)
        try {
            std::thread([pool] { pool->Run(); }).detach();
            return true;
        } catch (const std::exception&) {  // system_error, bad_alloc
            return false;
        }
#else
        pthread_attr_t attr;
        if (pthread_attr_init(&attr) != 0)
            return false;
        pthread_t thread;
        const bool started = pthread_attr_setstacksize(&attr, bsv::detail::INPUT_HELPER_STACK_BYTES) == 0
            && pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0
            && pthread_create(&thread, &attr, &InputVerifyPool::RunHelper, pool) == 0;
        pthread_attr_destroy(&attr);
        return started;
#endif
    }

#if !defined(_WIN32)
    static void* RunHelper(void* pool)
    {
        static_cast<InputVerifyPool*>(pool)->Run();
        return nullptr;
    }
#endif

    // A place under the helper limit, held while this helper is on a job.
    bool TryTakeHelperPlace()
    {
        const size_t limit = HelperLimit();
        size_t onJobs = helpersOnJobs_.load(std::memory_order_relaxed);
        while (onJobs < limit) {
            if (helpersOnJobs_.compare_exchange_weak(onJobs, onJobs + 1, std::memory_order_relaxed))
                return true;
        }
        return false;
    }

    static bool TryTakeCore(size_t cores)
    {
        size_t busy = busyThreads.load(std::memory_order_seq_cst);
        while (busy < cores) {
            if (busyThreads.compare_exchange_weak(busy, busy + 1, std::memory_order_seq_cst))
                return true;
        }
        return false;
    }

    // Prefer the most unclaimed inputs; retain queue order for equal counts.
    std::shared_ptr<InputJob> NextJob()
    {
        while (!jobs_.empty() && !jobs_.front()->HasWork()) {
            jobs_.pop_front();
            openJobs_.fetch_sub(1, std::memory_order_relaxed);
        }
        std::shared_ptr<InputJob> largest;
        size_t most = 0;
        for (const auto& job : jobs_) {
            const size_t remaining = job->HelperUnclaimedInputs();
            if (remaining > most) {
                most = remaining;
                largest = job;
            }
        }
        return largest;
    }

    void Run()
    {
        threadIsBusy = true;  // a helper's own work never counts twice
        for (;;) {
            std::shared_ptr<InputJob> job;
            {
                std::unique_lock lock{ mutex_ };
                wake_.wait(lock, [&] {
                    job = NextJob();
                    while (job && TryTakeHelperPlace()) {
                        if (TryTakeCore(cores_))
                            return true;
                        if (const auto hook = inputPoolHook.load(std::memory_order_relaxed))
                            hook(bsv::detail::InputPoolEvent::CoreUnavailable);
                        // Pair the returned reservation and the capacity check
                        // with ReleasedCore's seq_cst helper-cap load. If that
                        // release saw our reserved place as full, this check
                        // sees its freed core; retry before going to sleep.
                        helpersOnJobs_.fetch_sub(1, std::memory_order_seq_cst);
                        if (inputCountersEnabled.load(std::memory_order_relaxed))
                            helperWakesWithoutCore.fetch_add(1, std::memory_order_relaxed);
                        if (busyThreads.load(std::memory_order_seq_cst) >= cores_)
                            break;
                    }
                    job.reset();  // don't keep a finished job alive while asleep
                    if (const auto hook = inputPoolHook.load(std::memory_order_relaxed))
                        hook(bsv::detail::InputPoolEvent::Waiting);
                    return false;
                });
            }
            const bool count = inputCountersEnabled.load(std::memory_order_relaxed);
            InputJob::HelperTerms terms{ cores_, &helpersOnJobs_, count ? &helperVerifiedInputs : nullptr };
            if (job->TryJoin()) {
                const size_t verified = job->Work(&terms).value_or(0);
                if (count) {
                    if (verified > 0)
                        helperJobParticipations.fetch_add(1, std::memory_order_relaxed);
                    jobVerifiedInputs.fetch_add(verified, std::memory_order_relaxed);
                }
                job->Leave();
            }
            job.reset();
            if (!terms.placeReturned)
                helpersOnJobs_.fetch_sub(1, std::memory_order_relaxed);
            const size_t busyAfter = busyThreads.fetch_sub(1, std::memory_order_seq_cst) - 1;
            ReleasedCore(busyAfter);
            if (const auto hook = helperReleasedCoreHook.load(std::memory_order_relaxed))
                hook();
        }
    }

    const size_t cores_;
    size_t helpers_{ 0 };
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::shared_ptr<InputJob>> jobs_;
    std::atomic<size_t> openJobs_{ 0 };
    std::atomic<size_t> helpersOnJobs_{ 0 };  // helpers holding a place under HelperLimit()

    static inline std::atomic<InputVerifyPool*> created_{ nullptr };
};

// Counts the calling thread in busyThreads for the duration of one
// VerifyScript, ValidateTransaction or VerifySpend call after its entry poll,
// unless it is already counted.
class BusyScope {
public:
    BusyScope() : counted_{ !threadIsBusy }
    {
        if (counted_) {
            threadIsBusy = true;
            busyThreads.fetch_add(1, std::memory_order_seq_cst);
        }
    }
    ~BusyScope()
    {
        if (!counted_)
            return;
        threadIsBusy = false;
        const size_t busyAfter = busyThreads.fetch_sub(1, std::memory_order_seq_cst) - 1;
        if (InputVerifyPool* pool = InputVerifyPool::Existing())
            pool->ReleasedCore(busyAfter);
    }
    BusyScope(const BusyScope&) = delete;
    BusyScope& operator=(const BusyScope&) = delete;

private:
    const bool counted_;
};

// The caller's side of a queued job: it always closes the job, waits for the
// helpers that joined and withdraws it, also when its own Work() throws, since
// joined helpers read the caller's stack.
class JobCaller {
public:
    JobCaller(InputVerifyPool& pool, std::shared_ptr<InputJob> job) : pool_{ pool }, job_{ std::move(job) } {}
    ~JobCaller()
    {
        job_->CloseAndWait();
        pool_.Withdraw(job_);
    }
    JobCaller(const JobCaller&) = delete;
    JobCaller& operator=(const JobCaller&) = delete;

private:
    InputVerifyPool& pool_;
    const std::shared_ptr<InputJob> job_;
};

#endif // BDK_PARALLEL_INPUTS

} // anonymous namespace

void bsv::SetParallelInputHelpers(int64_t helpers)
{
    parallelInputHelpers.store(helpers < 0 ? -1 : helpers, std::memory_order_relaxed);
}

int64_t bsv::GetParallelInputHelpers()
{
    return parallelInputHelpers.load(std::memory_order_relaxed);
}

size_t bsv::detail::SetParallelInputThresholdForTesting(size_t minInputs)
{
    return parallelInputThreshold.exchange(minInputs);
}

uint64_t bsv::detail::HelperVerifiedInputCountForTesting()
{
    return helperVerifiedInputs.load();
}

bool bsv::detail::SetInputCountersEnabledForTesting(bool enabled)
{
    return inputCountersEnabled.exchange(enabled);
}

uint64_t bsv::detail::HelperWakesWithoutCoreForTesting()
{
    return helperWakesWithoutCore.load();
}

uint64_t bsv::detail::HelperReleaseNotificationsForTesting()
{
    return helperReleaseNotifications.load();
}

uint64_t bsv::detail::HelperJobParticipationCountForTesting()
{
    return helperJobParticipations.load();
}

uint64_t bsv::detail::InputCancellationSourceCountForTesting()
{
    return inputCancellationSources.load();
}

bsv::detail::InputSourceAllocationHook bsv::detail::SetInputSourceAllocationHookForTesting(InputSourceAllocationHook hook)
{
    return inputSourceAllocationHook.exchange(hook);
}

bsv::detail::ValidationStageHook bsv::detail::SetValidationStageHookForTesting(ValidationStageHook hook)
{
    return validationStageHook.exchange(hook);
}

bsv::detail::InputHook bsv::detail::SetInputHookForTesting(InputHook hook)
{
    return inputHook.exchange(hook);
}

bsv::detail::HelperReleasedCoreHook bsv::detail::SetHelperReleasedCoreHookForTesting(HelperReleasedCoreHook hook)
{
    return helperReleasedCoreHook.exchange(hook);
}

bsv::detail::InputPoolHook bsv::detail::SetInputPoolHookForTesting(InputPoolHook hook)
{
    return inputPoolHook.exchange(hook);
}

void bsv::detail::WakeInputHelpersForTesting(bool all)
{
#if BDK_PARALLEL_INPUTS
    if (InputVerifyPool* pool = InputVerifyPool::Existing())
        pool->WakeHelpersForTesting(all);
#else
    (void)all;
#endif
}

uint64_t bsv::detail::JobVerifiedInputCountForTesting()
{
    return jobVerifiedInputs.load();
}

size_t bsv::detail::SetInputPoolCoresForTesting(size_t cores)
{
    return inputPoolCoresCap.exchange(cores);
}

size_t bsv::detail::BusyThreadCountForTesting()
{
#if BDK_PARALLEL_INPUTS
    return busyThreads.load();
#else
    return 0;
#endif
}

size_t bsv::detail::QueuedJobCountForTesting()
{
#if BDK_PARALLEL_INPUTS
    if (InputVerifyPool* pool = InputVerifyPool::Existing())
        return pool->QueuedJobs();
#endif
    return 0;
}

size_t bsv::detail::InputPoolHelperCountForTesting()
{
#if BDK_PARALLEL_INPUTS
    if (InputVerifyPool* pool = InputVerifyPool::Get())
        return pool->Helpers();
#endif
    return 0;
}

void bsv::detail::AddBusyThreadsForTesting(size_t count)
{
#if BDK_PARALLEL_INPUTS
    busyThreads.fetch_add(count);
#else
    (void)count;
#endif
}

void bsv::detail::ReleaseBusyThreadsForTesting(size_t count)
{
#if BDK_PARALLEL_INPUTS
    for (size_t i = 0; i < count; ++i) {
        const size_t busyAfter = busyThreads.fetch_sub(1) - 1;
        if (InputVerifyPool* pool = InputVerifyPool::Existing())
            pool->ReleasedCore(busyAfter);
    }
#else
    (void)count;
#endif
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  End of file-local helpers — everything below is the public CTxValidator   //
//  implementation.                                                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// A UTXO height can be MEMPOOL_HEIGHT (0x7FFFFFFF) when the UTXO being spent has not yet
// been confirmed in a block — i.e. the parent transaction is still in the mempool. The svnode
// uses this sentinel to tell BDK "I don't have a block height for this UTXO yet."
//
// GetProtocolEra throws when passed MEMPOOL_HEIGHT because it cannot determine which
// protocol era an unconfirmed UTXO belongs to. To handle this, we substitute blockHeight+1
// (the next candidate block) for policy validation: this is the block the unconfirmed UTXO
// would be confirmed in if mined next, so its era is correctly evaluated against that height.
//
// In consensus mode (block validation), a MEMPOOL_HEIGHT UTXO is conceptually invalid —
// a block cannot spend an output that has no confirmed height — so callers return a typed
// error (UnconfirmedInputInBlock) before this helper is reached.
static int32_t resolveUtxoEraHeight(int32_t utxoHeight, int32_t nextBlockHeight) {
    return (utxoHeight == MEMPOOL_HEIGHT) ? nextBlockHeight : utxoHeight;
}

static Consensus::Params consensusParameters(bsv::TxValidationNetwork network)
{
    Consensus::Params params{};
    switch(network) {
    case bsv::TxValidationNetwork::Main:
        params.BIP65Height = 388381;
        params.BIP66Height = 363725;
        params.CSVHeight = 419328;
        params.uahfHeight = 478558;
        params.daaHeight = 504031;
        params.genesisHeight = 620538;
        params.chronicleHeight = 943816;
        params.p2shHeight = 173805;
        break;
    case bsv::TxValidationNetwork::Test:
        params.BIP65Height = 581885;
        params.BIP66Height = 330776;
        params.CSVHeight = 770112;
        params.uahfHeight = 1155875;
        params.daaHeight = 1188697;
        params.genesisHeight = 1344302;
        params.chronicleHeight = 1713168;
        params.p2shHeight = 519;
        break;
    case bsv::TxValidationNetwork::Stn:
        params.uahfHeight = 15;
        params.daaHeight = 2200;
        params.genesisHeight = 100;
        params.chronicleHeight = 250;
        params.p2shHeight = 1;
        break;
    case bsv::TxValidationNetwork::Regtest:
        params.BIP65Height = 1351;
        params.BIP66Height = 1251;
        params.CSVHeight = 576;
        params.genesisHeight = 10000;
        params.chronicleHeight = 15000;
        params.p2shHeight = 1;
        break;
    case bsv::TxValidationNetwork::TeraTestnet:
        params.genesisHeight = 1;
        params.chronicleHeight = 2;
        params.p2shHeight = 519;
        break;
    case bsv::TxValidationNetwork::TeraScalingTestnet:
        params.genesisHeight = 1;
        params.chronicleHeight = 2;
        params.p2shHeight = 1;
        break;
    }
    return params;
}

bsv::CTxValidator::CTxValidator(const std::string chainName)
    : source{ task::CCancellationSource::Make() }
{
    // Before the chain params below, which already hash the genesis block
    bsv::SelectSha256Backend();

    // SV's CStnParams does not initialize BIP65Height, BIP66Height or
    // CSVHeight. Reading those indeterminate fields makes script flags depend
    // on the compiler and whatever happened to occupy the object storage.
    // Normalize STN through the explicit verifier table used by the compact
    // constructor. Other built-in and registered custom chains continue to
    // use their complete CChainParams definitions.
    const bool isStn = chainName == CBaseChainParams::STN;
    const auto chainParams = isStn ? nullptr : bsv::CreateCustomChainParams(chainName);
    consensusParams = isStn
        ? consensusParameters(TxValidationNetwork::Stn)
        : chainParams->GetConsensus();
    std::string errStr;
    bool ok {true};

    policySettings.SetRequireStandard(isStn ? false : chainParams->RequireStandard());

    ok = ok && this->SetGenesisActivationHeight(consensusParams.genesisHeight, &errStr);
    ok = ok && this->SetChronicleActivationHeight(consensusParams.chronicleHeight, &errStr);
    if (!(ok || errStr.empty())){
        throw std::runtime_error("error setting genesis and chronicle heights " + errStr);
    }
}

bsv::CTxValidator::CTxValidator(TxValidationNetwork network)
    : CTxValidator(consensusParameters(network), network == TxValidationNetwork::Main)
{}

bsv::CTxValidator::CTxValidator(
    const Consensus::Params& consensusParamsIn,
    bool requireStandard)
    : consensusParams{consensusParamsIn}
    , source{task::CCancellationSource::Make()}
{
    bsv::SelectSha256Backend();

    std::string errStr;
    bool ok {true};

    policySettings.SetRequireStandard(requireStandard);

    ok = ok && this->SetGenesisActivationHeight(consensusParams.genesisHeight, &errStr);
    ok = ok && this->SetChronicleActivationHeight(consensusParams.chronicleHeight, &errStr);
    if (!(ok || errStr.empty())){
        throw std::runtime_error("error setting genesis and chronicle heights " + errStr);
    }
}

bool bsv::CTxValidator::SetMaxOpsPerScriptPolicy(int64_t maxOpsPerScriptPolicyIn, std::string* err)
{
    return policySettings.SetMaxOpsPerScriptPolicy(maxOpsPerScriptPolicyIn, err);
}

bool bsv::CTxValidator::SetMaxScriptNumLengthPolicy(int64_t maxScriptNumLengthIn, std::string* err)
{
    return policySettings.SetMaxScriptNumLengthPolicy(maxScriptNumLengthIn, err);
}

bool bsv::CTxValidator::SetMaxScriptSizePolicy(int64_t maxScriptSizePolicyIn, std::string* err)
{
    return policySettings.SetMaxScriptSizePolicy(maxScriptSizePolicyIn, err);
}

bool bsv::CTxValidator::SetMaxPubKeysPerMultiSigPolicy(int64_t maxPubKeysPerMultiSigIn, std::string* err)
{
    return policySettings.SetMaxPubKeysPerMultiSigPolicy(maxPubKeysPerMultiSigIn, err);
}

bool bsv::CTxValidator::SetMaxStackMemoryUsage(int64_t maxStackMemoryUsageConsensusIn, int64_t maxStackMemoryUsagePolicyIn, std::string* err)
{
    return policySettings.SetMaxStackMemoryUsage(maxStackMemoryUsageConsensusIn, maxStackMemoryUsagePolicyIn, err);
}

bool bsv::CTxValidator::SetGenesisActivationHeight(int32_t genesisActivationHeightIn, std::string* err)
{
    bool ok{true};
    ok = ok && policySettings.SetGenesisActivationHeight(genesisActivationHeightIn, err);

    // Chronicle height should be always at least genesis height
    auto chronicleHeight = policySettings.GetChronicleActivationHeight();
    if ( chronicleHeight < policySettings.GetGenesisActivationHeight() ) {
        ok = ok && policySettings.SetChronicleActivationHeight(genesisActivationHeightIn, err);
    }

    return ok;
}

bool bsv::CTxValidator::SetChronicleActivationHeight(int32_t chronicleActivationHeightIn, std::string* err)
{
    bool ok{true};
    ok = ok && policySettings.SetChronicleActivationHeight(chronicleActivationHeightIn, err);

    // Chronicle height should be always at least genesis height
    auto genesisHeight = policySettings.GetGenesisActivationHeight();
    if ( policySettings.GetChronicleActivationHeight() < genesisHeight ) {
        ok = ok && policySettings.SetGenesisActivationHeight(chronicleActivationHeightIn, err);
    }

    return ok;
}

uint64_t bsv::CTxValidator::GetMaxOpsPerScript(bool isGenesisEnabled, bool consensus) const
{
    return policySettings.GetMaxOpsPerScript(isGenesisEnabled, consensus);
}

uint64_t bsv::CTxValidator::GetMaxScriptNumLength(bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus) const
{
    if (isGenesisEnabled && isChronicleEnabled) {
        throw std::runtime_error("protocol should not be both Genesis and Chronicle");
    }

    ProtocolEra era {ProtocolEra::PreGenesis};
    if (isGenesisEnabled) {
        era = ProtocolEra::PostGenesis;
    }

    if (isChronicleEnabled) {
        era = ProtocolEra::PostChronicle;
    }

    return policySettings.GetMaxScriptNumLength(era, isConsensus);
}

uint64_t bsv::CTxValidator::GetMaxScriptSize(bool isGenesisEnabled, bool isConsensus) const
{
    return policySettings.GetMaxScriptSize(isGenesisEnabled, isConsensus);
}

uint64_t bsv::CTxValidator::GetMaxPubKeysPerMultiSig(bool isGenesisEnabled, bool isConsensus) const
{
    return policySettings.GetMaxPubKeysPerMultiSig(isGenesisEnabled, isConsensus);
}

uint64_t bsv::CTxValidator::GetMaxStackMemoryUsage(bool isGenesisEnabled, bool isConsensus) const
{
    return policySettings.GetMaxStackMemoryUsage(isGenesisEnabled, isConsensus);
}

int32_t bsv::CTxValidator::GetGenesisActivationHeight() const
{
    return policySettings.GetGenesisActivationHeight();
}

int32_t bsv::CTxValidator::GetChronicleActivationHeight() const
{
    return policySettings.GetChronicleActivationHeight();
}

uint64_t bsv::CTxValidator::GetGenesisGracefulPeriod() const
{
    return policySettings.GetGenesisGracefulPeriod();
}

uint64_t bsv::CTxValidator::GetChronicleGracefulPeriod() const
{
    return policySettings.GetChronicleGracefulPeriod();
}

bool bsv::CTxValidator::SetGenesisGracefulPeriod(int64_t genesisGracefulPeriodIn, std::string* err)
{
    return policySettings.SetGenesisGracefulPeriod(genesisGracefulPeriodIn, err);
}

bool bsv::CTxValidator::SetChronicleGracefulPeriod(int64_t chronicleGracefulPeriodIn, std::string* err)
{
    return policySettings.SetChronicleGracefulPeriod(chronicleGracefulPeriodIn, err);
}

bool bsv::CTxValidator::SetMaxTxSizePolicy(int64_t value, std::string* err)
{
    return policySettings.SetMaxTxSizePolicy(value, err);
}

void bsv::CTxValidator::SetDataCarrierSize(uint64_t dataCarrierSize)
{
    policySettings.SetDataCarrierSize(dataCarrierSize);
}

void bsv::CTxValidator::SetDataCarrier(bool dataCarrier)
{
    policySettings.SetDataCarrier(dataCarrier);
}

void bsv::CTxValidator::SetAcceptNonStandardOutput(bool accept)
{
    policySettings.SetAcceptNonStandardOutput(accept);
}

void bsv::CTxValidator::SetRequireStandard(bool require)
{
    policySettings.SetRequireStandard(require);
}

void bsv::CTxValidator::SetPermitBareMultisig(bool permit)
{
    policySettings.SetPermitBareMultisig(permit);
}

void bsv::CTxValidator::ResetDefault()
{
    policySettings.ResetDefault();
    consolidationMinFactor = 20;
    consolidationMaxInputScriptSize = 150;
    consolidationMinConf = 6;
    consolidationAcceptNonStd = false;
    maxSigOpsPolicy = MAX_TX_SIGOPS_COUNT_POLICY_BEFORE_GENESIS;
    maxSigOpsPostGenesisPolicy = MAX_TX_SIGOPS_COUNT_POLICY_AFTER_GENESIS;
    minMiningTxFeeSatPerKB = 0;
}

void bsv::CTxValidator::SetMaxSigOpsPolicy(uint64_t value) { maxSigOpsPolicy = value; }
uint64_t bsv::CTxValidator::GetMaxSigOpsPolicy() const { return maxSigOpsPolicy; }
bool bsv::CTxValidator::SetMaxSigOpsPostGenesisPolicy(int64_t value, std::string* err)
{
    if (value < 0) {
        if (err) *err = "Post-Genesis sigops policy limit cannot be negative";
        return false;
    }
    if (static_cast<uint64_t>(value) > MAX_TX_SIGOPS_COUNT_POLICY_AFTER_GENESIS) {
        if (err) *err = "Post-Genesis sigops policy limit exceeds maximum (" + std::to_string(MAX_TX_SIGOPS_COUNT_POLICY_AFTER_GENESIS) + ")";
        return false;
    }
    // 0 means reset to default (UINT32_MAX), mirroring bitcoin-sv SetMaxTxSigOpsCountPolicy.
    maxSigOpsPostGenesisPolicy = (value == 0) ? MAX_TX_SIGOPS_COUNT_POLICY_AFTER_GENESIS
                                              : static_cast<uint64_t>(value);
    return true;
}
uint64_t bsv::CTxValidator::GetMaxSigOpsPostGenesisPolicy() const { return maxSigOpsPostGenesisPolicy; }

// Consolidation setters mirror bitcoin-sv GlobalConfig::Set* (config.cpp:318/333/352):
// signed int64 input, reject negatives via error channel. Zero handling matches:
// MinConsolidationFactor stores 0 literally (disables consolidation; see implIsFreeConsolidation),
// while MaxConsolidationInputScriptSize and MinConfConsolidationInput map 0 → svnode default.
bool bsv::CTxValidator::SetMinConsolidationFactor(int64_t value, std::string* err)
{
    if (value < 0) {
        if (err) *err = "Minimum consolidation factor cannot be less than zero.";
        return false;
    }
    consolidationMinFactor = static_cast<uint64_t>(value);
    return true;
}
bool bsv::CTxValidator::SetMaxConsolidationInputScriptSize(int64_t value, std::string* err)
{
    if (value < 0) {
        if (err) *err = "Maximum length for a scriptSig input in a consolidation txn cannot be less than zero.";
        return false;
    }
    consolidationMaxInputScriptSize = (value == 0) ? uint64_t{150} : static_cast<uint64_t>(value);
    return true;
}
bool bsv::CTxValidator::SetMinConfConsolidationInput(int64_t value, std::string* err)
{
    if (value < 0) {
        if (err) *err = "Minimum number of confirmations of inputs spent by consolidation transactions cannot be less than 0";
        return false;
    }
    consolidationMinConf = (value == 0) ? uint64_t{6} : static_cast<uint64_t>(value);
    return true;
}
void bsv::CTxValidator::SetAcceptNonStdConsolidationInput(bool value) { consolidationAcceptNonStd = value; }

// Static-fee-path policy setter. 0 = no fee policy (every tx passes the fee floor),
// matching teranode's Policy.MinMiningTxFee = 0 sentinel. Negative is rejected.
bool bsv::CTxValidator::SetMinMiningTxFee(int64_t satoshisPerKB, std::string* err)
{
    if (satoshisPerKB < 0) {
        if (err) *err = "Minimum mining transaction fee rate (satoshis/kB) cannot be negative.";
        return false;
    }
    minMiningTxFeeSatPerKB = satoshisPerKB;
    return true;
}
int64_t bsv::CTxValidator::GetMinMiningTxFee() const { return minMiningTxFeeSatPerKB; }

uint64_t bsv::CTxValidator::GetMinConsolidationFactor() const { return consolidationMinFactor; }
uint64_t bsv::CTxValidator::GetMaxConsolidationInputScriptSize() const { return consolidationMaxInputScriptSize; }
uint64_t bsv::CTxValidator::GetMinConfConsolidationInput() const { return consolidationMinConf; }
bool bsv::CTxValidator::GetAcceptNonStdConsolidationInput() const { return consolidationAcceptNonStd; }

uint64_t bsv::CTxValidator::GetMaxTxSize(bool isGenesisEnabled, bool isChronicleEnabled, bool isConsensus) const
{
    if (isGenesisEnabled && isChronicleEnabled) {
        throw std::runtime_error("protocol should not be both Genesis and Chronicle");
    }

    ProtocolEra era{ProtocolEra::PreGenesis};
    if (isGenesisEnabled) {
        era = ProtocolEra::PostGenesis;
    }
    if (isChronicleEnabled) {
        era = ProtocolEra::PostChronicle;
    }

    return policySettings.GetMaxTxSize(era, isConsensus);
}

uint64_t bsv::CTxValidator::GetDataCarrierSize() const
{
    return policySettings.GetDataCarrierSize();
}

bool bsv::CTxValidator::GetDataCarrier() const
{
    return policySettings.GetDataCarrier();
}

bool bsv::CTxValidator::GetAcceptNonStandardOutput(bool isGenesisEnabled, bool isChronicleEnabled) const
{
    if (isGenesisEnabled && isChronicleEnabled) {
        throw std::runtime_error("protocol should not be both Genesis and Chronicle");
    }

    ProtocolEra era{ProtocolEra::PreGenesis};
    if (isGenesisEnabled) {
        era = ProtocolEra::PostGenesis;
    }
    if (isChronicleEnabled) {
        era = ProtocolEra::PostChronicle;
    }

    return policySettings.GetAcceptNonStandardOutput(era);
}

bool bsv::CTxValidator::GetRequireStandard() const
{
    return policySettings.GetRequireStandard();
}

bool bsv::CTxValidator::GetPermitBareMultisig() const
{
    return policySettings.GetPermitBareMultisig();
}

uint32_t bsv::CTxValidator::CalculateFlags(int32_t utxoHeight, int32_t blockHeight, bool consensus) const
{
    ProtocolEra era;
    uint32_t protocolFlags;
    if (consensus) {
        // For tx coming from a block.
        // blockHeight is the block the tx belong to
        // In that case, the chainTip is blockHeight-1
        era = GetProtocolEra(policySettings, blockHeight );
        protocolFlags = GetBlockScriptFlags(consensusParams, blockHeight - 1 , era);
    } else {
        // For tx coming from a peer
        // blockHeight is the chain tip (highest current block)
        era = GetProtocolEra(policySettings, blockHeight + 1);
        const bool requireStandard = policySettings.GetRequireStandard();
        protocolFlags = GetScriptVerifyFlags(era, requireStandard);
    }

    ProtocolEra utxoEra{ GetProtocolEra(policySettings, resolveUtxoEraHeight(utxoHeight, blockHeight + 1)) };
    const uint32_t utxoFlags { InputScriptVerifyFlags(era, utxoEra) };

    return (protocolFlags | utxoFlags);
}

// The implementation of this method replicate the bsv method ::GetTransactionSigOpCount
// where inside it has GetSigOpCountWithoutP2SH and GetP2SHSigOpCount
// These methods are all implemented in src/validation.cpp that we can not have it because
// of the huge dependencies
//
// So we replicate them here
uint64_t bsv::CTxValidator::GetSigOpCount(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool countP2SHSigOps, bool consensus) const {
    if (consensus) {
        for (auto h : utxoHeights) {
            if (h == MEMPOOL_HEIGHT)
                throw std::runtime_error("MEMPOOL_HEIGHT UTXO is invalid in consensus sigop counting");
        }
    }

    const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
    const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
    CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
    bsv::CMutableTransactionExtended eTX;
    tx_stream >> eTX;

    if (!tx_stream.empty()) {
        throw std::runtime_error("error serializing extended tx");
    }

    const CTransaction ctx(eTX.mtx);
    const int32_t eraHeight = consensus ? blockHeight : blockHeight + 1;
    const ProtocolEra era = GetProtocolEra(policySettings, eraHeight);
    return implGetSigOpCount(ctx, eTX.vutxo, utxoHeights, era, eraHeight, countP2SHSigOps);
}

uint64_t bsv::CTxValidator::implGetSigOpCount(
    const CTransaction& ctx,
    const std::vector<CTxOut>& prevUTXO,
    std::span<const int32_t> utxoHeights,
    ProtocolEra era,
    int32_t nextBlockHeight,
    bool countP2SHSigOps) const
{

    // The part GetSigOpCountWithoutP2SH //////////////////////////////////////////////////
    bool sigOpCountError = false;
    uint64_t nSigOpsWithoutP2SH {0};
    for (const auto& txin : ctx.vin)
    {
        // After Genesis, this should return 0, since only push data is allowed in input scripts:
        nSigOpsWithoutP2SH += txin.scriptSig.GetSigOpCount(false, era, sigOpCountError);
        if (sigOpCountError) {
            throw std::runtime_error("error calculating sigop count without P2SH for scriptSigs");
        }
    }
    for (const auto& txout : ctx.vout)
    {
        nSigOpsWithoutP2SH += txout.scriptPubKey.GetSigOpCount(false, era, sigOpCountError);
        if (sigOpCountError) {
            throw std::runtime_error("error calculating sigop count without P2SH for scriptPubKeys");
        }
    }

    if (ctx.IsCoinBase()) {
        return nSigOpsWithoutP2SH;
    }

    // The part GetP2SHSigOpCount //////////////////////////////////////////////////
    // countP2SHSigOps mirrors the bitcoin-sv fP2SH parameter in GetTransactionSigOpCount:
    // in the consensus path it is derived from the block's SCRIPT_VERIFY_P2SH flag
    // (GetBlockScriptFlags), not from the UTXO era. In the policy path it is always true
    // and the per-UTXO era check below handles the genesis cutoff.
    if (!countP2SHSigOps)
        return nSigOpsWithoutP2SH;

    if (prevUTXO.size() != ctx.vin.size()) {
        throw std::runtime_error("inconsistent inputs size");
    }
    if (prevUTXO.size() != utxoHeights.size()) {
        throw std::runtime_error("inconsistent utxo heights and number of utxo");
    }

    uint64_t nSigOpsP2SH{0};
    for (size_t index = 0; index < prevUTXO.size(); ++index) {
        const ProtocolEra utxoEra = GetProtocolEra(policySettings, resolveUtxoEraHeight(utxoHeights[index], nextBlockHeight));
        if (IsProtocolActive(utxoEra, ProtocolName::Genesis)) {
            continue;
        }

        const CTxOut& prevout = prevUTXO[index];
        const CTxIn& input = ctx.vin[index];
        if (IsP2SH(prevout.scriptPubKey)) {
            nSigOpsP2SH += prevout.scriptPubKey.GetSigOpCount(input.scriptSig, utxoEra, sigOpCountError);
            if (sigOpCountError) {
                throw std::runtime_error("error calculating P2SH sigop count");
            }
        }
    }

    return (nSigOpsWithoutP2SH + nSigOpsP2SH);
}


TxError bsv::CTxValidator::VerifyScript(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool consensus, std::span<const uint32_t> customFlags) const
{
    return VerifyScript(extendedTX, utxoHeights, blockHeight, consensus, customFlags, source->GetToken());
}

TxError bsv::CTxValidator::VerifyScript(std::span<const uint8_t> extendedTX, std::span<const int32_t> utxoHeights, int32_t blockHeight, bool consensus, std::span<const uint32_t> customFlags, const task::CCancellationToken& token) const
{
    if (token.IsCanceled())
        return bsv::TxErrorCancelled();

#if BDK_PARALLEL_INPUTS
    const BusyScope busy;  // count deserialization, hashing and transaction checks
#endif

    try {
        if (const auto hook = validationStageHook.load(std::memory_order_relaxed))
            hook(bsv::detail::ValidationStage::Deserializing);
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;

        if (!tx_stream.empty())
            throw std::runtime_error("error serializing extended tx");

        const CTransaction ctx(eTX.mtx);
        // Deserialising is linear in the transaction size with no poll inside;
        // check before implVerifyScript's argument checks can throw. It polls again
        // once it has built the shared signature-hash data.
        if (token.IsCanceled())
            return bsv::TxErrorCancelled();
        return implVerifyScript(ctx, eTX.vutxo, utxoHeights, blockHeight, consensus, customFlags, token);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::VerifySpend(
    std::span<const uint8_t> transaction,
    uint32_t inputIndex,
    std::span<const uint8_t> lockingScript,
    int64_t sourceSatoshis,
    int32_t utxoHeight,
    int32_t blockHeight,
    bool consensus,
    std::optional<uint32_t> customFlags
) const
{
    return VerifySpend(transaction, inputIndex, lockingScript, sourceSatoshis, utxoHeight, blockHeight,
                       consensus, customFlags, source->GetToken());
}

TxError bsv::CTxValidator::VerifySpend(
    std::span<const uint8_t> transaction,
    uint32_t inputIndex,
    std::span<const uint8_t> lockingScript,
    int64_t sourceSatoshis,
    int32_t utxoHeight,
    int32_t blockHeight,
    bool consensus,
    std::optional<uint32_t> customFlags,
    const task::CCancellationToken& token
) const
{
    if (token.IsCanceled())
        return bsv::TxErrorCancelled();

#if BDK_PARALLEL_INPUTS
    const BusyScope busy;  // one input, verified here, but it takes a core
#endif

    try {
        if (sourceSatoshis < 0)
            throw std::runtime_error("source satoshis cannot be negative");

        const char* begin{ reinterpret_cast<const char*>(transaction.data()) };
        const char* end{ reinterpret_cast<const char*>(transaction.data() + transaction.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        CMutableTransaction mutableTx;
        tx_stream >> mutableTx;

        if (!tx_stream.empty())
            throw std::runtime_error("error serializing transaction");

        const CTransaction tx(std::move(mutableTx));
        // Deserialising is linear in the transaction size with no poll inside;
        // check before the checks below can return a result.
        if (token.IsCanceled())
            return bsv::TxErrorCancelled();
        if (inputIndex >= tx.vin.size())
            throw std::runtime_error("input index is out of range");
        if (consensus && utxoHeight == MEMPOOL_HEIGHT)
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::UnconfirmedInputInBlock));

        const uint32_t flags = customFlags.value_or(CalculateFlags(utxoHeight, blockHeight, consensus));
        const CScript script(lockingScript.begin(), lockingScript.end());
        const Amount amount{ sourceSatoshis };
        // One input can run several FORKID checks (multisig, contracts with
        // several CHECKSIGs); they all read these hashes.
        const PrecomputedTransactionData txdata(tx);
        // Copying the locking script and building txdata are linear in their size
        // with no poll inside.
        if (token.IsCanceled())
            return bsv::TxErrorCancelled();
        CachingScriptChecker checker(&tx, inputIndex, amount, txdata);
        return bsvVerifyScript(tx.vin[inputIndex].scriptSig, script, consensus, flags, checker, token);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

std::vector<TxError> bsv::CTxValidator::ValidateBatch(const bsv::ValidateBatch& batch) const
{
    return ValidateBatch(batch, source->GetToken());
}

std::vector<TxError> bsv::CTxValidator::ValidateBatch(const bsv::ValidateBatch& batch, const task::CCancellationToken& token) const
{
    std::vector<TxError> results;
    results.reserve(batch.size());

    for (const auto& elem : batch) {
        results.push_back(ValidateTransaction(
            elem.extendedTX,
            elem.utxoHeights,
            elem.blockHeight,
            elem.consensus,
            token
        ));
    }

    return results;
}

TxError bsv::CTxValidator::CheckStandardness(std::span<const uint8_t> extendedTX,
                                              std::span<const int32_t> utxoHeights,
                                              int32_t blockHeight) const
{
    return CheckStandardness(extendedTX, utxoHeights, blockHeight, source->GetToken());
}

TxError bsv::CTxValidator::CheckStandardness(std::span<const uint8_t> extendedTX,
                                              std::span<const int32_t> utxoHeights,
                                              int32_t blockHeight,
                                              const task::CCancellationToken& token) const
{
    if (token.IsCanceled())
        return bsv::TxErrorCancelled();

    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;

        if (!tx_stream.empty())
            throw std::runtime_error("error serializing extended tx");

        const CTransaction ctx(eTX.mtx);
        if (token.IsCanceled())
            return bsv::TxErrorCancelled();
        return implCheckStandardness(ctx, eTX.vutxo, utxoHeights, blockHeight, token);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::implVerifyScript(
    const CTransaction& ctx,
    const std::vector<CTxOut>& prevUTXO,
    std::span<const int32_t> utxoHeights,
    int32_t blockHeight,
    bool consensus,
    std::span<const uint32_t> customFlags,
    const task::CCancellationToken& token
) const
{
    if (prevUTXO.size() != utxoHeights.size())
        throw std::runtime_error("inconsistent utxo heights and number of utxo");
    if (prevUTXO.size() != ctx.vin.size())
        throw std::runtime_error("inconsistent inputs size");
    if (prevUTXO.empty() || ctx.vin.empty() || utxoHeights.empty())
        throw std::runtime_error("tx has no inputs to verify");
    if (!customFlags.empty() && customFlags.size() != prevUTXO.size())
        throw std::runtime_error("inconsistent custom flags size");

    if (const auto hook = validationStageHook.load(std::memory_order_relaxed))
        hook(bsv::detail::ValidationStage::PrecomputedData);
    // Hashed once here and shared by every input's FORKID signature checks.
    const PrecomputedTransactionData txdata(ctx);
    // Building it is linear in the transaction size with no poll inside, so poll
    // after it, not before: a cancel that lands while it runs is then seen before
    // the first script. No test pins this position; polling just before the build
    // would give the same results and polls, only later on a large transaction.
    if (token.IsCanceled())
        return bsv::TxErrorCancelled();

    const bool useCustomFlags = !customFlags.empty();
    // Reads only shared state and its own checker, so inputs can run on any thread.
    const auto verifyInput = [&](size_t index, const task::CCancellationToken& inputToken) -> TxError {
        const uint64_t amount = prevUTXO[index].nValue.GetSatoshis();
        const CScript& lscript = prevUTXO[index].scriptPubKey;
        const CScript& uscript = ctx.vin[index].scriptSig;

        const int32_t utxoHeight{ utxoHeights[index] };
        if (consensus && utxoHeight == MEMPOOL_HEIGHT)
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::UnconfirmedInputInBlock));
        const uint32_t flags = !useCustomFlags ? CalculateFlags(utxoHeight, blockHeight, consensus) : customFlags[index];

        if (!ctx.vin.empty() && !ctx.vout.empty()) {
            const Amount amt{ amount };
            CachingScriptChecker sig_checker(&ctx, index, amt, txdata);
            return bsvVerifyScript(uscript, lscript, consensus, flags, sig_checker, inputToken);
        }
        BaseSignatureChecker sig_checker;
        return bsvVerifyScript(uscript, lscript, consensus, flags, sig_checker, inputToken);
    };

    const size_t nInputs = prevUTXO.size();
#if BDK_PARALLEL_INPUTS
    const size_t threshold = parallelInputThreshold.load(std::memory_order_relaxed);
    // A limit of 0 never creates the pool, so no helper thread is started.
    const size_t helperLimit = nInputs >= threshold ? HelperLimit() : 0;
    if (helperLimit > 0) {
        if (InputVerifyPool* pool = InputVerifyPool::Get()) {
            // A tx at the threshold is offered at least one helper when a core is free.
            const size_t perParticipant = std::clamp<size_t>(threshold / 2, 1, MIN_INPUTS_PER_PARTICIPANT);
            const size_t participants = (nInputs + perParticipant - 1) / perParticipant;
            // With every core already busy the job is still queued, waking no
            // one: a helper joins it when a core frees.
            if (const auto hook = inputPoolHook.load(std::memory_order_relaxed))
                hook(bsv::detail::InputPoolEvent::BeforeOffer);
            std::shared_ptr<InputJob> job;
            try {
                job = std::make_shared<InputJob>(nInputs, verifyInput, token);
                pool->Offer(job, participants - 1);
            } catch (const std::exception&) {
                job.reset();  // not queued (bad_alloc, system_error): verify serially below
            }
            if (job) {
                std::optional<size_t> verified;
                {
                    const JobCaller caller(*pool, job);
                    verified = job->Work();
                    if (!verified)
                        pool->Withdraw(job);  // no new selections while joined helpers drain
                    if (inputCountersEnabled.load(std::memory_order_relaxed))
                        jobVerifiedInputs.fetch_add(verified.value_or(0), std::memory_order_relaxed);
                }
                // All joined work has drained. A caller setup failure falls
                // through to serial verification; otherwise completion decides.
                if (verified)
                    return job->Result();
            }
        }
    }
#endif
    for (size_t index = 0; index < nInputs; ++index) {
        const TxError verifyResult = verifyInput(index, token);
        if (const auto hook = inputHook.load(std::memory_order_relaxed))
            hook(index, bsv::detail::InputEvent::Verified);
        if (!bsv::TxErrorIsOk(verifyResult))
            return verifyResult;
    }
    return bsv::TxErrorOk();
}

TxError bsv::CTxValidator::bsvVerifyScript(
    const CScript& unlocking_script,
    const CScript& locking_script,
    const bool consensus,
    const unsigned int flags,
    BaseSignatureChecker& sig_checker,
    const task::CCancellationToken& token
) const
{
    const verify_script_params veriParams{make_verify_script_params(policySettings, flags, consensus)};
    const auto ret = ::VerifyScript(veriParams,
        token,
        unlocking_script,
        locking_script,
        flags,
        sig_checker);

    if (!ret.has_value())
        return bsv::TxErrorCancelled();  // cancellation token fired

    if (ret.value() == SCRIPT_ERR_OK)
        return bsv::TxErrorOk();

    return bsv::TxErrorScript(ret.value());
}

// Helper function to perform standardness checks
// Returns SCRIPT_ERR_OK if all checks pass, otherwise returns SCRIPT_ERR_UNKNOWN_ERROR
// Replicates the standardness checks performed by bitcoin-sv's TxnValidation
// (validation.cpp) when a transaction is received from a peer (consensus=false).
// This is never called during block validation (consensus=true).
//
// The check has two stages, both governed by acceptNonStandardOutput, which is
// era-dependent:
//   pre-Genesis  : acceptNonStandardOutput = !requireStandard
//   post-Genesis : acceptNonStandardOutput = policySettings.acceptNonStandardOutput (default true)
//
// Stage 1 — output standardness (IsStandardTx):
//   FAILS when IsStandardTx returns false AND:
//     - acceptNonStandardOutput is false, OR
//     - era is Genesis AND requireStandard is true AND reason != "scriptpubkey"
//   PASSES otherwise. In particular, post-Genesis with acceptNonStandardOutput=true
//   (mainnet default), a non-standard output script of type "scriptpubkey" (any
//   unrecognised script) is explicitly allowed.
//
// Stage 2 — input standardness (AreInputsStandard equivalent):
//   Only executed when acceptNonStandardOutput is false.
//   FAILS when any input's scriptSig is not standard for the UTXO's era.
//   PASSES (skipped entirely) when acceptNonStandardOutput is true.
//
// Summary of common mainnet scenarios (post-Genesis, requireStandard=true,
// acceptNonStandardOutput=true by default):
//   - Standard tx                                       → PASSES (stage 1 ok, stage 2 skipped)
//   - Non-standard output, reason="scriptpubkey"        → PASSES (post-Genesis exception)
//   - Non-standard output, reason!="scriptpubkey"       → FAILS  (dust, bare-multisig, etc.)
//   - acceptNonStandardOutput=false, non-standard input → FAILS  (stage 2 triggered)
TxError bsv::CTxValidator::implCheckStandardness(
    const CTransaction& tx,
    const std::vector<CTxOut>& prevUTXO,
    std::span<const int32_t> utxoHeights,
    int32_t blockHeight,
    const task::CCancellationToken& token
) const
{
    // One previous output and one height per input; the input loop below indexes both.
    if (prevUTXO.size() != tx.vin.size() || utxoHeights.size() != tx.vin.size())
        throw std::runtime_error("implCheckStandardness: inconsistent inputs, utxos and utxo heights");

    // Replicate TxnValidation standardness logic from bitcoin-sv validation.cpp.
    // acceptNonStandardOutput is era-dependent:
    //   pre-Genesis  → !requireStandard
    //   post-Genesis → the explicit acceptNonStandardOutput policy setting (default true)
    const ProtocolEra era = GetProtocolEra(policySettings, blockHeight);
    const bool acceptNonStandardOutput = policySettings.GetAcceptNonStandardOutput(era);

    std::string reason;
    if (!IsStandardTx(policySettings, tx, blockHeight, reason)) {
        // Mirror the bitcoin-sv rejection condition:
        //   always reject if acceptNonStandardOutput is false, OR
        //   in Genesis era, reject only when requireStandard is true AND
        //   the failure reason is not "scriptpubkey" (unrecognised output type),
        //   because post-Genesis nodes deliberately allow arbitrary output scripts.
        if (!acceptNonStandardOutput ||
            (IsProtocolActive(era, ProtocolName::Genesis) && policySettings.GetRequireStandard() && reason != "scriptpubkey")) {
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::NotStandard));
        }
    }

    // AreInputsStandard equivalent — only enforced when non-standard outputs are not accepted,
    // mirroring the bitcoin-sv TxnValidation path (validation.cpp:1208).
    if (!acceptNonStandardOutput && !tx.IsCoinBase()) {
        constexpr bool consensus = false;
        constexpr uint32_t flags = SCRIPT_VERIFY_NONE;
        const auto params = make_eval_script_params(policySettings, flags, consensus);

        for (size_t i = 0; i < tx.vin.size(); ++i) {
            if (token.IsCanceled())
                return bsv::TxErrorCancelled();
            const CScript& scriptSig = tx.vin[i].scriptSig;
            const CScript& prevScript = prevUTXO[i].scriptPubKey;
            const ProtocolEra utxoEra = GetProtocolEra(policySettings, resolveUtxoEraHeight(utxoHeights[i], blockHeight));

            auto result = IsInputStandard(token, params, scriptSig, prevScript, utxoEra, flags);
            if (!result.has_value())
                return bsv::TxErrorCancelled();  // cancellation token fired, no verdict
            if (!result.value())
                return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::NotStandard));
        }
    }

    return bsv::TxErrorOk();
}

// Replicates Consensus::CheckTxInputs (validation.cpp:2590-2655):
//   - per-input MoneyRange check (bad-txns-inputvalues-outofrange, DoS 100)
//   - accumulated nValueIn MoneyRange check
//   - nValueIn >= tx.GetValueOut() (bad-txns-in-belowout, DoS 100)
// Output values are already validated by implCheckTransactionCommon, so
// GetValueOut() is safe to call here.
TxError bsv::CTxValidator::implCheckInputValues(
    const CTransaction& tx,
    const std::vector<CTxOut>& prevUTXO
) const
{
    if (prevUTXO.size() != tx.vin.size())
        throw std::runtime_error("implCheckInputValues: inconsistent inputs size");

    Amount nValueIn(0);
    for (const auto& utxo : prevUTXO) {
        if (!MoneyRange(utxo.nValue) || !MoneyRange(nValueIn + utxo.nValue))
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::InputValuesOutOfRange));
        nValueIn += utxo.nValue;
    }

    if (nValueIn < tx.GetValueOut())
        return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::InputsBelowOutputs));

    return bsv::TxErrorOk();
}

int bsv::CPP_SCRIPT_ERR_ERROR_COUNT(){
    return SCRIPT_ERR_ERROR_COUNT;
}

TxError bsv::CTxValidator::ValidateTransaction(std::span<const uint8_t> extendedTX,
                                             std::span<const int32_t> utxoHeights,
                                             int32_t blockHeight,
                                             bool consensus) const
{
    return ValidateTransaction(extendedTX, utxoHeights, blockHeight, consensus, source->GetToken());
}

TxError bsv::CTxValidator::ValidateTransaction(std::span<const uint8_t> extendedTX,
                                             std::span<const int32_t> utxoHeights,
                                             int32_t blockHeight,
                                             bool consensus,
                                             const task::CCancellationToken& token) const
{
    if (token.IsCanceled())
        return bsv::TxErrorCancelled();

#if BDK_PARALLEL_INPUTS
    const BusyScope busy;  // count deserialization, hashing and transaction checks
#endif

    try {
        if (const auto hook = validationStageHook.load(std::memory_order_relaxed))
            hook(bsv::detail::ValidationStage::Deserializing);
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;

        if (!tx_stream.empty())
            throw std::runtime_error("error serializing extended tx");

        const CTransaction ctx(eTX.mtx);

        // Polled here, below between groups of checks, and in implVerifyScript once
        // the shared signature-hash data is built. Within a group nothing polls:
        // deserialising and hashing above, then the transaction-level, prevout and
        // output checks, then the standardness (polled per input when inputs are
        // checked), sigop, input-value and fee checks and the shared signature-hash
        // data. Each is linear in the transaction size.
        if (token.IsCanceled())
            return bsv::TxErrorCancelled();

        if (const auto hook = validationStageHook.load(std::memory_order_relaxed))
            hook(bsv::detail::ValidationStage::TransactionChecks);
        if (ctx.IsCoinBase())
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::CoinbaseNotAllowed));

        // Guard against malformed extended-tx metadata before any method indexes prevUTXO or utxoHeights.
        if (eTX.vutxo.size() != ctx.vin.size())
            throw std::runtime_error("inconsistent utxo and input count in extended transaction");
        if (utxoHeights.size() != ctx.vin.size())
            throw std::runtime_error("inconsistent utxo heights and input count");

        // Policy era is computed at blockHeight+1 (next block), consensus at blockHeight.
        // implCheckSigOpsPolicy and CalculateFlags already apply +1 internally; all other
        // era-sensitive helpers receive the pre-adjusted height here.
        const int32_t checkHeight = consensus ? blockHeight : blockHeight + 1;

        if (auto r = implCheckTransactionCommon(ctx, checkHeight);                              !bsv::TxErrorIsOk(r)) return r;
        if (auto r = implCheckPrevOutputs(ctx);                                                 !bsv::TxErrorIsOk(r)) return r;
        if (auto r = implCheckOutputs(ctx, checkHeight);                                        !bsv::TxErrorIsOk(r)) return r;
        if (token.IsCanceled())
            return bsv::TxErrorCancelled();
        if (!consensus) {
            if (auto r = implCheckStandardness(ctx, eTX.vutxo, utxoHeights, checkHeight, token); !bsv::TxErrorIsOk(r)) return r;
            if (auto r = implCheckSigOpsPolicy(ctx, eTX.vutxo, utxoHeights, blockHeight);      !bsv::TxErrorIsOk(r)) return r;
        } else {
            if (auto r = implCheckConsensusSigops(ctx, eTX.vutxo, utxoHeights, blockHeight);   !bsv::TxErrorIsOk(r)) return r;
        }
        if (auto r = implCheckInputValues(ctx, eTX.vutxo);                                     !bsv::TxErrorIsOk(r)) return r;
        // Fee check runs only in policy mode and only after implCheckInputValues, so MoneyRange
        // holds and the inputs - outputs subtraction inside implCheckFee is well-defined.
        if (!consensus) {
            if (auto r = implCheckFee(ctx, eTX.vutxo, utxoHeights, blockHeight);               !bsv::TxErrorIsOk(r)) return r;
        }
        // implVerifyScript polls once it has built the shared signature-hash data.
        return implVerifyScript(ctx, eTX.vutxo, utxoHeights, blockHeight, consensus, {}, token);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::CheckPrevOutputs(std::span<const uint8_t> extendedTX) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;
        const CTransaction ctx(eTX.mtx);
        return implCheckPrevOutputs(ctx);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::CheckOutputs(std::span<const uint8_t> extendedTX, int32_t blockHeight) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;
        const CTransaction ctx(eTX.mtx);
        return implCheckOutputs(ctx, blockHeight);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::CheckConsensusSigops(std::span<const uint8_t> extendedTX,
                                                  std::span<const int32_t> utxoHeights,
                                                  int32_t blockHeight) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;
        const CTransaction ctx(eTX.mtx);
        return implCheckConsensusSigops(ctx, eTX.vutxo, utxoHeights, blockHeight);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

TxError bsv::CTxValidator::CheckSigOpsPolicy(std::span<const uint8_t> extendedTX,
                                              std::span<const int32_t> utxoHeights,
                                              int32_t blockHeight) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;
        const CTransaction ctx(eTX.mtx);
        return implCheckSigOpsPolicy(ctx, eTX.vutxo, utxoHeights, blockHeight);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

// Replicates bitcoin-sv CheckTransactionCommon (validation.cpp:520).
// Checks empty vin/vout, consensus serialization size, per-output money range,
// total output overflow, and pre-Genesis non-P2SH sigops consensus limit.
// blockHeight is the era-determining height: blockHeight for consensus,
// blockHeight+1 for policy (callers pass the already-adjusted value).
TxError bsv::CTxValidator::implCheckTransactionCommon(
    const CTransaction& tx,
    int32_t blockHeight) const
{
    if (tx.vin.empty())
        return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::VinEmpty));

    if (tx.vout.empty())
        return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::VoutEmpty));

    const ProtocolEra era = GetProtocolEra(policySettings, blockHeight);
    const uint64_t maxTxSize = policySettings.GetMaxTxSize(era, true);
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > maxTxSize)
        return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::Oversize));

    Amount nValueOut(0);
    for (const auto& txout : tx.vout) {
        if (txout.nValue < Amount(0))
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::OutputNegative));
        if (txout.nValue > MAX_MONEY)
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::OutputTooLarge));
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::OutputTotalTooLarge));
    }

    if (!IsProtocolActive(era, ProtocolName::Genesis)) {
        bool sigOpCountError = false;
        uint64_t nSigOps = 0;
        for (const auto& txin : tx.vin)
            nSigOps += txin.scriptSig.GetSigOpCount(false, era, sigOpCountError);
        for (const auto& txout : tx.vout)
            nSigOps += txout.scriptPubKey.GetSigOpCount(false, era, sigOpCountError);
        if (sigOpCountError || nSigOps > MAX_TX_SIGOPS_COUNT_BEFORE_GENESIS)
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::SigopsConsensus));
    }

    return bsv::TxErrorOk();
}

// Replicates CheckRegularTransaction (validation.cpp): null-prevout check and
// duplicate-prevout detection (bad-txns-inputs-duplicate, DoS 100).
TxError bsv::CTxValidator::implCheckPrevOutputs(const CTransaction& tx) const
{
    std::set<COutPoint> seen;
    for (const auto& txin : tx.vin) {
        if (txin.prevout.IsNull())
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::NullPrevout));
        if (!seen.insert(txin.prevout).second)
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::DuplicateInputs));
    }
    return bsv::TxErrorOk();
}

// Replicates the hasP2SHOutput check in CheckRegularTransaction (validation.cpp).
TxError bsv::CTxValidator::implCheckOutputs(const CTransaction& tx, int32_t blockHeight) const
{
    const ProtocolEra era = GetProtocolEra(policySettings, blockHeight);
    if (!IsProtocolActive(era, ProtocolName::Genesis))
        return bsv::TxErrorOk();

    for (const auto& txout : tx.vout) {
        if (IsP2SH(txout.scriptPubKey))
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::P2SHOutputPostGenesis));
    }
    return bsv::TxErrorOk();
}

// Replicates GetTransactionSigOpCount (inputs + outputs + P2SH redeem scripts) + 20,000 limit
// as done in BlockValidateTxns (validation.cpp). Uses blockHeight era (no +1) for block context.
//
// NOTE — block-level aggregate sigops (M13 sub-problem 1) is NOT implemented here.
// Bitcoin-sv accumulates sigops across all transactions in the block and checks against
// config.GetMaxBlockSigOpsConsensusBeforeGenesis(currentBlockSize), which is proportional
// to the current block size. BDK validates one transaction at a time and has no access to
// the block size or a cross-transaction accumulator, so this check must be performed by the
// node caller: sum GetSigOpCount() results per transaction and compare against the block limit.
TxError bsv::CTxValidator::implCheckConsensusSigops(const CTransaction& tx,
                                                     const std::vector<CTxOut>& prevUTXO,
                                                     std::span<const int32_t> utxoHeights,
                                                     int32_t blockHeight) const
{
    const ProtocolEra era = GetProtocolEra(policySettings, blockHeight);
    if (IsProtocolActive(era, ProtocolName::Genesis))
        return bsv::TxErrorOk();

    for (auto h : utxoHeights) {
        if (h == MEMPOOL_HEIGHT)
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::UnconfirmedInputInBlock));
    }

    // Gate P2SH sigop counting on the block's active SCRIPT_VERIFY_P2SH flag, mirroring
    // bitcoin-sv's BlockValidateTxns which passes (flags & SCRIPT_VERIFY_P2SH) to
    // GetTransactionSigOpCount. BDK previously gated on UTXO era, which is wrong at
    // historical pre-P2SH heights where P2SH was not yet enforced by the block flags.
    const uint32_t blockFlags = GetBlockScriptFlags(consensusParams, blockHeight - 1, era);
    const bool countP2SHSigOps = (blockFlags & SCRIPT_VERIFY_P2SH) != 0;

    const uint64_t nSigOps = implGetSigOpCount(tx, prevUTXO, utxoHeights, era, blockHeight, countP2SHSigOps);
    if (nSigOps > MAX_TX_SIGOPS_COUNT_BEFORE_GENESIS)
        return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::SigopsConsensus));

    return bsv::TxErrorOk();
}

// Replicates GetTransactionSigOpCount + policy limit check in AcceptToMemoryPool (validation.cpp).
// Pre-Genesis: counts inputs + outputs + P2SH redeem scripts, checks against maxSigOpsPolicy.
// Post-Genesis: P2SH sigops skipped (implGetSigOpCount handles this); checks against
//   maxSigOpsPostGenesisPolicy (mirrors GetMaxTxSigOpsCountPolicy, default UINT32_MAX).
TxError bsv::CTxValidator::implCheckSigOpsPolicy(const CTransaction& tx,
                                                   const std::vector<CTxOut>& prevUTXO,
                                                   std::span<const int32_t> utxoHeights,
                                                   int32_t blockHeight) const
{
    const ProtocolEra era = GetProtocolEra(policySettings, blockHeight + 1);
    const uint64_t limit = IsProtocolActive(era, ProtocolName::Genesis)
        ? maxSigOpsPostGenesisPolicy
        : maxSigOpsPolicy;

    // implGetSigOpCount replicates GetTransactionSigOpCount (inputs + outputs + P2SH redeem scripts)
    const uint64_t nSigOps = implGetSigOpCount(tx, prevUTXO, utxoHeights, era, blockHeight + 1, /*countP2SHSigOps=*/true);
    if (nSigOps > limit)
        return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::SigopsPolicy));

    return bsv::TxErrorOk();
}

TxError bsv::CTxValidator::IsFreeConsolidation(std::span<const uint8_t> extendedTX,
                                                std::span<const int32_t> utxoHeights,
                                                int32_t blockHeight) const
{
    try {
        const char* begin{ reinterpret_cast<const char*>(extendedTX.data()) };
        const char* end{ reinterpret_cast<const char*>(extendedTX.data() + extendedTX.size()) };
        CDataStream tx_stream(begin, end, SER_NETWORK, PROTOCOL_VERSION);
        bsv::CMutableTransactionExtended eTX;
        tx_stream >> eTX;
        const CTransaction ctx(eTX.mtx);
        return implIsFreeConsolidation(ctx, eTX.vutxo, utxoHeights, blockHeight);
    }
    catch (const std::exception&) {
        return bsv::TxErrorException();
    }
}

// Replicates IsFreeConsolidationTxn from bitcoin-sv policy/policy.cpp.
// Returns OK if the transaction qualifies as a fee-exempt consolidation (or dust donation).
// Returns NotFreeConsolidation otherwise.
TxError bsv::CTxValidator::implIsFreeConsolidation(const CTransaction& tx,
                                                     const std::vector<CTxOut>& prevUTXO,
                                                     std::span<const int32_t> utxoHeights,
                                                     int32_t blockHeight) const
{
    constexpr auto notFree = []() {
        return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::NotFreeConsolidation));
    };

    if (consolidationMinFactor == 0)
        return notFree();

    if (tx.IsCoinBase())
        return notFree();

    // Inline equivalent of IsDustReturnTxn (policy/policy.cpp is not linked in bdk_core)
    const bool isDonation = tx.vout.size() == 1
        && tx.vout[0].nValue.GetSatoshis() == 0
        && IsDustReturnScript(tx.vout[0].scriptPubKey);
    const uint64_t factor = isDonation ? tx.vin.size() : consolidationMinFactor;
    const int32_t minConf = isDonation ? int32_t(0) : static_cast<int32_t>(consolidationMinConf);

    if (tx.vin.size() < factor * tx.vout.size())
        return notFree();

    if (prevUTXO.size() != tx.vin.size() || utxoHeights.size() != tx.vin.size())
        return notFree();

    uint64_t sumInputScriptPubKeySize = 0;
    for (size_t i = 0; i < tx.vin.size(); ++i) {
        const int32_t coinHeight = utxoHeights[i];

        if (minConf > 0 && coinHeight == MEMPOOL_HEIGHT)
            return notFree();

        const int32_t seenConf = blockHeight + 1 - coinHeight;
        if (minConf > 0 && coinHeight && seenConf < minConf)
            return notFree();

        if (tx.vin[i].scriptSig.size() > consolidationMaxInputScriptSize)
            return notFree();

        txnouttype dummyType;
        if (!consolidationAcceptNonStd && !IsStandardOutput(policySettings, prevUTXO[i].scriptPubKey, coinHeight, dummyType))
            return notFree();

        sumInputScriptPubKeySize += prevUTXO[i].scriptPubKey.size();
    }

    uint64_t sumOutputScriptPubKeySize = 0;
    for (const CTxOut& o : tx.vout)
        sumOutputScriptPubKeySize += o.scriptPubKey.size();

    if (sumInputScriptPubKeySize < factor * sumOutputScriptPubKeySize)
        return notFree();

    return bsv::TxErrorOk();
}

/*
 * implCheckFee — static-fee-path floor with free-consolidation bypass (policy mode only).
 *
 * Deltas from bitcoin-sv TxnValidation fee path (validation.cpp:1255-1296). Read
 * before changing — any new delta must be added to this list.
 *
 *   (a) Single static rate. bitcoin-sv tracks two distinct fee quantities:
 *         blockMinTxFee    = pool.GetBlockMinTxFee()      // static, from -minminingtxfee
 *         mempoolRejectFee = GetMempoolRejectFee(...)     // eviction-driven, can rise above
 *                                                         // blockMinTxFee under mempool pressure
 *       BDK collapses both into one static minMiningTxFeeSatPerKB. Under mempool
 *       pressure bitcoin-sv may reject txs BDK accepts — out of scope (no mempool
 *       state exists at this layer to read from).
 *
 *   (b) No PrioritiseTransaction credit synthesis. bitcoin-sv implements the
 *       free-consolidation bypass by injecting a per-tx fee delta via
 *       pool.PrioritiseTransaction(...) when isFree is true. BDK has no per-tx
 *       delta state; the bypass is expressed directly as a control-flow gate
 *       (`if (under floor && !isFree) reject`). Net accept/reject is the same.
 *
 *   (c) Policy-only. Caller (ValidateTransaction) gates on `!consensus` before
 *       invoking this helper. Mirrors bitcoin-sv: TxnValidation runs the fee
 *       check; block validation does not.
 *
 *   (d) Wire tx size, not extended-tx size. Uses CTransaction::GetTotalSize() —
 *       the bare wire transaction, matching bitcoin-sv's ptx->GetTotalSize() at
 *       validation.cpp:1262. The extended-tx byte length would inflate minFee
 *       by the prepended UTXO metadata and falsely reject txs bitcoin-sv would
 *       accept.
 *
 *   (e) Special-zero rate. minMiningTxFeeSatPerKB == 0 short-circuits to accept
 *       (every tx passes the floor). Matches teranode's existing semantics on
 *       Policy.MinMiningTxFee == 0 and avoids triggering CFeeRate's round-up-
 *       to-1-sat rule on a disabled policy.
 *
 *   (f) Integer math, matches CFeeRate::GetFee bitwise (amount.cpp:28-44),
 *       including the round-up-to-1 rule for small non-zero txs.
 *
 * Precondition: implCheckInputValues has run successfully, so MoneyRange holds
 * on inputs/outputs and inputSats >= outputSats — the subtraction below is safe.
 */
TxError bsv::CTxValidator::implCheckFee(const CTransaction& tx,
                                         const std::vector<CTxOut>& prevUTXO,
                                         std::span<const int32_t> utxoHeights,
                                         int32_t blockHeight) const
{
    if (minMiningTxFeeSatPerKB == 0) {
        return bsv::TxErrorOk();  // delta (e)
    }

    Amount nValueIn(0);
    for (const auto& utxo : prevUTXO)
        nValueIn += utxo.nValue;
    const Amount nFees = nValueIn - tx.GetValueOut();

    const int64_t txSize = static_cast<int64_t>(tx.GetTotalSize());  // delta (d)
    int64_t minFeeSats = (txSize * minMiningTxFeeSatPerKB) / 1000;   // delta (f)
    if (minFeeSats == 0 && txSize > 0 && minMiningTxFeeSatPerKB > 0)
        minFeeSats = 1;

    if (nFees < Amount(minFeeSats)) {
        // Free-consolidation gate is the only escape (delta (b)). NotFreeConsolidation
        // is not itself a rejection — only its presence here escalates the shortfall.
        const TxError isFree = implIsFreeConsolidation(tx, prevUTXO, utxoHeights, blockHeight);
        if (!bsv::TxErrorIsOk(isFree))
            return bsv::TxErrorDoS(static_cast<int32_t>(bsv::DoSError_t::InsufficientFee));
    }
    return bsv::TxErrorOk();
}
