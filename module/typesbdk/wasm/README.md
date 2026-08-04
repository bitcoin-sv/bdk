# `module/typesbdk/wasm` — build inputs and committed artifacts

This directory holds exactly two kinds of file: the **build inputs** of the
TypeScript/JavaScript WASM verifier, and the **eight committed artifacts** it ships.
Nothing else. In particular there are **no tests here** — they live in
[`test/types/`](../../../test/types/README.md), the same way `test/golang` and
`test/rust` hold the tests for the other two bindings.

## Build inputs

| Group | Files | Role |
|-------|-------|------|
| Binding source | `txvalidator_wasm.cpp`, `txvalidator_wasm.h` | the compact C ABI exported to JavaScript (`bdk_verify_script`, `bdk_verify_spend`, `bdk_sign_digest`, the key helpers, …) |
| Emscripten glue | `txvalidator_wasm.js` | `--post-js` loader that wraps the C ABI in the typed-array JavaScript API (`VerifyScriptArray`, `VerifyScriptBatchArray`, `SignDigest`, …) |
| | `txvalidator_wasm_compat.js` | `--post-js` compatibility layer: the `VectorUInt8`/`VectorInt32`/`VectorUInt32` shims and the vector-flavoured entry points. **Not** included in the slim build |
| | `txvalidator_wasm_snapshot.js` | `--post-js` verification-table export/import for worker instances. Not in the slim build |
| | `txvalidator_wasm_minimal_extern.js`, `txvalidator_wasm_minimal_pre.js` | `--extern-pre-js`/`--pre-js` for the `MINIMAL_RUNTIME` slim UMD build |
| Substituted core sources | `big_int_boost.cpp` | replaces bitcoin-sv's OpenSSL `big_int.cpp` with a header-only Boost.Multiprecision backend |
| | `memory_cleanse_wasm.cpp` | replaces `support/cleanse.cpp`, which needs OpenSSL |
| | `secp256k1_runtime.c`, `secp256k1_runtime_precomputed.c`, `secp256k1_runtime_precomputed.h` | reconstruct libsecp256k1's large fixed-base *verification* table at run time instead of shipping ~2.3 MB of static tables. The compact *signing* table is kept as upstream generates it |
| secp256k1 parity-suite inputs | `wasm_tests.c`, `wasm_tests_patch.cmake` | wrap and patch upstream's `tests.c` so libsecp256k1's own unit suite can run against the substituted runtime tables. Both are build inputs of `EXCLUDE_FROM_ALL` targets; the CTest entries that run them are registered in `test/types` |

`CMakeLists.txt` defines **targets only** — the module's own `bdk_core_wasm` variant
(from the shared recipe in `core/bdk-core-recipe.cmake`), the four binding targets and
their `wasm-opt` edges, the secp256k1 rewiring, the two parity-suite executables, and
the in-source publish target. It contains no `add_test`.

The `RISK — THIS SUBSTITUTION IS UNVERIFIED` block in `CMakeLists.txt` documents what
the runtime verification-table substitution still lacks a direct check for; read it
before changing anything under `secp256k1_runtime*`.

## The eight committed artifacts

```
bdk-core.mjs             bdk-core.wasm              # Node ESM
bdk-core.browser.mjs     bdk-core.browser.wasm      # browser/worker ESM
bdk-core.umd.js          bdk-core.umd.wasm          # classic script / UMD
bdk-core.slim.umd.js     bdk-core.slim.umd.wasm     # MINIMAL_RUNTIME UMD
```

They are committed **for consumer convenience**, so that a checkout (or a package
consuming this repository directly) works without an Emscripten toolchain. They are
refreshed **on demand by CI**, not on every change: a source change that lands without
an artifact refresh is expected and allowed, exactly like `module/gobdk/bdkcgo`'s
committed `libGoBDK_*.a` archives. There is no build-reproducibility gate on pull
requests, so do not read these bytes as "what the current sources produce".

To refresh them, dispatch `.github/workflows/build_bdk.yaml` with the
**`commit-wasm-artifacts`** box ticked; the bot commits the result with a
`[WasmBDKUpdate]` subject marker.

## The three commands

```console
# 1. Build (build tree only; never writes the eight files above)
module/typesbdk/wasm/build.sh

# 2. Validate (the tests are registered by test/types/CMakeLists.txt)
( cd build-wasm && ctest --output-on-failure )

# 3. Publish the eight artifacts into this directory (CI commit path only)
cmake --build build-wasm --target bdk_wasm_install_insource
```

One responsibility each: `build.sh` builds, `ctest` validates, the CMake target
publishes. `node` must be on `PATH` at **configure** time or ten of the twelve CTest
entries are not registered at all — see [`test/types/README.md`](../../../test/types/README.md).

See also [`module/typesbdk/examples/README.md`](../examples/README.md) for the ABI and
the benchmark controls, and `documentation/docs/build.md` for the environment the build
expects.
