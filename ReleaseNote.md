# Release Notes

**Current version: BDK 1.4.0** (`BDK_VERSION_MAJOR/MINOR/PATCH` in `CMakeLists.txt`).

BDK is built against a pinned `bitcoin-sv` commit; the exact source commit and toolchain versions
are recorded in [documentation/docs/build.md](documentation/docs/build.md) and captured into the
generated version values (declared in `core/BDKVersion.h` and exposed by
`module/gobdk/version.go`) at build time.

For the detailed list of changes, see the project's commit history / changelog. This file is
installed alongside the package (`CMakeLists.txt`).

## Unreleased

- **GCC builds align the per-byte script loop to 32 bytes** (`-falign-loops=32
  --param=align-loop-iterations=1`, plus `--param=align-threshold=4000` on bitcoin-sv's
  `src/script/interpreter.cpp`; option `BDK_ALIGN_LOOPS`, default ON; Clang, MSVC and WASM
  builds are unchanged). bitcoin-sv's `prevector::erase` loop is inlined into the
  `CTxIn`/`CTxOut` vector destructors and into `EvalScript`, which frees the script code
  after each signature check. Unaligned, it crosses a 64-byte line at some link layouts,
  measured on Zen 3 and Sapphire Rapids at 11-16% slower on large-script transactions
  (destructors) and 7-10% slower on signature checks over large script code (`EvalScript`).
  Now aligned: both destructors, and `EvalScript`'s copies on the `OP_CHECKSIG` path and
  both `OP_CHECKMULTISIG` copies. Not aligned: the copy on `EvalScript`'s early-return path,
  which GCC treats as cold and which runs at most once per failing script. On CI-built
  archives no loop on these paths crosses a line, Zen 3 is flat across placements, and
  Graviton4 was unchanged by the destructor alignment (the `EvalScript` threshold was
  benchmarked on x86-64 only). Two effects remain, on Sapphire Rapids only and neither a
  line crossing: `EvalScript` on large script code differs by about 8% between
  interpreter.cpp's two 32-byte placements, and 1,000 × 1 KB outputs is 2-4% slower than
  1.4.0. CI checks the loop's placement in the gobdk and libbdkffi archives. The complete
  fix belongs in bitcoin-sv's `prevector::erase`.

## 1.4.0

