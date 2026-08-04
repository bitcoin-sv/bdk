# Release Notes

**Current version: BDK 1.2.2** (`BDK_VERSION_MAJOR/MINOR/PATCH` in `CMakeLists.txt`).

BDK is built against a pinned `bitcoin-sv` commit; the exact source commit and toolchain versions
are recorded in [documentation/docs/build.md](documentation/docs/build.md) and captured into the
generated version header (`core/BDKVersion.h` / `module/gobdk/version.go`) at build time.

For the detailed list of changes, see the project's commit history / changelog. This file is
installed alongside the package (`CMakeLists.txt`).

## Recent changes

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
