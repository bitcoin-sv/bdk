# Release Notes

**Current version: BDK 1.2.2** (`BDK_VERSION_MAJOR/MINOR/PATCH` in `CMakeLists.txt`).

BDK is built against a pinned `bitcoin-sv` commit; the exact source commit and toolchain versions
are recorded in [documentation/docs/build.md](documentation/docs/build.md) and captured into the
generated version header (`core/BDKVersion.h` / `module/gobdk/version.go`) at build time.

For the detailed list of changes, see the project's commit history / changelog. This file is
installed alongside the package (`CMakeLists.txt`).

## Recent changes

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
