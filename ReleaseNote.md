# Release Notes

**Current version: BDK 1.3.0** (`BDK_VERSION_MAJOR/MINOR/PATCH` in `CMakeLists.txt`).

BDK is built against a pinned `bitcoin-sv` commit; the exact source commit and toolchain versions
are recorded in [documentation/docs/build.md](documentation/docs/build.md) and captured into the
generated version header (`core/BDKVersion.h` / `module/gobdk/version.go`) at build time.

For the detailed list of changes, see the project's commit history / changelog. This file is
installed alongside the package (`CMakeLists.txt`).

## Recent changes

- **A transaction with 16 or more inputs is verified on several cores.** `ValidateTransaction`,
  `VerifyScript` and `ValidateBatch` hand such a transaction's inputs to the calling thread plus
  idle helper threads; smaller transactions, `VerifySpend` and the WASM build verify serially as
  before. The result is always the serial one: the error of the lowest-index failing input. At
  1,000 to 40,000 P2PKH inputs, `ValidateTransaction` is 2.3x faster on a 4-vCPU x86-64 runner,
  3.7x on a 4-core ARM64 runner and 7.4x on a 14-core Apple M3 Max; with callers already using
  every core, aggregate throughput is unchanged. What embedders should know:
  - The helpers belong to one process-wide pool, created on the first such transaction and never
    torn down (they idle on a condition variable). It has one helper per available CPU minus one.
  - Available CPUs are `std::thread::hardware_concurrency()`, lowered on Linux to the affinity
    mask of the thread that creates the pool and to the cgroup CPU limit (v2 `cpu.max`, v1
    `cpu.cfs_quota_us`/`cpu.cfs_period_us`), rounded up and never below 2 as Go's default
    GOMAXPROCS is. Both are read once, when the pool is created.
  - Helpers only use CPUs that callers leave free: a helper joins a job only while fewer threads
    than CPUs are verifying, and leaves before its next input once more are. Callers are never held
    back: when callers alone fill every CPU, a large transaction starts on its caller alone and a
    helper joins it as soon as a CPU frees.
  - `bdk_core` now links `Threads::Threads`; a consumer linking the static archive needs the
    platform's thread library (cgo and Cargo add it already).
  - Changing a `CTxValidator` setting (`Set...`) while any validation runs on it is a data race,
    as before; helpers now read those settings too.
  - Helpers do not see goroutines or other threads of the embedding process. While a large
    transaction is verified they compete with them for CPUs: on a 14-core machine with every Go P
    busy, a 10,000-input transaction verified 4.7x faster than serially while the goroutines'
    throughput fell by about 40% for that shorter time; with at most 2 helpers, 2.8x faster at
    about 20%. `SetParallelInputHelpers` (below) sets that trade-off.
- **New, additive API: a process-wide limit on parallel input helpers.** C++
  `bsv::SetParallelInputHelpers(int64_t)` / `bsv::GetParallelInputHelpers()`
  (`core/txvalidator.hpp`), Go `script.SetParallelInputHelpers(int64)` /
  `script.GetParallelInputHelpers()`, Rust `rust_bdk::set_parallel_input_helpers(i64)` /
  `rust_bdk::parallel_input_helpers()`. A negative value is automatic (the default, unchanged
  behaviour), `0` turns helpers off (no helper thread is started), `n > 0` caps the helpers
  verifying at once at `n`. Results are the same at every setting; it can be changed at any time.
  The WASM build has no helpers and does not expose it. The Go binding calls two new cgo symbols,
  so it needs GoBDK archives built from this version.
