# Release Notes

**Current version: BDK 1.3.0** (`BDK_VERSION_MAJOR/MINOR/PATCH` in `CMakeLists.txt`).

BDK is built against a pinned `bitcoin-sv` commit; the exact source commit and toolchain versions
are recorded in [documentation/docs/build.md](documentation/docs/build.md) and captured into the
generated version header (`core/BDKVersion.h` / `module/gobdk/version.go`) at build time.

For the detailed list of changes, see the project's commit history / changelog. This file is
installed alongside the package (`CMakeLists.txt`).

## Recent changes

- **The Go binding version equals the BDK version again.** The `+2` patch offset is gone, so
  this release's Go module is `1.3.0` rather than `1.3.2`, and it sorts after the last published
  `module/gobdk/v1.2.4`. The configure-time check that the overall version is the maximum of
  all component versions never fired: it compared dotted versions with the numeric `LESS`. It
  now uses `VERSION_LESS`, fails the configure, and covers the Rust C ABI version as well.
- **Built against bitcoin-sv 1.2.3** (`6504a3aff65ba97c0f6c80962b033e35ecbfed4b`, tag `v1.2.3`),
  up from 1.2.2. `BSV_CLIENT_VERSION_REVISION` is now `3`. The curated source list gains the
  new SHA-256 dispatcher (`src/crypto/sha256_dispatch.cpp` plus its scalar and SHA-NI
  transforms and shims). On x86_64 `sha256d64_shani.cpp` is compiled with
  `-msha -msse4.1 -mssse3` and `ENABLE_X86_SHANI`, as upstream does; elsewhere it compiles
  empty. BDK does not call `sha256_dispatch::AutoDetect()`, so hashing stays on the scalar
  path, as it did before. Behaviour changes from upstream that reach BDK: `CScriptNum`
  now throws `scriptnum_overflow_error` for non-big-int numbers that overflow `int64_t`,
  `OP_SPLIT` rejects split positions above `INT32_MAX`, and `CScriptBase` grows from 28
  to 32 inline bytes. `sizeof(CScript)` stays 40 on 64-bit and wasm32.
- **Builds with clang before 21.** bitcoin-sv 1.2.3's thread-safety annotations put parameter
  packs inside attributes, which clang 19 and 20 and AppleClang 17 reject. CMake now probes
  for this and, when the compiler fails, force-includes a generated header that expands the
  annotations to nothing, as upstream already does for non-clang compilers. The annotations
  only feed `-Wthread-safety`, so generated code is unchanged.
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