- **Hardware SHA-256** on x86-64 and ARM64, selected at load time with known-answer
  self-tests and a scalar fallback; Go and Rust expose the selected backend
  ([#66](https://github.com/bitcoin-sv/bdk/pull/66)).
- **Cancellable validation** in C++, Go and Rust, including Go `*Context` functions and
  Rust `CancelFlag`. Cancellation returns a distinct error rather than a validation verdict.
  `CheckStandardness` also rejects mismatched UTXO height counts and trailing bytes.
  The Rust crates are `0.2.0` because `TxError` is now `#[non_exhaustive]`
  ([#68](https://github.com/bitcoin-sv/bdk/pull/68)).
- **Go dependency and toolchain updates:** `golang.org/x/crypto` is `v0.58.0`;
  gobdk now requires **Go 1.26.0 or later**, matching the CI toolchain
  ([#72](https://github.com/bitcoin-sv/bdk/pull/72)).

## Recent changes

- **Validation calls can be cancelled.** `CTxValidator` has overloads of `ValidateTransaction`,
  `VerifyScript`, `VerifySpend`, `ValidateBatch` and `CheckStandardness` that take a cancellation
  token; Go has `ValidateTransactionContext`, `VerifyScriptContext`,
  `VerifyScriptWithCustomFlagsContext` and `ValidateBatchContext`; Rust has `*_cancellable`
  methods with a `CancelFlag`. A cancelled call returns the new `TX_ERR_DOMAIN_CANCELLED` (5),
  Go `*CancelledError` / `ErrCancelled`, Rust `TxError::Cancelled`: no validation verdict.
  Existing `CheckStandardness` calls now return `TX_ERR_DOMAIN_EXCEPTION` for mismatched
  UTXO height counts or trailing bytes, as detailed below. Rust's `TxError` is now `#[non_exhaustive]`;
  as that breaks exhaustive matches, `rust-bdk` and `bdk-sys` move to 0.2.0.
  See [ObjectModel](documentation/docs/ObjectModel.md#cancellation).
- **Go cancellation causes stay separate from validation errors.**
  `CancelledError.ContextCause()` returns `context.Cause(ctx)`. It deliberately has neither
  `Cause()` nor `Unwrap()`: `github.com/pkg/errors.Cause(err)` returns the cancelled result
  itself, and `errors.As` does not expose a script error supplied as the context cause.
- **`CheckStandardness` checks it has one UTXO height per input.** Where policy reads heights,
  a shorter span used to be read past its end. Where it does not (the post-Genesis default),
  mismatched sizes used to pass. Both cases now give `TX_ERR_DOMAIN_EXCEPTION`, as the other
  entry points do for mismatched sizes. Both overloads also reject trailing bytes with
  `TX_ERR_DOMAIN_EXCEPTION`, as `VerifyScript` and `ValidateTransaction` already do;
  `CheckStandardness` previously ignored those bytes.
- **Hardware SHA-256 on x86-64 and ARM64.** Every SHA-256 BDK computes during validation
  (sighash, txid, the hashing opcodes) now runs on the fastest transform the CPU supports,
  chosen at run time. The 2-way merkle-pair transform is installed too, but BDK validation never
  computes a merkle root. On x86-64 BDK builds bitcoin-sv's SSE4 and SHA-NI transforms and lets
  bitcoin-sv's own `sha256_dispatch::AutoDetect()` pick by CPUID. On 64-bit ARM it uses a new
  backend for the ARMv8 SHA-2 instructions, vendored unchanged from Bitcoin Core v29.0
  (`core/sha256_arm_shani.cpp`, MIT), for both the streaming transform and the 2-way merkle-pair
  transform. It is enabled when Linux reports `HWCAP_SHA2` or macOS reports
  `hw.optional.arm.FEAT_SHA256`; other ARM operating systems, MSVC/clang-cl builds and the WASM build
  keep the scalar transforms. No file gets instruction-set flags and nothing is built with
  `-march=native`: the SHA-NI and ARMv8 SHA-2 code is compiled through wrappers
  (`core/sha256_x86_shani.cpp`, `core/sha256_arm_shani_target.cpp`) that enable the instructions
  with target pragmas for their own architecture, chosen by the compiler's target macros, so
  macOS universal, Rosetta and cross builds get the right code in each slice. Each candidate
  passes a known-answer self-test before it is installed, and the scalar transforms remain the
  fallback. One M3 core goes from 471 to 2,611 MB/s of raw hashing. Validation speeds up only
  where hashing is the main cost: txids of large transactions, large scriptCodes in sighash
  preimages, and the hashing opcodes on large data. Ordinary P2PKH validation, which is mostly
  signature checks since the shared precompute below, does not change.
    - **Selection happens once, while BDK is loaded.** A load-time initializer in the object
      that holds `CSHA256` (`core/sha256_hasher.cpp`, which compiles bitcoin-sv's
      `crypto/sha256.cpp` for the native core) calls the new `bsv::SelectSha256Backend()`
      (`core/sha256_backend.hpp`). Anything that can hash links that object, so the dispatch
      pointers are written before `main()`, before Go runs any `init` function and before any
      thread can hash; afterwards they are only read. `SelectSha256Backend()` returns what was
      installed, e.g. `shani,shani(2way)` or `arm_shani,arm_shani(2way)`, and
      `bsv::Sha256SupportedBackends()` lists every backend the CPU can run. Go has
      `gobdk.SHA256_BACKEND()` and Rust has `rust_bdk::sha256_backend()` (C ABI
      `cgo_sha256_backend()` / `bdkffi_sha256_backend()`) for a startup log line.
    - **`BDK_SHA256_FORCE_SCALAR=1`** keeps the scalar transforms, for debugging and for
      comparing backends. It is read at load, so it must be in the environment the process
      starts with: setting it later, for example with Go's `os.Setenv`, has no effect.
    - **`bdk_core` now defines `sha256_dispatch::AutoDetect()`**, declared in the installed
      `crypto/sha256_dispatch.h`, so the link gap is closed. Consumers must not call it. On
      x86-64 it installs SSE4 / SHA-NI regardless of `BDK_SHA256_FORCE_SCALAR`: under forced
      scalar BDK never calls it, so a direct call is the first write to the dispatch pointers,
      while the backend string still says `standard(forced)`. Called before any selection, it
      writes the pointers outside BDK's once-only guard (only possible from another static
      initializer, since BDK selects at load). On ARM64 it installs nothing. Call
      `bsv::SelectSha256Backend()` instead.
    - `test_sha256_backends` checks every backend the runner supports against FIPS 180-4
      vectors and a seeded differential run, and prints the selected backend and its
      throughput; CI logs it for each runner. `test_txvalidator`, `test_bsv` and
      `test_sha256_backends` also run a second time with the scalar transforms forced.
      `test_sha256_select_race` checks, in a fresh process, that the backend is installed
      before anything runs, then hashes on 20 threads at once through every binding entry
      point; a CI job runs it under GCC ThreadSanitizer on x86-64 and ARM64 Linux, with a
      positive control that must be reported. Another CI job builds a macOS universal
      `bdk_core` and checks each slice carries its own backend and the load-time selection.
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
- **The Go binding version equals the BDK version again.** BDK `1.3.0` removed the `+2`
  patch offset, making its Go module `1.3.0` rather than `1.3.2`, sorting after
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
  transform; merkle hashing calls the 1-way transform twice, since the 2-way pointer stays null. (Superseded by the hardware SHA-256
  entry above: the dispatcher's `AutoDetect()` and its SSE4 and SHA-NI transforms are now built
  and used.) Behaviour
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