- **Validation calls can be cancelled.** `CTxValidator` has overloads of `ValidateTransaction`,
  `VerifyScript`, `VerifySpend`, `ValidateBatch` and `CheckStandardness` that take a cancellation
  token; Go has `ValidateTransactionContext`, `VerifyScriptContext`,
  `VerifyScriptWithCustomFlagsContext` and `ValidateBatchContext`; Rust has `*_cancellable`
  methods with a `CancelFlag`. A cancelled call returns the new `TX_ERR_DOMAIN_CANCELLED` (5),
  Go `*CancelledError` / `ErrCancelled`, Rust `TxError::Cancelled`: no verdict, never a script
  or DoS error. Existing functions are unchanged. Rust's `TxError` is now `#[non_exhaustive]`;
  as that breaks exhaustive matches, `rust-bdk` and `bdk-sys` move to 0.2.0.
  See [ObjectModel](documentation/docs/ObjectModel.md#cancellation).
- **`CheckStandardness` checks it has one UTXO height per input.** Where policy reads heights,
  a shorter span used to be read past its end. Where it does not (the post-Genesis default),
  mismatched sizes used to pass. Both cases now give `TX_ERR_DOMAIN_EXCEPTION`, as the other
  entry points do for mismatched sizes.
- **Script validation of many-input transactions is linear in the input count.**
  `ValidateTransaction` and `VerifyScript` now build one `PrecomputedTransactionData` per
  transaction and share it across all inputs, as SV Node does; `VerifySpend` builds one per call
  and shares it across the signature checks of its input, such as the keys of a multisig. Before,
  every FORKID signature check hashed all of the transaction's outpoints, sequences and outputs
  again, so validation time grew with the square of the input count. On an Apple M3 Max, a
  synthetic 8,000-input P2PKH transaction validated through the Go binding took 6.2 s and now
  takes 0.18 s, about 23 µs per input at every size measured. The 16,000-input, 16,000-output
  transaction of the scaling test takes 44.7 s of CPU time with this change reverted and 0.37 s
  with it. Verdicts are unchanged, and signatures on the original sighash algorithm (without
  FORKID, with the Chronicle flag, or any signature in consensus mode before the UAHF fork) never
  read the shared data. No API change in C++, Go, Rust or WASM.
- **The Go binding version equals the BDK version again.** The `+2` patch offset is gone, so
  this release's Go module is `1.3.0` rather than `1.3.2`, and it sorts after the last published
  `module/gobdk/v1.2.4`. The configure-time check that the overall version is the maximum of
  all component versions never fired: it compared dotted versions with the numeric `LESS`. It
  now uses `VERSION_LESS`, fails the configure, and covers the Rust C ABI version as well.
- **Build metadata stays current.** The BDK and bitcoin-sv version strings and git details used
  to be frozen at a build directory's first configure by a cached guard, so a reused directory
  reported stale versions after a bump or a bitcoin-sv checkout change. They are now recomputed
  on every configure, and the configure re-runs by itself when the BDK or bitcoin-sv checkout
  moves or is tagged, or bitcoin-sv's `clientversion.h` changes. Uncommitted edits do not
  trigger it, so the `_dirty` suffix on the commit hashes reflects the tree at the last
  configure: run cmake again after editing if the hash must say `_dirty`. The committed
  GoBDK archives and wasm artifacts, which local builds overwrite in source, do not count. A re-run configure
  keeps the bitcoin-sv checkout it resolved before, even from a shell without `BSV_ROOT`.
  `BDK_BUILD_DATETIME_UTC` is now in UTC (it was local time despite the name) and honours
  `SOURCE_DATE_EPOCH`. It is stamped once per source revision, the BDK and bitcoin-sv commits
  as of the configure, and again when `SOURCE_DATE_EPOCH` is set, changed or unset, so a
  cmake rerun with nothing changed rebuilds nothing.
- **Built against bitcoin-sv 1.2.3** (`6504a3aff65ba97c0f6c80962b033e35ecbfed4b`, tag `v1.2.3`),
  up from 1.2.2. `BSV_CLIENT_VERSION_REVISION` is now `3`. The curated source list gains the
  new SHA-256 dispatcher's header, its scalar stream transform and its scalar 1-way double-SHA-256
  transform; merkle hashing calls the 1-way transform twice, since the 2-way pointer stays null. The dispatcher's
  `AutoDetect()` (`src/crypto/sha256_dispatch.cpp`) and the SSE4 and SHA-NI transforms it would
  install are not built: BDK never calls it, so hashing stays on the scalar path, as it did
  before. The installed `crypto/sha256_dispatch.h` still declares `sha256_dispatch::AutoDetect()`,
  but `bdk_core` does not define it, so a C++ consumer that calls it fails to link. Behaviour
  changes from upstream that reach BDK: `CScriptNum` now throws `scriptnum_overflow_error` for non-big-int numbers that overflow `int64_t`,
  `OP_SPLIT` rejects split positions above `INT32_MAX`, and `CScriptBase` grows from 28
  to 32 inline bytes. `sizeof(CScript)` stays 40 on 64-bit and wasm32.
- **Builds with clang before 21.** bitcoin-sv 1.2.3's thread-safety annotations put parameter
  packs inside attributes, which clang 19 and 20 and AppleClang 17 reject. CMake now probes
  for this and, when the compiler fails, force-includes a generated header that expands the
  annotations to nothing, as upstream already does for non-clang compilers. The annotations
  only feed `-Wthread-safety`, so generated code is unchanged. This covers BDK's own build only:
  C++ projects that include the installed bitcoin-sv headers (anything pulling in `sync.h`, such
  as `config.h` or `script/interpreter.h`) need clang 21 or later, or GCC.
- **WASM bundles stay under the 300 KB ceiling.** bitcoin-sv 1.2.3 routes `CSHA256::Write`
  through a function pointer, and LTO then inlined it into every hashing call site, adding about
  6 KB. The wasm variant now compiles `crypto/sha256.cpp` outside LTO. `bdk-core` is 292,174
  bytes (295,284 at 1.2.2), and `test/types/benchmark.mjs` is unchanged within noise.
- **typesbdk build/test split, and the WASM artifacts are now refreshed on demand.** The node
  test, benchmark and vector files moved out of `module/typesbdk/wasm/` into the new
  **`test/types/`**, which now owns every wasm CTest registration (`test/golang` and `test/rust`
  already worked this way). `module/typesbdk/wasm/` holds build inputs and the eight committed
  artifacts only; its `CMakeLists.txt` defines targets only. User-visible breaks:
    - `module/typesbdk/wasm/build.sh` is **build-only**. `BDK_WASM_RUN_TESTS` and
      `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS` **no longer exist** — anything setting them silently
      has no effect. Validate with `( cd build-wasm && ctest --output-on-failure )`; publish the
      committed artifacts with
      `cmake --build build-wasm --target bdk_wasm_install_insource`. The preflight now covers
      build tools only: `node` and `ctest` are no longer required to run the script, but `node`
      must be on `PATH` at **configure** time or ten of the twelve tests are not registered.
    - The **artifact reproducibility gate is removed with no replacement** — not even an
      informational report. The eight committed artifacts are a refreshed-on-demand convenience
      like `module/gobdk/bdkcgo/libGoBDK_*.a`; they may lag the sources, and a pull request that
      changes wasm sources no longer has to carry regenerated binaries.
    - **`build_wasm.yaml` is no longer manually dispatchable.** It runs automatically on PR/push
      and is otherwise called by `build_bdk.yaml`. A wasm-only manual run is a `build_bdk.yaml`
      dispatch with both commit boxes unticked.
    - **`build_bdk.yaml`'s `commit-built-binaries` input is replaced** by two independent
      booleans, `commit-gobdk-archives` and `commit-wasm-artifacts`; saved dispatch forms and any
      external caller must be updated. A single commit job now handles both families, with
      all-or-nothing gating (any red leg blocks all commits), the `[GoBDKUpdate]` and
      `[WasmBDKUpdate]` loop-breaker markers, and one push.
- **Standalone WASM build architecture.** The TypeScript/JavaScript WASM verifier no longer
  builds by mutating the shared core: it assembles its own `bdk_core_wasm` variant from the
  reusable core recipe (`core/bdk-core-recipe.cmake`) inside `module/typesbdk/wasm/`, and the
  canonical native `bdk_core` is never modified by any module. New root flags: `BDK_BUILD_CORE`
  (default `ON`) and `BDK_BUILD_WASM` (default `OFF`; the name is kept but its semantics
  changed from the old in-tree overlay to the standalone build); the wasm build configures with
  `-DBDK_BUILD_CORE=OFF -DBDK_BUILD_WASM=ON` under Emscripten. The
  `BDK_BUILD_NATIVE_VERIFY_BENCHMARK` option is removed and nothing reads it anymore. The
  VerifyScript benchmark moved to `module/example/bench_verifyscript`, built by the regular
  native build. The default native build configures, builds and tests again on the official
  prebuilt dependency package. Boost multiprecision is now a required native dependency (the
  packages carry the headers): it backs the big-int parity suite (`test_big_int_boost`), which
  always builds and runs — a Boost install without multiprecision fails configure with an
  actionable error.
