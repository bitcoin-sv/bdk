# `test/types` — tests for the typesbdk WASM module

This directory is the test consumer of what
[`module/typesbdk/wasm`](../../module/typesbdk/wasm/README.md) ships, mirroring
`test/golang` and `test/rust` for the other two bindings.

It **registers tests only and defines no build target**. Consequences worth knowing:
the wasm `ALL` target is byte-for-byte unaffected by anything here, and test code
cannot perturb the shipped artifacts. `CMakeLists.txt` owns **every** CTest
registration for the wasm module; the module directory owns only target definitions.

It is added from the root `CMakeLists.txt`'s `if(BDK_BUILD_WASM)` block, immediately
after the module directory — **never** from `test/CMakeLists.txt`. That file belongs to
the native build (`BDK_BUILD_CORE=ON`) and hard-requires
`Boost::unit_test_framework`, neither of which exists in the standalone Emscripten
configure.

## `node` must be on `PATH` at configure time

Ten of the twelve entries are node scripts and are registered inside
`if(BDK_NODE_EXECUTABLE)` guards. If `node` is absent when CMake configures, those ten
registrations are simply **not created** — the configure log carries a
`message(WARNING)` saying so, and `ctest` then runs the remaining two
(`wasm_build_optimized`, `wasm_size_gate`) and reports green. `build.sh` no longer
preflights `node` (it is a build-only script and nothing in the compile or link path
touches node), so this is on you locally; CI protects itself by asserting all twelve
registrations **by name** before running `ctest`.

Check it after configuring:

```console
( cd build-wasm && ctest -N )   # twelve entries, not two
```

## The twelve CTest entries

| Test | Label(s) | What it covers |
|------|----------|----------------|
| `wasm_build_optimized` | `wasm;fixture` | fixture (`wasm_optimized`): brings the four `wasm-opt` sidecars up to date, so no test can validate a raw or stale artifact |
| `wasm_functional_node` | `wasm;functional` | `test.mjs` against `dist/bdk-core.mjs`: real positive and negative transaction vectors, the batch and Spend ABIs, the digest/key primitives |
| `wasm_functional_browser` | `wasm;functional` | the same suite against the browser/worker loader, driven in Node with an injected `wasmBinary` |
| `wasm_functional_umd` | `wasm;functional` | `test-umd.mjs`: the UMD bundle evaluated in a `node:vm` context |
| `wasm_functional_slim_umd` | `wasm;functional` | the same, against the `MINIMAL_RUNTIME` slim bundle (no compatibility layer, so the vector shims are absent by design) |
| `wasm_size_gate` | `wasm;sizegate` | loader + `.wasm` bytes per variant against a 300000-byte ceiling; reports every oversized bundle and fails once |
| `wasm_secp256k1_build` | `wasm;secp256k1;fixture` | fixture (`wasm_secp256k1_build`): builds the two `EXCLUDE_FROM_ALL` suite executables and, transitively, the patched `tests.c` |
| `wasm_secp256k1_tests` | `wasm;secp256k1;parity` | libsecp256k1's own unit suite, compiled with `VERIFY`, run against the substituted runtime verification tables |
| `wasm_secp256k1_noverify_tests` | `wasm;secp256k1;parity` | the same suite without `VERIFY`, exactly as upstream separates the two |
| `wasm_secp256k1_first_call_parity` | `wasm;parity;functional` | known-bug reproducer, **inverted exit semantics** — see below |
| `wasm_secp256k1_verify_corpus` | `wasm;secp256k1;fixture` | fixture (`wasm_secp256k1_verify_corpus`): signs a spread of edge and deterministic-random key/digest inputs and records the definitional verdicts into the build tree |
| `wasm_secp256k1_verify_parity_node` | `wasm;parity;functional` | verifies that corpus through the substituted tables: every valid signature must verify, every tampered one must be rejected |

Fixture ordering: `wasm_optimized` is required by the four functional suites, the size
gate, the first-call entry and the corpus fixture; `wasm_secp256k1_build` is required by
the two unit suites; `wasm_secp256k1_verify_corpus` is additionally required by
`wasm_secp256k1_verify_parity_node`.

### `test-first-call.mjs` has inverted exit semantics

`wasm_secp256k1_first_call_parity` is **green while the bug still reproduces**.
`TweakPublicKeyAdd` reaches the fixed-base tables but does not prime them, so on a
freshly instantiated module it operates on all-zero runtime tables and returns a wrong
point. The script exits 0 only when every invariant holds *and* the mismatch reproduces
on every case; it exits 1 when first-call parity holds (the defensive fix has landed —
at which point the entry should become a must-pass parity test) and for any real
regression. `WILL_FAIL` is deliberately **not** used: it would mask setup and
prepared-path regressions.

The tracked rationale for the substitution this exercises is, in order of depth:
`test/types/test-first-call.mjs`'s own header comment,
`module/typesbdk/wasm/wasm_tests.c`'s header (why upstream's suite needs the
`split_128` → `split_lambda` redefinition), and the
`RISK — THIS SUBSTITUTION IS UNVERIFIED` block in
`module/typesbdk/wasm/CMakeLists.txt`.

## Route 1 — `ctest` (the full validation, and what CI runs)

```console
source /path/to/emsdk/emsdk_env.sh
export BOOST_ROOT=/path/to/boost_1.85.0
module/typesbdk/wasm/build.sh
( cd build-wasm && ctest --output-on-failure )
```

Subsets and knobs:

```console
( cd build-wasm && ctest -L wasm )                    # everything here
( cd build-wasm && ctest -L functional )              # the four suites + the parity legs
( cd build-wasm && ctest -L secp256k1 )               # the unit suites and their fixtures
emcmake cmake -S . -B build-wasm -DWASM_SECP256K1_TEST_ITERS=16 …   # deeper randomised coverage (default 4)
```

## Route 2 — the toolchain-free node runner

`package.json` is private and has zero dependencies, so the artifact-level tests can run
against the **committed** artifacts with no Emscripten, no CMake and no build tree — the
default module argument of every runner points at
`../../module/typesbdk/wasm/bdk-core*`:

```console
cd test/types
npm test              # 4 functional suites + the first-call regression
npm run test:parity   # corpus generation + verification parity
npm run bench         # node benchmark.mjs 5000 11
```

This is a convenience, **not** a CI gate, and it is **not** the full validation. It
covers the four functional suites, the first-call regression and the corpus parity leg.
It does **not** cover the two secp256k1 unit suites (they require compilation) or the
size gate (it belongs with a fresh build). `npm run test:parity` writes `.corpus.json`
here; it is gitignored and must never be committed.

`node test/types/benchmark.mjs <iterations> <samples>` also runs directly from the
repository root.
