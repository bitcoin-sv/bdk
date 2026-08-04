# Refactoring plan — decoupling the `module/typesbdk` WASM module

This document is the implementation plan for restructuring the TypeScript/WASM binding
(`module/typesbdk`) so that build inputs, committed artifacts and tests live in separate,
single-purpose places, and so that publishing the committed artifacts is a CI decision rather than
a side effect of the developer build script.

It is a plan only. No file is moved and no build/CI file is edited by this document.

**Scope**

In scope:

- `module/typesbdk/wasm/` — directory contents and `CMakeLists.txt`
- `module/typesbdk/wasm/build.sh`
- new directory `test/types/`
- root `CMakeLists.txt` (one `add_subdirectory` line)
- `.github/workflows/build_wasm.yaml`
- `.github/workflows/build_bdk.yaml`
- documentation that references any of the above

Out of scope (must not be touched):

- `module/rustbdk/**`, `test/rust/**`, `.github/workflows/rust_bdk.yaml`,
  `.github/workflows/rustbdk_archive_refresh.yaml`
- `core/**`, `cmake/**`, `core/bdk-core-recipe.cmake`
- `test/CMakeLists.txt` — it belongs to the native build, which never configures the WASM module
- the WASM C/C++/glue sources themselves (no behaviour change to the shipped module)
- the eight committed artifacts under `module/typesbdk/wasm/` — they stay committed, at the same
  paths, with the same names

---

## 1. Current state

### 1.1 What lives in `module/typesbdk/wasm/` today

All of the following are in one directory (from `git ls-files module/typesbdk/wasm`):

| Kind | Files |
|------|-------|
| Build system | `CMakeLists.txt`, `build.sh`, `wasm_tests_patch.cmake` |
| C/C++ sources | `txvalidator_wasm.cpp`, `txvalidator_wasm.h`, `big_int_boost.cpp`, `memory_cleanse_wasm.cpp`, `secp256k1_runtime.c`, `secp256k1_runtime_precomputed.c`, `secp256k1_runtime_precomputed.h`, `wasm_tests.c` |
| Emscripten glue (`--pre-js`/`--post-js`/`--extern-pre-js`) | `txvalidator_wasm.js`, `txvalidator_wasm_compat.js`, `txvalidator_wasm_snapshot.js`, `txvalidator_wasm_minimal_extern.js`, `txvalidator_wasm_minimal_pre.js` |
| Committed artifacts (8) | `bdk-core.mjs`, `bdk-core.wasm`, `bdk-core.browser.mjs`, `bdk-core.browser.wasm`, `bdk-core.umd.js`, `bdk-core.umd.wasm`, `bdk-core.slim.umd.js`, `bdk-core.slim.umd.wasm` |
| Test / bench scripts and data | `test.mjs`, `test-umd.mjs`, `test-suite.mjs`, `test-first-call.mjs`, `test-verify-parity.mjs`, `verify-corpus-gen.mjs`, `vectors.json`, `benchmark.mjs` |

### 1.2 How the WASM project is configured

- Root `CMakeLists.txt:158-160` adds `module/typesbdk/wasm` when `BDK_BUILD_WASM=ON`; the
  standalone-contract guards at `CMakeLists.txt:54-61` require Emscripten and
  `BDK_BUILD_CORE=OFF`.
- Root `CMakeLists.txt:162-170` adds `test/` **only** when `BDK_BUILD_CORE` is ON, so `test/` is
  never part of a WASM configure. `test/CMakeLists.txt` also unconditionally requires
  `Boost::unit_test_framework` (`test/CMakeLists.txt:8-12`), which the WASM environment does not
  provide.
- `enable_testing()` is called once at root scope from `cmake/BDKInit.cmake:155`, so `add_test()`
  from any subdirectory is registered. The current WASM build tree confirms this: the generated
  `build-wasm/CTestTestfile.cmake` contains `subdirs("module/typesbdk/wasm")`.

### 1.3 What `module/typesbdk/wasm/CMakeLists.txt` currently mixes

Target definitions (must stay):

- Boost import + pinned-version enforcement (`:8-42`), `-ffile-prefix-map` normalisation (`:44-52`)
- `bdk_core_wasm` via `bdk_add_core_library()` from the shared recipe (`:77-90`)
- secp256k1 rewiring — `secp256k1_precomputed` sources, `secp256k1` sources/includes (`:92-158`)
- `wasm-opt` resolution and `BDK_WASM_OPT` (`:160-204`)
- `BDK_WASM_DIST_DIR` (`:209`)
- `add_bdk_wasm_target()` and the four bindings + `*_optimized` custom targets (`:211-319`)
- patched `tests.c` generation and the two `wasm_secp256k1_*` executables (`:395-507`)
- `bdk_wasm_install_insource` (`:584-601`)

Test registrations (must move to `test/types/`):

- `find_program(BDK_NODE_EXECUTABLE ...)` (`:321`)
- `wasm_build_optimized` fixture (`:326-331`)
- `wasm_functional_node`, `wasm_functional_browser`, `wasm_functional_umd`,
  `wasm_functional_slim_umd` (`:333-360`)
- `wasm_size_gate` and its generated `wasm_size_gate.cmake` (`:365-393`)
- `WASM_SECP256K1_TEST_ITERS` cache option (`:429-430`)
- `wasm_secp256k1_build` fixture, `wasm_secp256k1_tests`, `wasm_secp256k1_noverify_tests` test
  registrations (`:509-529`)
- `wasm_secp256k1_first_call_parity` (`:538-544`)
- `wasm_secp256k1_verify_corpus` fixture and `wasm_secp256k1_verify_parity_node` (`:551-569`)

### 1.4 What `build.sh` currently does

`module/typesbdk/wasm/build.sh` has four phases: preflight (`:9-15`), configure (`:38-56`), build
(`:63`), then **validate** (`:72-82`, `BDK_WASM_RUN_TESTS`, default on) and **publish**
(`:88-90`, `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS`, default off, refuses to publish when validation
was skipped).

### 1.5 What CI currently does

`build_wasm.yaml`: single `ubuntu-22.04` job (WASM output is OS-independent, no matrix). It builds
via `build.sh` with `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS` set to `1` whenever the effective
bitcoin-sv commit equals the default pin (`:85-90`), runs a standalone `emcmake` configure smoke
(`:94-104`), then enforces byte reproducibility of the tracked artifacts with
`git diff --exit-code` over the eight files (`:109-120`), and uploads the build-tree `dist/`
directory (`:127-140`).

`build_bdk.yaml`: `build-bdk` matrix (`ubuntu-22.04-arm`, `ubuntu-22.04`, `macos-15`) builds and
tests the native tree with `-DBUILD_MODULE_GOLANG_INSTALL_INSOURCE=ON`, detects whether
`module/gobdk/bdkcgo/libGoBDK_*.a` changed (`:127-139`), uploads each changed archive
(`:141-148`), and the `commit-static-gobdk` job (`:168-209`) commits and pushes them under a
five-part gate (dispatch + `commit-built-binaries` + `success()` + `modified` + no
`[GoBDKUpdate]` marker).

---

## 2. Target layout

```
module/typesbdk/
  wasm/
    README.md                     # NEW - the layout: build inputs + committed artifacts, tests moved
    CMakeLists.txt                # targets only, no add_test
    build.sh                      # preflight + configure + build, nothing else
    txvalidator_wasm.cpp/.h
    txvalidator_wasm.js
    txvalidator_wasm_compat.js
    txvalidator_wasm_snapshot.js
    txvalidator_wasm_minimal_extern.js
    txvalidator_wasm_minimal_pre.js
    big_int_boost.cpp
    memory_cleanse_wasm.cpp
    secp256k1_runtime.c
    secp256k1_runtime_precomputed.c/.h
    wasm_tests.c
    wasm_tests_patch.cmake
    bdk-core.mjs                  # 8 committed artifacts, unchanged paths
    bdk-core.wasm
    bdk-core.browser.mjs
    bdk-core.browser.wasm
    bdk-core.umd.js
    bdk-core.umd.wasm
    bdk-core.slim.umd.js
    bdk-core.slim.umd.wasm
  examples/                       # unchanged location; README + backend example fixed

test/types/                       # NEW - mirrors test/golang and test/rust
  CMakeLists.txt                  # owns EVERY wasm CTest registration
  README.md
  package.json                    # optional toolchain-free node runner
  test.mjs
  test-umd.mjs
  test-suite.mjs
  test-first-call.mjs
  test-verify-parity.mjs
  verify-corpus-gen.mjs
  benchmark.mjs
  vectors.json
```

Rationale, one line per directory:

- `module/typesbdk/wasm/` answers *"what is compiled, and what is shipped"*.
- `test/types/` answers *"what proves it works"*, exactly as `test/golang/` and `test/rust/` do for
  the other two bindings.

---

## 3. Path inventory (old → new)

### 3.1 Files that move

| Old path | New path | Notes |
|----------|----------|-------|
| `module/typesbdk/wasm/test.mjs` | `test/types/test.mjs` | default module argument changes (§5.4) |
| `module/typesbdk/wasm/test-umd.mjs` | `test/types/test-umd.mjs` | default module argument changes (§5.4) |
| `module/typesbdk/wasm/test-suite.mjs` | `test/types/test-suite.mjs` | imported by `test.mjs` and `test-umd.mjs`; loads `./vectors.json` via `import.meta.url` — unchanged because it moves together with `vectors.json` |
| `module/typesbdk/wasm/test-first-call.mjs` | `test/types/test-first-call.mjs` | default module argument changes (§5.4) |
| `module/typesbdk/wasm/test-verify-parity.mjs` | `test/types/test-verify-parity.mjs` | default module argument changes (§5.4) |
| `module/typesbdk/wasm/verify-corpus-gen.mjs` | `test/types/verify-corpus-gen.mjs` | default module argument changes (§5.4) |
| `module/typesbdk/wasm/benchmark.mjs` | `test/types/benchmark.mjs` | **has a hard-coded `import ... from './bdk-core.mjs'`** — must be repointed (§5.4) |
| `module/typesbdk/wasm/vectors.json` | `test/types/vectors.json` | consumed by `test-suite.mjs` and `benchmark.mjs` via `new URL('./vectors.json', import.meta.url)` |

### 3.2 Files that stay put

Everything else under `module/typesbdk/wasm/`: `CMakeLists.txt`, `build.sh`, the C/C++ sources, the
five glue `.js` files, `wasm_tests.c`, `wasm_tests_patch.cmake`, and the eight committed artifacts.

### 3.3 References to the moved files that must be updated

| Referencing file | Location | Current reference | Action |
|------------------|----------|-------------------|--------|
| `module/typesbdk/wasm/CMakeLists.txt` | `:321` | `find_program(BDK_NODE_EXECUTABLE ...)` | move to `test/types/CMakeLists.txt` |
| `module/typesbdk/wasm/CMakeLists.txt` | `:326-331` | `wasm_build_optimized` | move |
| `module/typesbdk/wasm/CMakeLists.txt` | `:333-360` | `${CMAKE_CURRENT_SOURCE_DIR}/test.mjs`, `.../test-umd.mjs` | move; path becomes `test/types` source dir |
| `module/typesbdk/wasm/CMakeLists.txt` | `:365-393` | generated `wasm_size_gate.cmake` + `wasm_size_gate` test | move (script generated into the `test/types` binary dir) |
| `module/typesbdk/wasm/CMakeLists.txt` | `:429-430` | `WASM_SECP256K1_TEST_ITERS` | move |
| `module/typesbdk/wasm/CMakeLists.txt` | `:509-529` | `wasm_secp256k1_build`, `wasm_secp256k1_tests`, `wasm_secp256k1_noverify_tests` | move |
| `module/typesbdk/wasm/CMakeLists.txt` | `:538-544` | `.../test-first-call.mjs` | move |
| `module/typesbdk/wasm/CMakeLists.txt` | `:551-569` | `.../verify-corpus-gen.mjs`, `.../test-verify-parity.mjs`, corpus JSON in the binary dir | move |
| `module/typesbdk/wasm/CMakeLists.txt` | `:356-359` | warning text *"build.sh preflights node before configuring, so its validated path always registers them"* | move with the `find_program` **and reword** — after §5.6 that claim is false: `build.sh` no longer preflights node. Point at `test/types/README.md` instead |
| `module/typesbdk/wasm/build.sh` | `:9` | preflight list including `node`, `ctest` | drop both — a build-only script requires build tools only (§5.6) |
| `module/typesbdk/wasm/build.sh` | `:66-90` | ctest phase + publish phase | delete (§5.6) |
| `CMakeLists.txt` (root) | `:158-160` | `add_subdirectory(module/typesbdk/wasm)` | add `add_subdirectory(test/types)` right after it, inside the same `if(BDK_BUILD_WASM)` block (§5.3) |
| `.github/workflows/build_wasm.yaml` | `:5-18` | path filters | add `test/types/**` (§5.7) |
| `.github/workflows/build_wasm.yaml` | `:19-24` | `workflow_dispatch` trigger + its `bitcoin-sv-commit` input | **delete** — `build_bdk.yaml` is the single manual dispatch entry (§5.7) |
| `.github/workflows/build_wasm.yaml` | `:36-47` | `bsvcommit` resolution branching on `github.event_name == 'workflow_dispatch'` | rewrite for the `workflow_call` input (§5.7) |
| `.github/workflows/build_wasm.yaml` | `:85-90` | build step with `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS` | replace with plain build + separate ctest step (§5.7) |
| `.github/workflows/build_wasm.yaml` | `:106-120` | *"Verify committed artifacts are reproducible"* | delete (§5.7) |
| `module/typesbdk/examples/README.md` | `:89` | `node module/typesbdk/wasm/benchmark.mjs 5000 11` | → `node test/types/benchmark.mjs 5000 11` |
| `module/typesbdk/examples/README.md` | `:19-22`, `:38`, `:51-55` | `build.sh` "validate" claims, `BDK_WASM_RUN_SECP_TESTS` (already dead), `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS` | rewrite (§7) |
| `module/typesbdk/examples/backend/index.mjs` | `:1-20` | embind `VectorUInt8`/`VerifyScript` usage | **mandatory** — rewrite, or at minimum flag with a stale-ABI header comment (§7.5, commit C9) |
| `documentation/docs/build.md` | `:210-215` | `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS=1 module/typesbdk/wasm/build.sh` | rewrite (§7) |
| `documentation/docs/architecture.md` | `:135-147` | `commit-static-gobdk` gate description | rewrite for the unified commit job (§7) |
| `documentation/docs/architecture.md` | `:165-174` | `build.sh` "configure/build/validate", *"CI … fails if the tracked bytes drift"* | rewrite (§7) |
| `documentation/docs/directories.md` | `:15-18` | `test/` tree listing (no `rust`, no `types`) | add `test/types` (and `test/rust`, which is also missing) |

`test/CMakeLists.txt` is **not** in this table: it is deliberately not modified.

---

## 4. Design decisions

1. **`test/types` is added from the standalone WASM project, not from `test/CMakeLists.txt`.**
   `test/CMakeLists.txt` is only reachable when `BDK_BUILD_CORE=ON` (root `CMakeLists.txt:162`),
   and it hard-requires `Boost::unit_test_framework`. The WASM configure is a different project
   shape (`BDK_BUILD_CORE=OFF`, Emscripten only). Adding `test/types` from the root's
   `if(BDK_BUILD_WASM)` block keeps the two test trees independent and keeps the native build
   completely unaffected.
2. **`test/types` defines no build targets.** It only registers tests. Consequences: the WASM
   `ALL` target is byte-for-byte unchanged, and the shipped artifacts cannot be perturbed by test
   code.
3. **Committed artifacts stay committed, and staleness is accepted by design.** This matches how
   `module/gobdk/bdkcgo/libGoBDK_*.a` is handled: the tracked binary is a convenience for
   consumers, refreshed by an explicit CI dispatch, not a build-reproducibility contract enforced
   on every pull request.
4. **`build.sh` builds; `ctest` validates; the CMake target publishes.** One responsibility each.
   The developer runs `build.sh` then `ctest` in the build directory; CI runs the same two steps;
   only the CI commit path additionally invokes `bdk_wasm_install_insource`.
5. **One commit job, one push.** Two jobs pushing to the same branch race. A single job that
   stages both artifact sets and pushes once cannot.
6. **One manual entry point.** `build_bdk.yaml` is the only workflow with a `workflow_dispatch`
   trigger for this pipeline; `build_wasm.yaml` becomes reusable-and-automatic only
   (`pull_request`, `push`, `workflow_call`). Two manual entries would let the wasm leg be run and
   published independently of the all-or-nothing commit gate, which is exactly the coupling the
   single commit job exists to guarantee.
7. **"Did it change" is answered where the files are, not where they were built.** The commit job
   stages what actually arrived and lets `git diff --cached --quiet` decide, per artifact family.
   Build-leg flags are hints; a matrix job cannot produce a trustworthy aggregate flag at all
   (§5.8).

---

## 5. Detailed change specification

### 5.1 `module/typesbdk/wasm/CMakeLists.txt`

Remove: every `add_test`, every `set_tests_properties`, the `find_program(BDK_NODE_EXECUTABLE …)`,
the `WASM_SECP256K1_TEST_ITERS` cache entry, the `file(WRITE … wasm_size_gate.cmake)` block, and
both `if(BDK_NODE_EXECUTABLE) … else() message(WARNING …) endif()` wrappers. The bodies of those
`if` blocks that create **targets** (the `wasm_secp256k1_*` executables at `:481-507`) are already
outside the node guard and stay unconditional — verify this when editing: only the registration
half moves.

Keep: everything listed in §1.3 as "target definitions", verbatim, including the long explanatory
comments (`:54-76` on why upstream's curve suites are not enabled, `:97-121` on the unverified
table substitution, `:432-479` on mirroring the sub-build's compile definitions). Those comments
document target configuration, not test policy, so they must not migrate.

Add, immediately after `set(BDK_WASM_DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")` (`:209`):

```cmake
# Published to the parent (root) scope so the sibling test directory, added from
# the same if(BDK_BUILD_WASM) block, can register tests against the dist artifacts.
# A sibling directory does not inherit variables from this scope, only from the parent.
set(BDK_WASM_DIST_DIR "${BDK_WASM_DIST_DIR}" PARENT_SCOPE)
```

Rationale for `PARENT_SCOPE` rather than a cache entry: the dist directory is derived state, and
this file already documents (`:204`) a deliberate policy of keeping derived values out of the
cache. If a future arrangement needs the value from an unrelated scope, promote it to
`set(... CACHE INTERNAL "")` at that point — not now.

Also add a short header comment stating the contract:

```cmake
# Target definitions only. Every CTest registration for this module lives in
# test/types/CMakeLists.txt, which the root CMakeLists.txt adds alongside this
# directory when BDK_BUILD_WASM=ON.
```

Everything `test/types` needs is otherwise already globally reachable:

- target names (`bdk_wasm_optimized`, `wasm_secp256k1_tests`, …) — CMake targets are global
- `$<TARGET_FILE:wasm_secp256k1_tests>` — generator expressions on targets resolve globally
- `${CMAKE_BINARY_DIR}` — available in every scope
- `BDK_NODE_EXECUTABLE` — `find_program` writes a cache entry, so it can simply be declared in
  `test/types` where it is used

### 5.2 `test/types/CMakeLists.txt` (new)

Owns **all** registrations. Illustrative structure (final wording during implementation; the
`add_test`/`set_tests_properties` bodies are lifted verbatim from
`module/typesbdk/wasm/CMakeLists.txt` with the two path substitutions noted below):

```cmake
#################################################################
#  Independent TypeScript/WASM application-level tests          #
#                                                               #
#  Mirrors test/golang and test/rust: a separate consumer of     #
#  what module/typesbdk/wasm ships. This directory owns EVERY    #
#  CTest registration for the wasm module; the module directory  #
#  owns only target definitions.                                 #
#                                                               #
#  Added from the root CMakeLists.txt if(BDK_BUILD_WASM) block,  #
#  NOT from test/CMakeLists.txt: that file belongs to the native  #
#  build (BDK_BUILD_CORE=ON) and requires                        #
#  Boost::unit_test_framework, neither of which exists in the    #
#  standalone Emscripten configure.                              #
#################################################################

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "test/types is part of the standalone WASM build; configure with emcmake")
endif()
if(NOT TARGET bdk_wasm)
  message(FATAL_ERROR "test/types requires the wasm module targets; add module/typesbdk/wasm first")
endif()
if(NOT BDK_WASM_DIST_DIR)
  message(FATAL_ERROR "BDK_WASM_DIST_DIR was not published by module/typesbdk/wasm")
endif()

find_program(BDK_NODE_EXECUTABLE NAMES node NO_CMAKE_FIND_ROOT_PATH)

# (1) optimize fixture  -> wasm_build_optimized
# (2) 4 functional tests -> wasm_functional_{node,browser,umd,slim_umd}
# (3) size gate          -> wasm_size_gate  (max_bundle_bytes 300000, unchanged)
# (4) secp256k1 parity   -> wasm_secp256k1_build (fixture),
#                           wasm_secp256k1_tests, wasm_secp256k1_noverify_tests
# (5) first-call regression -> wasm_secp256k1_first_call_parity
# (6) corpus fixture + parity -> wasm_secp256k1_verify_corpus,
#                                wasm_secp256k1_verify_parity_node
```

Substitutions applied while lifting:

| In the old registration | Becomes |
|-------------------------|---------|
| `${CMAKE_CURRENT_SOURCE_DIR}/<script>.mjs` | `${CMAKE_CURRENT_SOURCE_DIR}/<script>.mjs` — same expression, now resolving inside `test/types` |
| `WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"` | unchanged expression, now `test/types` (all module paths are passed absolute, so this only affects relative resolution) |
| `${CMAKE_CURRENT_BINARY_DIR}/wasm_size_gate.cmake` | unchanged expression, now under the `test/types` binary dir |
| `${CMAKE_CURRENT_BINARY_DIR}/wasm_secp256k1_corpus.json` | unchanged expression, now under the `test/types` binary dir |
| `${BDK_WASM_DIST_DIR}/…` | unchanged — the value now arrives from the parent scope |

Invariants to preserve exactly (a diff of `ctest -N` output before and after must be empty):

- test names: `wasm_build_optimized`, `wasm_functional_node`, `wasm_functional_browser`,
  `wasm_functional_umd`, `wasm_functional_slim_umd`, `wasm_size_gate`, `wasm_secp256k1_build`,
  `wasm_secp256k1_tests`, `wasm_secp256k1_noverify_tests`, `wasm_secp256k1_first_call_parity`,
  `wasm_secp256k1_verify_corpus`, `wasm_secp256k1_verify_parity_node`
- labels, `TIMEOUT` values, `FIXTURES_SETUP`/`FIXTURES_REQUIRED` wiring
- the size ceiling literal `300000` — moving it is not the place to change it
- `WASM_SECP256K1_TEST_ITERS` default `4`, still a cache `STRING`
- `wasm_secp256k1_first_call_parity` must **not** gain `WILL_FAIL`; the script owns its inverted
  exit semantics (see its header comment)
- the `else() message(WARNING "node not found: …")` branches, with the text updated to point at
  `test/types` rather than at `build.sh`

### 5.3 Root `CMakeLists.txt`

```cmake
## The typesbdk WASM module is a standalone build: it assembles its own core
## variant from the shared recipe in its own directory
if(BDK_BUILD_WASM)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/module/typesbdk/wasm)
  ## The module's tests live outside the module, like test/golang and test/rust.
  ## They are added here (and never from test/CMakeLists.txt, which is native-only)
  ## so the module directory scope has already defined every target they register.
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test/types)
endif()
```

Ordering is load-bearing: `test/types` asserts `TARGET bdk_wasm` and reads `BDK_WASM_DIST_DIR`, so
it must come second.

Note: `.github/workflows/build_wasm.yaml`'s "Standalone configure smoke" step configures the same
root project and then builds only `--target bdk_wasm`. It will therefore also configure
`test/types`. That is harmless — registration costs nothing and builds nothing — and it means the
smoke step also proves `test/types` configures cleanly.

### 5.4 Node script adjustments

Two problems appear the moment the scripts move, and both must be fixed in the same change set.

**(a) `benchmark.mjs` statically imports the committed artifact by relative path.**
`module/typesbdk/wasm/benchmark.mjs:4` is `import createBdkModule from './bdk-core.mjs'`. After
the move that path does not exist. Fix:

```js
import createBdkModule from '../../module/typesbdk/wasm/bdk-core.mjs'
```

The documented CLI (`node test/types/benchmark.mjs <iterations> <samples>`) is unchanged.
Optionally accept a `BDK_WASM_MODULE` environment override via dynamic import; that is a
nice-to-have, not required.

**(b) every runner's default module argument resolves relative to the script.**
`test.mjs:5-8`, `test-umd.mjs:8-10`, `test-first-call.mjs:27-29`, `test-verify-parity.mjs:12-18`
and `verify-corpus-gen.mjs:25-31` all do
`const moduleName = process.argv[2] ?? '<default>'` followed by
`new URL(moduleName, import.meta.url)`. CTest always passes an absolute path, so the CTest runs are
unaffected; the *default* currently resolves to the committed artifact sitting beside the script,
and after the move would resolve to a non-existent `test/types/bdk-core.mjs`.

Change each default to the committed artifact's new relative location:

| Script | Old default | New default |
|--------|-------------|-------------|
| `test.mjs` | `'bdk-core.mjs'` | `'../../module/typesbdk/wasm/bdk-core.mjs'` |
| `test-umd.mjs` | `'bdk-core.umd.js'` | `'../../module/typesbdk/wasm/bdk-core.umd.js'` |
| `test-first-call.mjs` | `'bdk-core.mjs'` | `'../../module/typesbdk/wasm/bdk-core.mjs'` |
| `test-verify-parity.mjs` | `'bdk-core.mjs'` | `'../../module/typesbdk/wasm/bdk-core.mjs'` |
| `verify-corpus-gen.mjs` | `'bdk-core.mjs'` | `'../../module/typesbdk/wasm/bdk-core.mjs'` |

This is what makes §5.5 work: with no argument, every runner targets the **committed** artifacts.
`new URL()` resolution keeps working for both the relative default and the absolute CTest argument,
and the `.browser.`/`.slim.` sniffing in those scripts is substring-based, so a longer path is
harmless.

No other content change to any moved script. `test-suite.mjs` and `vectors.json` need no edit at
all because they move together.

### 5.5 Optional toolchain-free node runner

`test/types/package.json` (private, zero dependencies), so the artifact-level tests can run against
the committed artifacts with no Emscripten, no CMake and no build tree:

```json
{
  "name": "bdk-wasm-tests",
  "private": true,
  "description": "Artifact-level tests for the committed module/typesbdk/wasm bundles",
  "scripts": {
    "test": "node test.mjs && node test.mjs ../../module/typesbdk/wasm/bdk-core.browser.mjs && node test-umd.mjs && node test-umd.mjs ../../module/typesbdk/wasm/bdk-core.slim.umd.js && node test-first-call.mjs",
    "test:parity": "node verify-corpus-gen.mjs ../../module/typesbdk/wasm/bdk-core.mjs .corpus.json && node test-verify-parity.mjs ../../module/typesbdk/wasm/bdk-core.mjs .corpus.json",
    "bench": "node benchmark.mjs 5000 11"
  }
}
```

- Add `/test/types/.corpus.json` to `.gitignore` (the generated corpus must never be committed).
- The two secp256k1 unit suites and the size gate are **not** in this runner: the suites require
  compilation, and the size gate belongs with a fresh build. This runner covers the four functional
  suites, the first-call regression and the corpus parity leg only — that must be stated in
  `test/types/README.md` so nobody mistakes it for the full validation.
- This runner is a convenience, not a CI gate. CI uses `ctest`.

### 5.6 `module/typesbdk/wasm/build.sh`

Delete lines `66-90` outright: the whole `BDK_WASM_RUN_TESTS` ctest phase (including the
`ctest -N` non-empty assertion and the `summary_label` branching) and the whole
`BDK_WASM_UPDATE_COMMITTED_ARTIFACTS` publish phase (including the "refusing to publish" guard,
which has nothing left to guard).

Keep: the preflight loop (`:9-15`), the bitcoin-sv/Boost discovery comments (`:17-29`),
`BDK_WASM_CLEAN` (`:31-33`), the configure (`:38-56`), the build (`:63`), `dist_dir`, and the
closing `ls -lh` of the eight dist files with a fixed `Built:` label.

**Reduce the preflight list to build tools only**: drop both `ctest` and `node`, leaving
`cmake emcmake emcc em++ emar emranlib make`. A build-only script must not demand the test runner,
and `node` is a *test* dependency, not a build dependency — nothing in the compile or link path
touches it.

That does lose one property, so it is re-established elsewhere rather than dropped. `node` presence
at **configure** time is what decides whether CMake registers the ten node-based tests
(`test/types/CMakeLists.txt`'s `if(BDK_NODE_EXECUTABLE)` guards); `wasm_build_optimized` and
`wasm_size_gate` register unconditionally. So a node-less configure yields a build tree with **two**
tests that all pass, which a bare `ctest -N | grep -c 'Test #'` count check cannot distinguish from
a healthy tree. Two mitigations, both specified:

- `test/types/CMakeLists.txt` keeps the `message(WARNING "node not found: …")` branches, so the
  configure log says so, and `test/types/README.md` states that node is required for all but two of
  the tests;
- the CI test step asserts the node-dependent tests **by name**, not by count (§5.7 step 5). That is
  the assertion that actually protects CI, and it belongs in CI.

New usage header, replacing nothing and clarifying everything:

```sh
# Build the standalone WASM module. Build ONLY:
#
#   * validation is `ctest` in the build directory, which is where the tests are
#     registered (test/types/CMakeLists.txt):
#         module/typesbdk/wasm/build.sh
#         ( cd build-wasm && ctest --output-on-failure )
#
#   * publishing the eight committed artifacts into the source tree is
#         cmake --build build-wasm --target bdk_wasm_install_insource
#     invoked only by the CI commit path (.github/workflows/build_bdk.yaml).
#
# Environment: BDK_WASM_BUILD_DIR (default <repo>/build-wasm), BDK_WASM_JOBS,
# BDK_WASM_CLEAN (default 1). BDK_WASM_RUN_TESTS and
# BDK_WASM_UPDATE_COMMITTED_ARTIFACTS no longer exist.
#
# Preflight covers build tools only. `node` is needed to RUN the tests (all but
# the optimize fixture and the size gate are node scripts) and must be on PATH at
# CONFIGURE time for CMake to register them; `ctest` ships with cmake.
```

Removing those two environment variables is a user-visible change: anything that sets them silently
loses its effect. §7 lists the documents that mention them.

### 5.7 `.github/workflows/build_wasm.yaml`

**Triggers.** Keep `pull_request` and `push: [master]` with path filters; add `test/types/**` to
both filter lists (and keep `CMakeLists.txt`, `cmake/**`, `core/**`,
`module/typesbdk/wasm/**`, `.github/workflows/build_wasm.yaml`).

**Delete the `workflow_dispatch` trigger** (`:19-24`) and its `bitcoin-sv-commit` input.
`build_bdk.yaml` is the single manual dispatch entry, so this workflow must have exactly two ways
in: the automatic PR/push path, and `workflow_call` from `build_bdk.yaml`. Everything the old
dispatch offered is still reachable — a wasm-only manual run is a `build_bdk.yaml` dispatch with
both commit boxes unticked (it builds and tests both legs and commits nothing), and a custom
bitcoin-sv commit is `build_bdk.yaml`'s existing `bitcoin-sv-commit` input, forwarded through
`workflow_call`.

Add `workflow_call`:

```yaml
  workflow_call:
    inputs:
      bitcoin-sv-commit:
        required: false
        type: string
        default: 879fc8b42168dd0e608dafd51b39c6dabad37d4d
      publish-artifacts:
        description: 'Publish the freshly built artifacts in-source and upload them for the commit job'
        required: false
        type: boolean
        default: false
    outputs:
      modified:
        description: 'true when the freshly built artifacts differ from the committed ones'
        value: ${{ jobs.wasm.outputs.modified }}
```

The `bsvcommit` resolution step (`:36-47`) currently branches on
`github.event_name == 'workflow_dispatch'` and reads `github.event.inputs.bitcoin-sv-commit`.
With the dispatch trigger gone there is only one input source, so the branch disappears:

```bash
# workflow_call supplies the commit; PR/push runs supply nothing and take the pin.
bsvcommit="${{ inputs.bitcoin-sv-commit }}"
[ -n "$bsvcommit" ] || bsvcommit="${{ env.DEFAULT_BITCOIN_SV_COMMIT }}"
echo "bsvcommit=$bsvcommit" >> "$GITHUB_OUTPUT"
```

`inputs.bitcoin-sv-commit` is empty on `pull_request`/`push`, so the `env` fallback keeps those runs
on the pin exactly as today. Also delete the stale comment at `:80-84` that explains when the
"flag stays off" for a custom-commit dispatch — there is no dispatch and no flag any more.

**Job.** `jobs.wasm` gains `outputs: { modified: ${{ steps.wasm_change.outputs.modified }} }`.

**Steps**, in order:

1. `actions/checkout@v4` — unchanged.
2. Define bitcoin-sv commit — as above.
3. Check out bitcoin-sv, install Emscripten 4.0.23, provision the pinned Boost package —
   unchanged. Drop the stale comment at `:49-53` that names the deleted reproducibility gate.
4. **Build**: `source "$RUNNER_TEMP/emsdk/emsdk_env.sh" && module/typesbdk/wasm/build.sh`, with no
   `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS` in `env:` (the variable no longer exists).
5. **Test** (new, separate step, green/red only):

   ```bash
   source "$RUNNER_TEMP/emsdk/emsdk_env.sh"
   cd build-wasm
   registered="$(ctest -N)"
   for expected in wasm_build_optimized wasm_size_gate \
                   wasm_functional_node wasm_functional_browser \
                   wasm_functional_umd wasm_functional_slim_umd \
                   wasm_secp256k1_build \
                   wasm_secp256k1_tests wasm_secp256k1_noverify_tests \
                   wasm_secp256k1_first_call_parity \
                   wasm_secp256k1_verify_corpus wasm_secp256k1_verify_parity_node; do
     grep -q "[[:space:]]${expected}\$" <<<"$registered" || {
       echo "CTest registration missing: $expected (node absent at configure time?)" >&2
       exit 1
     }
   done
   ctest --output-on-failure
   ```

   The non-empty assertion deleted from `build.sh` is re-homed here and **strengthened from a count
   to a name list**. A count cannot tell a healthy tree from a node-less one: without `node`, ten of
   the twelve registrations silently vanish and the remaining two pass, so `ctest -N | grep -c` would
   report a green two-test run. Naming the tests makes that state red. The repository CMake floor is
   3.16, which has neither `--test-dir` nor `--no-tests=error`, hence the `cd` and the manual check.
   Use the build directory `build.sh` used (`BDK_WASM_BUILD_DIR`, default `build-wasm`).

   The list names **all twelve** registrations, including the two fixture-only entries
   (`wasm_secp256k1_build`, `wasm_secp256k1_verify_corpus`). An earlier draft omitted them on the
   grounds that the suites requiring them would fail anyway — true but indirect: a fixture named in
   `FIXTURES_REQUIRED` that has no setup test is silently ignored by CTest, so the failure surfaces
   later and less legibly (a suite executable that was never built). Naming them costs two lines and
   makes a dropped registration fail as a dropped registration. This is a *presence* check that
   accommodates future additions; the *exact set* is pinned once at migration time by acceptance
   criterion 7's before/after `ctest -N` diff, which is where an exact count belongs — a permanent
   `-eq 12` in CI would make every future test addition a CI edit.
6. Standalone configure smoke (direct `emcmake`, no `build.sh`) — unchanged.
7. **Publish in-source** (new, conditional):
   `if: inputs.publish-artifacts && steps.define-bitcoin-sv-commit.outputs.bsvcommit == env.DEFAULT_BITCOIN_SV_COMMIT`
   → `cmake --build build-wasm --target bdk_wasm_install_insource`.
   The default-pin condition is retained deliberately: an off-pin dispatch must never overwrite the
   tracked bytes.
8. **Detect change** (new, `id: wasm_change`, same condition as step 7):
   `git diff --name-only -- module/typesbdk/wasm/bdk-core.mjs … bdk-core.slim.umd.wasm`
   → `modified=true|false` into `$GITHUB_OUTPUT`. When step 7 is skipped, the step is skipped and
   the output is empty, which the caller treats as not-modified.
9. **Upload for the commit job** (new): `actions/upload-artifact@v4`, `name: bdk-wasm-artifacts`,
   the eight tracked paths under `module/typesbdk/wasm/`, `retention-days: 1`, `overwrite: true`,
   `if-no-files-found: error`, conditional on step 8 reporting `modified == 'true'`. This is the
   transport that mirrors how `libGoBDK_*.a` travels today.
10. **Upload validated module** (existing, `name: bdk-wasm`, the build-tree `dist/` paths) —
    unchanged, still unconditional. Keep the comment explaining why it uploads `dist/` and not the
    tracked copies.

**Delete** the *"Verify committed artifacts are reproducible"* step (`:106-120`) entirely.
Consequence, stated plainly so it is a decision and not an accident: the workflow no longer
detects byte drift between the tracked artifacts and a fresh pinned build. Accepted — the tracked
artifacts become a refreshed-on-demand convenience, exactly like the gobdk archives, and a pull
request that changes wasm sources no longer has to carry regenerated binaries.

No replacement drift check of any kind is added — no gate, and no informational report either. A
publish-and-restore report would mean PR/push runs still write the tracked files and still compute a
diff nobody acts on, which is the coupling this change exists to remove. Drift between the tracked
artifacts and a fresh build is expected between refreshes and is not reported.

Bump `timeout-minutes` from 45 to 60 if the separated ctest step pushes total wall time close to
the current limit — the secp256k1 suites alone carry a 3600 s CTest timeout.

### 5.8 `.github/workflows/build_bdk.yaml`

**Inputs.** Replace `commit-built-binaries` with two independent booleans:

```yaml
  workflow_dispatch:
    inputs:
      bitcoin-sv-commit:
        description: 'Custom bitcoin-sv commit to be used to build bdk'
        required: false
        default: 879fc8b42168dd0e608dafd51b39c6dabad37d4d
      commit-gobdk-archives:
        description: 'Commit the gobdk static archives built by this run'
        required: true
        default: 'true'
        type: boolean
      commit-wasm-artifacts:
        description: 'Commit the typesbdk WASM artifacts built by this run'
        required: true
        default: 'true'
        type: boolean
```

Four combinations are all meaningful: go only, wasm only, both, neither (build+test everything,
commit nothing). Renaming the input is a visible break for anyone with a saved dispatch form —
call it out in the release note.

**Jobs.**

- `build-bdk` — matrix, native build/test, per-arch change detection, per-arch archive upload, all
  unchanged. Its `outputs.modified` is **kept for the log but is no longer a gate** (see below), and
  it gains one new step: a per-leg **status marker** upload.

**Aggregating "did it change" across the gobdk matrix.** `build-bdk` is a three-leg matrix, and all
legs write the same job-level output (`outputs.modified` ← `steps.gobdk_change.outputs.modified`).
Job outputs from a matrix are last-writer-wins, so if `ubuntu-22.04` rebuilt a changed
`libGoBDK_linux_x86_64.a` and uploaded it, but `macos-15` (unchanged, and finishing last) wrote
`modified=false`, then `needs.build-bdk.outputs.modified == 'true'` is false and today's gate drops a
genuinely changed, successfully uploaded archive on the floor. Since each leg detects only *its own*
archive, this is not a rare tie — it is the normal case whenever the platforms disagree.

The fix is to stop asking a matrix for a single aggregate answer. **The authoritative change decision
moves into the commit job**, where the whole picture exists: stage the files that actually arrived and
let `git diff --cached --quiet` decide. That makes leg ordering irrelevant and needs no aggregation
job. The job-level `if` therefore no longer references `needs.build-bdk.outputs.modified`, and an
unchanged run reaches the commit job, stages nothing, commits nothing and pushes nothing.

**Distinguishing "nothing was uploaded" from "the download failed".** Moving the decision into the
commit job creates a second problem that must be solved with it. The job now runs even when no
archive changed, so the archive download can legitimately find nothing — but a download that fails
for a real reason (artifact service error, expired retention, a lost upload) produces the *same*
observable state: no files on disk, nothing staged, no commit, **green run**. That is the C5 bug in a
new costume: a changed archive was built and uploaded, and the refresh is silently dropped.

Tolerating the download with `continue-on-error: true` is therefore rejected — it converts every real
download failure into a silent no-commit. Instead, each matrix leg states its intent explicitly, so
the commit job knows what it is *supposed* to receive:

- **In `build-bdk`** (dispatch only, and it runs whether or not the archive changed — no `if` on
  `modified`): write and upload a tiny marker.

  ```yaml
    - name: Publish gobdk change status for this leg
      if: runner.os != 'Windows' && github.event_name == 'workflow_dispatch'
      run: |
        printf 'os_arch=%s\nmodified=%s\n' \
          "${{ env.OS_ARCH }}" "${{ steps.gobdk_change.outputs.modified }}" \
          > "gobdk-status-${{ env.OS_ARCH }}.txt"
    - uses: actions/upload-artifact@v4
      if: runner.os != 'Windows' && github.event_name == 'workflow_dispatch'
      with:
        name: gobdk-status-${{ env.OS_ARCH }}
        path: gobdk-status-${{ env.OS_ARCH }}.txt
        retention-days: 1
        overwrite: true
        if-no-files-found: error
  ```

- **In the commit job**: download the markers **hard** (no `continue-on-error`), derive the expected
  archive set, and assert the download delivered exactly it. `success()` on `needs` already
  guarantees every leg finished, so all three markers must exist; a missing marker is a real fault
  and must be red.

  ```bash
  # ./tmp_status holds one gobdk-status-<os_arch>.txt per matrix leg.
  found="$(find ./tmp_status -type f -name 'gobdk-status-*.txt' | wc -l)"
  [ "$found" -eq 3 ] || { echo "expected 3 gobdk status markers, found $found" >&2; exit 1; }
  expected="$(grep -l 'modified=true' $(find ./tmp_status -name 'gobdk-status-*.txt') 2>/dev/null \
              | xargs -r -n1 sed -n 's/^os_arch=//p')"
  ```

  If `expected` is empty, the archive download and the gobdk staging are both skipped, and the run
  logs "no gobdk archive changed" — a *proven* no-op rather than an assumed one. If `expected` is
  non-empty, `actions/download-artifact@v4` runs with no failure tolerance, and every
  `libGoBDK_<os_arch>.a` named in `expected` must be present afterwards or the job fails.

  **Download layout — `merge-multiple` is required on both `pattern:` downloads.** A
  `download-artifact@v4` step that names a single artifact with `name:` unpacks its contents directly
  into `path`. A step that selects several with `pattern:` does **not**: it creates one
  subdirectory per artifact, `<path>/<artifact-name>/<contents>`, unless `merge-multiple: true` is
  set. Both new gobdk downloads select by pattern, so both set it:

  ```yaml
    - uses: actions/download-artifact@v4          # markers
      with:
        pattern: gobdk-status-*
        path: ./tmp_status
        merge-multiple: true
    - uses: actions/download-artifact@v4          # archives
      with:
        pattern: libGoBDK_*
        path: ./tmp_artifact_gobdk
        merge-multiple: true
  ```

  Without it, the archive assertion below looks for `./tmp_artifact_gobdk/libGoBDK_linux_x86_64.a`
  and finds a **directory** of that exact name containing the file — the upload names each artifact
  `libGoBDK_${OS_ARCH}.a`, extension included, so the per-artifact subdirectory is named identically
  to the file inside it. `[ -f … ]` is false for a directory while `[ -e … ]` would be true, which is
  precisely the kind of near-miss that reads as "the archive did not arrive". Today's workflow never
  hit this because its download is unfiltered and its `find … -exec mv` recurses.

  For the same reason the assertion is written with a recursive `find` rather than a fixed path, so it
  stays correct whether or not the flag survives a future edit, and it moves what it found:

  ```bash
  for os_arch in $expected; do
    archive="$(find ./tmp_artifact_gobdk -type f -name "libGoBDK_${os_arch}.a" -print -quit)"
    [ -n "$archive" ] \
      || { echo "leg $os_arch reported a changed archive but it did not arrive" >&2; exit 1; }
    mv "$archive" ./module/gobdk/bdkcgo/
  done
  ```

  The marker count above already uses a recursive `find`, so it is correct either way too.

  `merge-multiple` requires `actions/download-artifact` v4.1.0 or newer. The `@v4` major-version ref
  used throughout this repository resolves to the latest v4, so it is available; if a future change
  pins an exact `@v4.0.x`, drop the flag and rely on the recursive `find` — do not pin an exact
  version *and* keep a flat-path assumption.

  Hard-code `3` from the matrix length rather than deriving it; if a platform is added to the matrix,
  the assertion must be updated deliberately. Note the marker step is guarded by the same
  `runner.os != 'Windows'` condition as the existing change-detection step (`build_bdk.yaml:129`),
  so the count matches the three Unix legs that actually produce archives.

**The wasm family already has this property** and needs no marker. `build-wasm` is a single job, so
`needs.build-wasm.outputs.modified` is unambiguous: it is `true` exactly when the artifact was
uploaded, the download step is gated on it with **no** failure tolerance, and a failed download is
therefore already a red run. It also selects by `name:` (one artifact), so its contents land flat in
`path` and it needs no `merge-multiple` — the eight files appear directly under `./tmp_artifact_wasm`.
Both families end up with the same guarantee — an expected artifact that does not arrive fails the
job — reached by the means each job shape allows. The staged-diff rule remains the authoritative
commit decision for both.
- `build-wasm` (new):

  ```yaml
    build-wasm:
      if: ${{ github.event_name == 'workflow_dispatch' }}
      uses: ./.github/workflows/build_wasm.yaml
      with:
        bitcoin-sv-commit: ${{ github.event.inputs.bitcoin-sv-commit }}
        publish-artifacts: ${{ github.event.inputs.commit-wasm-artifacts == 'true' }}
  ```

  It runs on **every** dispatch, regardless of either checkbox. That is what makes the
  all-or-nothing gate meaningful: a red wasm leg blocks the gobdk commit too. When
  `commit-wasm-artifacts` is false, the leg still builds and tests but never publishes in-source,
  so it leaves the tracked tree clean and reports `modified` empty.
- `commit-built-artifacts` (replaces `commit-static-gobdk`):

  ```yaml
    commit-built-artifacts:
      runs-on: ubuntu-latest
      needs: [build-bdk, build-wasm]
      # Default success() semantics: ANY red leg blocks ALL commits, including the
      # un-ticked artifact. Dispatch-only, and at least one box must be ticked.
      # Whether anything actually CHANGED is decided per family by the staged diff
      # inside the job -- never by a matrix job's last-writer-wins output.
      if: ${{ github.event_name == 'workflow_dispatch' && success() && (
              github.event.inputs.commit-gobdk-archives == 'true'
              || github.event.inputs.commit-wasm-artifacts == 'true'
            ) }}
  ```

  Steps:

  1. `actions/checkout@v4` with `fetch-depth: 0`.
  2. **Loop-breaker** (`id: markers`): read the head commit message from git and expose two
     booleans:

     ```bash
     subject="$(git log -1 --pretty=%B)"
     grep -q '\[GoBDKUpdate\]'   <<<"$subject" && echo "gobdk_marker=true"  >> "$GITHUB_OUTPUT" || echo "gobdk_marker=false" >> "$GITHUB_OUTPUT"
     grep -q '\[WasmBDKUpdate\]' <<<"$subject" && echo "wasm_marker=true"   >> "$GITHUB_OUTPUT" || echo "wasm_marker=false"  >> "$GITHUB_OUTPUT"
     ```

     **Why a git read and not `github.event.head_commit.message`:** `head_commit` is populated for
     `push` events only. This job is dispatch-only, so the existing expression guard
     `!contains(github.event.head_commit.message, '[GoBDKUpdate]')` evaluates `contains(null, …)`
     → `false` → `!false` → always true, i.e. today's documented loop-breaker is inert on the only
     trigger that can reach the job. Reading git makes it real. Keep the expression form as well if
     parity with the current file is preferred, but do not rely on it alone.
  3. **Read the gobdk status markers** (`id: gobdk_expect`), conditional on
     `github.event.inputs.commit-gobdk-archives == 'true' && steps.markers.outputs.gobdk_marker != 'true'`:
     `actions/download-artifact@v4` with `pattern: 'gobdk-status-*'`, `path: ./tmp_status`,
     `merge-multiple: true`, **no `continue-on-error`**, then the marker-count assertion and
     `expected` derivation shown above. Expose `expected` (space-separated os_arch list) and
     `any=true|false` as step outputs.
  4. **Download gobdk archives**, conditional on the same expression **plus**
     `steps.gobdk_expect.outputs.any == 'true'`: `actions/download-artifact@v4` with
     `pattern: 'libGoBDK_*'`, `path: ./tmp_artifact_gobdk`, `merge-multiple: true`, **no
     `continue-on-error`** — a failure here is a real failure and must go red. Then run the
     find-assert-move loop above, which fails the job for any `expected` archive that did not arrive.
     This replaces today's bare
     `find ./tmp_artifact -type f -name 'libGoBDK_*_*.a' -exec mv {} ./module/gobdk/bdkcgo/ \;`
     (`build_bdk.yaml:186-189`), which moves whatever happens to be there and asserts nothing.

     Use an explicit `pattern`/`path` per artifact family: an unfiltered download would place both
     families under one directory and make the `bdk-core.*` glob ambiguous (the always-uploaded
     `bdk-wasm` dist artifact carries the same file names as `bdk-wasm-artifacts`). The
     `gobdk-status-*` markers need their own `path` for the same reason.
  5. **Download wasm artifacts**, conditional on
     `github.event.inputs.commit-wasm-artifacts == 'true' && needs.build-wasm.outputs.modified == 'true' && steps.markers.outputs.wasm_marker != 'true'`:
     `actions/download-artifact@v4` with `name: bdk-wasm-artifacts`, `path: ./tmp_artifact_wasm` —
     `name:` unpacks flat, so **no** `merge-multiple` and no recursion — then copy the eight files
     into `./module/typesbdk/wasm/`. `build-wasm` is a single job, so its `modified` output is
     unambiguous and is used here purely to skip a download that would find nothing (the artifact is
     only uploaded when it changed, so `name:` would otherwise 404).
  6. **Stage and commit sequentially**, one commit per artifact family, each with its own marker.
     `git diff --cached --quiet` is the **authoritative** per-family change decision: if the staged
     set is empty, no commit is made, whatever any leg reported. It is now a *second* line of defence
     rather than the only one — steps 3-4 have already proven that every archive a leg said it
     changed actually arrived, so an empty staged set here means "nothing changed", not "something
     went missing".

     ```bash
     git config --global user.name  "github-actions[bot]"
     git config --global user.email "github-actions[bot]@users.noreply.github.com"

     # gobdk -- only when the box is ticked, the marker is absent, and step 4 ran.
     git add ./module/gobdk/bdkcgo/libGoBDK_*_*.a
     git diff --cached --quiet || git commit -m "[bot] [GoBDKUpdate] Update gobdk static libraries changes"

     # wasm -- only when the box is ticked and the marker is absent (step 5 ran).
     git add ./module/typesbdk/wasm/bdk-core.mjs  ./module/typesbdk/wasm/bdk-core.wasm \
             ./module/typesbdk/wasm/bdk-core.browser.mjs ./module/typesbdk/wasm/bdk-core.browser.wasm \
             ./module/typesbdk/wasm/bdk-core.umd.js ./module/typesbdk/wasm/bdk-core.umd.wasm \
             ./module/typesbdk/wasm/bdk-core.slim.umd.js ./module/typesbdk/wasm/bdk-core.slim.umd.wasm
     git diff --cached --quiet || git commit -m "[bot] [WasmBDKUpdate] Update typesbdk wasm artifacts"
     ```

     Implement the two halves as two steps carrying the same `if:` conditions as their respective
     download steps, so an un-ticked or marker-suppressed family never even stages. Two separate
     commits are intentional: each carries the marker that suppresses its own rebuild loop, and each
     is independently revertable.

     `git add <glob>` on a path that matches nothing is an error in git, so keep the gobdk `git add`
     inside the gobdk-conditional step (where step 4 has run and `module/gobdk/bdkcgo/libGoBDK_*.a`
     always matches the four tracked archives regardless of what was downloaded).
  7. **One push at the end** — `git push -u origin "$BRANCH_NAME"`, executed exactly once, after
     both commit steps, unconditionally (a run that committed nothing pushes an already-current
     branch, which is a no-op). This is what removes the two-job same-branch race.

     Fix the branch resolution while here: the current job uses `BRANCH_NAME=${{ github.head_ref }}`,
     which is empty on `workflow_dispatch` (it is a pull-request-only context value). Use
     `${{ github.ref_name }}`, which is the dispatched branch. Keep this as a small, separately
     reviewable hunk since it changes existing behaviour.

**Path filters:** `build_bdk.yaml` has no path filters (it runs on all PRs/pushes) — nothing to
update there. Only `build_wasm.yaml` filters, and §5.7 covers it.

---

## 6. Commit strategy

Two phases, deliberately separated.

### Phase A — code and structure, as small reviewed commits only

- Every commit is reviewed and agreed before it lands.
- **No build and no test run between these commits.** The tree is expected to be temporarily
  inconsistent after C1 and C2 (the CMake registrations still point at the old paths); it becomes
  self-consistent again at C3.
- Each commit is a single logical unit that can be read on its own.

Suggested sequence (message prefixes follow the existing history style, e.g. `[wasm cmake] …`):

| # | Commit | Contents |
|---|--------|----------|
| C1 | `[wasm tests] move the node test, bench and vector files to test/types` | pure `git mv` of the 8 files in §3.1. No content edits, so the diff is a rename list and stays reviewable. |
| C2 | `[wasm tests] point the node runners at the committed artifacts by default` | §5.4 — `benchmark.mjs` import, and the five default-argument changes. |
| C3 | `[wasm cmake] give test/types ownership of every wasm CTest registration` | new `test/types/CMakeLists.txt` (§5.2); strip all registrations from `module/typesbdk/wasm/CMakeLists.txt` and publish `BDK_WASM_DIST_DIR` to the parent scope (§5.1); add `add_subdirectory(test/types)` to the root (§5.3). **The tree configures again from here.** |
| C4 | `[wasm tests] add a toolchain-free node runner for the committed artifacts` | `test/types/package.json` + the `.gitignore` entry (§5.5). **The only optional commit in the set** — drop it if the runner is not wanted (open point O1). |
| C5 | `[wasm build] reduce build.sh to preflight, configure and build` | §5.6 — delete the ctest and publish phases, reduce the preflight list to build tools, new usage header. |
| C6 | `[wasm ci] make build_wasm a reusable, non-dispatchable workflow` | §5.7 — remove `workflow_dispatch`, add `workflow_call` + `modified` output, separate ctest step with the named-test assertion, path filters, in-source publish + change detection + upload, removal of the git-diff gate with no replacement. |
| C7 | `[ci] single dispatch entry with independent gobdk and wasm commit checkboxes` | §5.8 — the two boolean inputs, the `build-wasm` reusable-workflow job, the per-leg gobdk status markers, the unified `commit-built-artifacts` job with both loop-breakers, the expected-archive assertion, the staged-diff change decision, and one push. |
| C8 | `[docs] document the typesbdk build/test split and the CI commit gates` | §7 items 1-4 and 6-11 (`module/typesbdk/wasm/README.md`, `test/types/README.md`, workflow comments, `examples/README.md`, `documentation/docs/*`, `ReleaseNote.md`; the `build.sh` header ships in C5). |
| C9 | `[types example] update the stale embind backend example` | §7 item 5 — `module/typesbdk/examples/backend/index.mjs`. **Mandatory.** |

Only C4 is optional. C9 in particular is **not** droppable: the example currently shows an API the
shipped module no longer presents, so leaving it untouched ships a worked example that cannot run.
Its position in the sequence is free (it depends on nothing else), but it must be in the set.

### Phase B — validate once, at the end

Only after the whole change set above is complete:

0. Confirm `node` is on `PATH` **before** configuring. `build.sh` no longer preflights it (§5.6), and
   without it the configure registers only `wasm_build_optimized` and `wasm_size_gate` — step 2 would
   then pass having run two tests. Check with `ctest -N` after step 1: twelve entries, not two.
1. Full clean WASM build:
   `source <emsdk>/emsdk_env.sh && export BOOST_ROOT=… && module/typesbdk/wasm/build.sh`
2. Full test run: `( cd build-wasm && ctest --output-on-failure )` — all twelve tests.
3. Toolchain-free runner, if C4 landed: `( cd test/types && npm test && npm run test:parity )`.
4. Publish-target smoke, which must leave the tree clean or show only expected artifact bytes:
   `cmake --build build-wasm --target bdk_wasm_install_insource && git status --porcelain`
5. Native regression guard — the native build must be untouched by all of this:
   configure and build the native tree as usual and confirm `ctest -N` output is unchanged from
   before the refactor.
6. Workflow syntax check on both edited workflows.

Then **at most one** final fix commit if anything is broken. If more than one fix is needed,
squash them into that single commit before it lands.

---

## 7. Documentation updates

1. **New `module/typesbdk/wasm/README.md`** — required, and it must live in
   `module/typesbdk/wasm/` itself, next to the inputs and artifacts it describes. That is the
   directory a consumer or a newcomer lands in, and after this refactor its contents are the thing
   most in need of explanation (why there are no tests here any more). A header comment block at the
   top of `module/typesbdk/wasm/CMakeLists.txt` is the acceptable alternative form, but the README is
   preferred: it shows up in a directory listing and on the repository web view. Do **not** put this
   content one level up in `module/typesbdk/README.md` — that file is not where the reader is
   looking, and the layout being documented is the `wasm/` layout. Content:
   - this directory holds **build inputs** and the **eight committed artifacts**, nothing else;
   - what each input group is (the binding source, the five Emscripten glue files, the substituted
     `big_int`/`cleanse`/secp256k1 runtime sources, the secp256k1 parity-suite inputs);
   - the artifacts are committed for consumer convenience and are refreshed on demand by CI, not on
     every change — a source change without an artifact refresh is expected and allowed;
   - the tests are in `test/types/`, with a link;
   - `build.sh` builds; `ctest` in the build directory validates;
   `cmake --build <dir> --target bdk_wasm_install_insource` publishes.
2. **New `test/types/README.md`**:
   - what each of the twelve CTest entries covers, and the fixture ordering
     (`wasm_optimized`, `wasm_secp256k1_build`, `wasm_secp256k1_verify_corpus`);
   - the ctest route: `module/typesbdk/wasm/build.sh` then
     `( cd build-wasm && ctest --output-on-failure )`, plus `ctest -L wasm`,
     `ctest -L functional`, `ctest -L secp256k1` and `-DWASM_SECP256K1_TEST_ITERS=<n>`;
   - the toolchain-free route (§5.5): what it covers (four functional suites, first-call
     regression, corpus parity) and explicitly what it does **not** (the two secp256k1 unit suites
     and the size gate);
   - that this directory registers tests only and defines no build targets, and that it is added
     from the root `if(BDK_BUILD_WASM)` block, never from `test/CMakeLists.txt`;
   - that `test-first-call.mjs` is a known-bug reproducer with inverted exit semantics — green means
     the bug still reproduces — pointing at the tracked in-repository rationale: the script's own
     header comment, `module/typesbdk/wasm/wasm_tests.c`'s header (why upstream's suite needs the
     `split_128` → `split_lambda` redefinition), and the `RISK — THIS SUBSTITUTION IS UNVERIFIED`
     block at `module/typesbdk/wasm/CMakeLists.txt:97-121`. Reference **only tracked files**: any
     working note living outside git is invisible to a reader who just cloned the repository, and
     committing new analysis documents is outside this change's scope;
   - that `node` must be on `PATH` at **configure** time or ten of the twelve tests are silently not
     registered (§5.6), and that `build.sh` no longer preflights it.
3. **`module/typesbdk/wasm/build.sh` usage header** — §5.6.
4. **Workflow comments**:
   - `build_wasm.yaml`: what the `workflow_call` inputs mean; that this workflow has **no manual
     trigger** and why (`build_bdk.yaml` is the single manual dispatch entry, and a wasm-only manual
     run is a dispatch with both boxes unticked); that `publish-artifacts` is the only thing that
     ever writes the tracked files; why the default-pin condition remains on the publish step; and
     why the reproducibility gate was removed (drift is accepted, mirroring gobdk, and no report
     replaces it).
   - `build_bdk.yaml`: the two checkboxes and their four combinations; that the wasm leg runs on
     every dispatch regardless of the checkboxes; that `needs:` both legs with default `success()`
     semantics means any red leg blocks all commits, including for the un-ticked artifact; both
     loop-breaker markers (`[GoBDKUpdate]`, `[WasmBDKUpdate]`) and why they are read from git rather
     than from `github.event.head_commit.message`; that per-family "did it change" is decided by the
     staged diff and **not** by the gobdk matrix's last-writer-wins output, with a one-line note on
     why (§5.8); what the `gobdk-status-*` marker artifacts are for — telling "no archive changed"
     apart from "the download failed", which is why no step in the job uses `continue-on-error` and
     why the marker count is pinned to the matrix length; and that there is exactly one push, at the
     end, to avoid a same-branch race.
5. **`module/typesbdk/examples/backend/index.mjs`** — **mandatory, not optional** (commit C9). Stale:
   it calls the embind-style API
   (`new bdk.VectorUInt8()`, `push_back`, then `bdk.VerifyScript(vector, …)`), which no longer
   matches the shipped compact C ABI plus glue loader: `txvalidator_wasm.js:154-158` defines
   `VerifyScript` as the typed-array entry point (aliased to `VerifyScriptArray`), and the
   `VectorUInt8`/`VectorInt32`/`VectorUInt32` shims exist only in the compatibility layer
   (`txvalidator_wasm_compat.js:151` onwards), which the slim build does not include. The **minimum
   acceptable outcome** is a header comment marking the file as not matching the current ABI and
   pointing at the `VerifyScriptArray` typed-array path; the preferred outcome is a rewrite against
   that API. Either way the file must not be left as-is — O2 below chooses the depth, not whether to
   do it. Also note it imports `'../../wasm/bdk-core.mjs'`, which stays valid — the artifacts do not
   move.
6. **`module/typesbdk/examples/README.md`** — several stale statements to fix:
   - `:89` benchmark path → `node test/types/benchmark.mjs 5000 11`;
   - `:19-23` "`build.sh` … runs libsecp256k1's verified, non-verified, and exhaustive WASM test
     binaries, and runs real … transaction vectors" → `build.sh` builds only; ctest validates; and
     the exhaustive suite is not registered (only `tests` and `noverify_tests`, see
     `module/typesbdk/wasm/CMakeLists.txt:481`);
   - `:38-39` `BDK_WASM_RUN_SECP_TESTS=0` — this variable is already dead today; delete the
     sentence;
   - `:51-55` `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS=1` → the CMake publish target and the
     `commit-wasm-artifacts` dispatch checkbox on `build_bdk.yaml`;
   - consider relocating the build/ABI sections into the new `module/typesbdk/wasm/README.md` and
     leaving `examples/README.md` about the examples; if so, keep a link so no content is lost.
7. **`documentation/docs/build.md`**:
   - `:210-215` — rewrite the "plain `build.sh` builds and validates … Regenerating the committed
     files is an explicit opt-in — `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS=1` …" paragraph;
   - add the ctest invocation and the `bdk_wasm_install_insource` target;
   - `:152` option table — the `BDK_BUILD_WASM` row should note that it also adds `test/types`.
8. **`documentation/docs/architecture.md`**:
   - `:135-147` — replace the `commit-static-gobdk` five-part gate description with the unified
     `commit-built-artifacts` job: two checkboxes, both legs required green, per-family
     modified detection, two markers, one push;
   - `:165-174` — `build.sh` is no longer "configure/build/validate", and the claim that CI
     "fails if the tracked bytes drift" is no longer true; state the new policy (refreshed on
     demand, drift accepted) and point at `test/types`.
9. **`documentation/docs/directories.md`** `:16-18` — the `test/` listing shows only `core` and
   `golang`; add `rust` (already missing today) and `types`.
10. **`ReleaseNote.md`** — add a "Recent changes" entry covering the user-visible breaks: the
    typesbdk build/test split (`test/types`); `build.sh` reduced to build-only with
    `BDK_WASM_RUN_TESTS`/`BDK_WASM_UPDATE_COMMITTED_ARTIFACTS` removed and `node`/`ctest` no longer
    preflighted; the artifact reproducibility gate dropped with no replacement; `build_wasm.yaml` no
    longer manually dispatchable (use `build_bdk.yaml`); and `commit-built-binaries` replaced by
    `commit-gobdk-archives` + `commit-wasm-artifacts`.
11. **Final sweep.** After the moves, re-run the sweep below and **triage every hit** — the sweep is
    not a "must return empty" check, because a number of mentions are deliberate and must survive.
    Use `git grep`, which searches tracked files only and therefore already excludes the build trees
    (`build-wasm/`, `build/`) and `test/golang/vendor` (untracked in this repository):

    ```console
    git grep -n -E 'test\.mjs|test-umd\.mjs|test-suite\.mjs|test-first-call\.mjs|test-verify-parity\.mjs|verify-corpus-gen\.mjs|benchmark\.mjs|vectors\.json|BDK_WASM_RUN_TESTS|BDK_WASM_UPDATE_COMMITTED_ARTIFACTS|BDK_WASM_RUN_SECP_TESTS|commit-built-binaries|commit-static-gobdk' \
      -- ':(exclude)WASM_refactor_structure.md' ':(exclude)ReleaseNote.md'
    ```

    Classify each remaining hit into exactly one bucket:

    - **(a) Correct new-location reference** — keep. Expected in `test/types/CMakeLists.txt`,
      `test/types/README.md`, `test/types/package.json`, the moved scripts' own cross-imports
      (`test.mjs` → `test-suite.mjs`, `benchmark.mjs` → `vectors.json`), and the updated
      `module/typesbdk/examples/README.md` / `documentation/docs/*` paths.
    - **(b) Intentional historical or announcement mention** — keep. Namely: the two paths excluded
      in the command above (`WASM_refactor_structure.md`, which is this plan and describes the old
      layout by design, and `ReleaseNote.md`, which is *required* by item 10 to name the removed
      inputs), plus `module/typesbdk/wasm/build.sh`'s usage header, which names
      `BDK_WASM_RUN_TESTS` / `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS` precisely to say they no longer
      exist. Any other bucket-(b) hit must be justified in the commit message, not merely tolerated.
    - **(c) Stale operational reference** — fix. A path that no longer resolves, a variable an
      instruction still tells the reader to set, or CI/CMake/shell code that reads a removed name.

    The bar is: **bucket (c) is empty**, and every bucket-(b) hit is one of the three named above.

    Run one more targeted sweep for the trigger change, same triage:

    ```console
    git grep -n -E 'build_wasm\.yaml' -- ':(exclude)WASM_refactor_structure.md'
    ```

    Anything that describes or instructs a *manual* run of `build_wasm.yaml` is bucket (c) now that
    its `workflow_dispatch` trigger is gone (§5.7); references to it as an automatic or called
    workflow are bucket (a).

---

## 8. Acceptance criteria

A green implementation satisfies all of the following.

**Layout**

1. `git ls-files module/typesbdk/wasm | wc -l` → **25**, being exactly: `CMakeLists.txt`,
   `build.sh`, `README.md`, the **14** build inputs, and the **8** committed artifacts. No `*.mjs`
   test script, no `vectors.json`. The 14 inputs are `txvalidator_wasm.cpp`, `txvalidator_wasm.h`,
   `txvalidator_wasm.js`, `txvalidator_wasm_compat.js`, `txvalidator_wasm_snapshot.js`,
   `txvalidator_wasm_minimal_extern.js`, `txvalidator_wasm_minimal_pre.js`, `wasm_tests.c`,
   `wasm_tests_patch.cmake`, `secp256k1_runtime.c`, `secp256k1_runtime_precomputed.c`,
   `secp256k1_runtime_precomputed.h`, `big_int_boost.cpp`, `memory_cleanse_wasm.cpp` — enumerated
   rather than counted in prose, since a prose count is what got this wrong once already. If the
   layout documentation is written as a `CMakeLists.txt` header comment instead of a README (§7.1),
   the expected total is 24.
2. `git ls-files test/types` lists `CMakeLists.txt`, `README.md`, the 7 moved `.mjs` scripts,
   `vectors.json`, and `package.json` if C4 landed.
3. The 8 committed artifacts are at their original paths, and `git log --follow` shows them
   untouched by the refactor commits.
4. `git status --porcelain` is clean after each Phase A commit (no stray generated files).

**CMake**

5. No *executable* test registration remains in the module directory. The kept comments legitimately
   discuss tests, so match the command call, not the word:

   ```console
   grep -c -E '^[[:space:]]*(add_test|set_tests_properties)[[:space:]]*\(' \
     module/typesbdk/wasm/CMakeLists.txt                                   # → 0
   ```
6. A standalone configure succeeds:
   `emcmake cmake -S . -B <dir> -DBDK_BUILD_CORE=OFF -DBDK_BUILD_WASM=ON …` with no warnings other
   than the pre-existing ones.
7. `ctest -N` in the WASM build directory lists exactly the twelve tests named in §5.2, with the
   same labels, timeouts and fixture wiring as before the refactor. A before/after diff of
   `ctest -N` output and of `ctest --print-labels` is empty.
8. The generated `CTestTestfile.cmake` at the top of the WASM build tree contains
   `subdirs("test/types")` and no test registrations under
   `module/typesbdk/wasm/CTestTestfile.cmake`.
9. The native build is unaffected: a native configure/build/`ctest -N` produces the same test list
   as before, and `test/CMakeLists.txt` is unmodified in the diff.
10. `cmake --build <dir>` (default `ALL`) still builds the four bindings and their optimize edges
    and does **not** build the two `wasm_secp256k1_*` executables or write anything into the source
    tree.

**Build and test**

11. `module/typesbdk/wasm/build.sh` completes, prints `Built:` and lists the eight `dist/` files;
    `git status --porcelain` afterwards shows no modification under `module/typesbdk/wasm/`.
12. `( cd build-wasm && ctest --output-on-failure )` — all twelve tests pass, including
    `wasm_size_gate` at the unchanged 300000-byte ceiling.
13. `build.sh` contains no *executable* reference to the removed variables — comments may still name
    them, and the usage header deliberately does (§7.11 bucket (b)), so strip comments before
    counting:

    ```console
    sed 's/#.*//' module/typesbdk/wasm/build.sh \
      | grep -c -E 'BDK_WASM_RUN_TESTS|BDK_WASM_UPDATE_COMMITTED_ARTIFACTS'      # → 0
    grep -c -E '^for command in cmake emcmake emcc em\+\+ emar emranlib make; do' \
      module/typesbdk/wasm/build.sh                                              # → 1
    ```

    The second check pins the preflight list exactly: no `node`, no `ctest`.
14. `cmake --build build-wasm --target bdk_wasm_install_insource` succeeds and is the only way the
    tracked artifacts change.
15. If C4 landed: from a checkout with **no** build tree and **no** Emscripten,
    `( cd test/types && npm test )` passes against the committed artifacts, and
    `npm run test:parity` passes; `npm run bench` runs.
16. `node test/types/benchmark.mjs 5000 11` runs from the repository root and from `test/types`.

**CI**

17. `build_wasm.yaml` on a pull request touching `test/types/**` triggers, builds, runs ctest, and
    reports green/red only — no artifact commit, and neither a git-diff gate nor any drift-report
    step is present in the workflow.
18. `build_wasm.yaml` has exactly three triggers — `pull_request`, `push`, `workflow_call` — and
    **no `workflow_dispatch`**: it does not appear in the Actions UI "Run workflow" list. Its
    `workflow_call` declares `bitcoin-sv-commit` and `publish-artifacts` inputs and a `modified`
    output. `build_bdk.yaml` is the only manually dispatchable workflow that builds wasm.
19. `build_bdk.yaml` dispatch with **both** boxes ticked, on a branch whose artifacts changed:
    both legs run, one job commits two commits (`[GoBDKUpdate]`, `[WasmBDKUpdate]`) and pushes
    once.
20. Dispatch with **go only**: only the gobdk commit appears; the wasm leg still builds and tests,
    and the tracked wasm artifacts are untouched.
21. Dispatch with **wasm only**: only the wasm commit appears; the gobdk archives are untouched.
22. Dispatch with **neither**: both legs build and test, the commit job does not run, nothing is
    committed, nothing is pushed.
23. A **red** wasm leg with the gobdk box ticked commits **nothing** (and vice versa).
24. A dispatch on a branch whose head commit subject contains `[WasmBDKUpdate]` does not produce a
    further wasm commit; same for `[GoBDKUpdate]`. Verified with the marker present in a real head
    commit, not only by reading the expression.
25. **Matrix-aggregation regression:** a dispatch where exactly one gobdk matrix leg produced a
    changed archive commits that archive, regardless of which leg finished last. Reproduce by
    inspecting a run where `build-bdk.outputs.modified == 'false'` while a `libGoBDK_*` artifact was
    uploaded — the commit must still happen. Conversely, a dispatch where no archive changed reaches
    the commit job, reads three `modified=false` markers, skips the archive download, makes no gobdk
    commit, and stays green.
26. **Download failures are hard.** No *executable* `continue-on-error` key anywhere in the workflow —
    the §7.4 comments deliberately mention the phrase to explain why it is absent, so the check must
    match the YAML key, not the word:

    ```console
    grep -c -E '^[[:space:]]*continue-on-error:' .github/workflows/build_bdk.yaml    # → 0
    ```

    Three status markers are uploaded on every dispatch (one per Unix matrix leg) regardless of
    whether that leg's archive changed. A run in which a leg reports `modified=true` but its
    `libGoBDK_<os_arch>.a` does not arrive **fails** the commit job rather than committing nothing;
    verify by inspecting the assertion, and if a real occurrence is available, by the red run.
    Fewer than three markers also fails.
27. **Artifact download layout.** Both `pattern:`-based gobdk downloads set `merge-multiple: true`:

    ```console
    grep -c -E '^[[:space:]]*merge-multiple:[[:space:]]*true' .github/workflows/build_bdk.yaml  # → 2
    ```

    The `bdk-wasm-artifacts` download uses `name:` and must **not** set it. A dispatch where a single
    leg's archive changed completes the move without a "did not arrive" failure — the case that
    breaks if the flag is missing, because the per-artifact subdirectory is named identically to the
    file it contains.
28. Both workflow files pass a YAML/Actions syntax check.

**Documentation**

29. `module/typesbdk/wasm/README.md` exists (or, in the alternative form, `CMakeLists.txt` opens with
    the layout header block) and states: build inputs + committed artifacts only, tests are in
    `test/types/`, artifacts are refreshed on demand and may be stale.
30. `module/typesbdk/examples/backend/index.mjs` either runs against the committed
    `bdk-core.mjs` or carries a header comment stating it does not match the shipped ABI and naming
    `VerifyScriptArray` as the current entry point. An unchanged file fails this criterion.
31. Every document reference added by §7 resolves to a **tracked** file:
    `git ls-files --error-unmatch <path>` succeeds for each one. In particular
    `test/types/README.md` must not cite an untracked working note — the tracked rationale is
    `test/types/test-first-call.mjs`'s header, `module/typesbdk/wasm/wasm_tests.c`'s header, and
    `module/typesbdk/wasm/CMakeLists.txt:97-121` (§7.2).
32. Every remaining item in §7 is done, and the §7.11 sweep has been **triaged**, not merely run:
    bucket (c) (stale operational references) is empty, and every surviving hit is either a correct
    new-location reference or one of the three named intentional mentions
    (`WASM_refactor_structure.md`, `ReleaseNote.md`, the `build.sh` usage header). "The grep returns
    no hits" is **not** the criterion and is not achievable — the plan file and the release note are
    both required to name the old paths and removed variables.

---

## 9. Risks and open points

| # | Item | Severity | Handling |
|---|------|----------|----------|
| R1 | `benchmark.mjs` hard-codes `import './bdk-core.mjs'`; moving it silently breaks the benchmark | high | fixed in C2 (§5.4a); acceptance criterion 16 |
| R2 | The five default `process.argv[2]` fallbacks resolve relative to the script and would point at a non-existent file after the move | medium | fixed in C2 (§5.4b); acceptance criterion 15 |
| R3 | `BDK_WASM_DIST_DIR` is a sibling-scope variable, not inherited by `test/types` | medium | `PARENT_SCOPE` publication plus a `FATAL_ERROR` assertion in `test/types` (§5.1, §5.2) |
| R4 | Dropping the reproducibility gate removes the only automatic detector of artifact/source drift | medium — accepted by design | documented as policy (§4.3, §5.7). No replacement gate **and no drift report**: a publish-and-restore report would keep PR/push runs writing tracked files for a diff nobody acts on |
| R5 | `github.event.head_commit.message` is empty on `workflow_dispatch`, so today's loop-breaker is inert on the only trigger that reaches the commit job | medium | read the marker from git in the job (§5.8 step 2) |
| R6 | `BRANCH_NAME=${{ github.head_ref }}` is empty on `workflow_dispatch` | low — works today by falling back to the checked-out branch | switch to `github.ref_name` as a separate hunk (§5.8 step 6) |
| R7 | Unfiltered `download-artifact` would mix `bdk-wasm` (dist) and `bdk-wasm-artifacts` (tracked copies), which share file names | medium | explicit `name`/`pattern` + separate paths per family (§5.8 steps 3-4) |
| R8 | Renaming `commit-built-binaries` breaks saved dispatch forms and any external caller | low | release-note entry (§7.10) |
| R9 | Removing `BDK_WASM_RUN_TESTS` / `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS` silently no-ops anything that sets them | low | documented in the `build.sh` header, `build.md`, `examples/README.md`, release note |
| R10 | With `node` dropped from `build.sh`'s preflight, a node-less environment registers only 2 of the 12 tests and both pass — a green run that validated almost nothing | medium | the CI test step asserts the ten node-dependent tests **by name** rather than counting (§5.7 step 5); `test/types` keeps its `message(WARNING)` branches; `test/types/README.md` states the configure-time node requirement (§5.6) |
| R11 | `build-bdk` is a matrix and all three legs write one job-level `outputs.modified`; last-writer-wins silently drops a changed, uploaded archive whenever the platforms disagree | **high — pre-existing, now fixed** | the commit job decides per family with a staged `git diff --cached --quiet`; the job `if` no longer reads `needs.build-bdk.outputs.modified` (§5.8); acceptance criterion 25 |
| R12 | The gobdk download step can find no artifacts at all now that the job is no longer gated on `modified` — and "nothing was uploaded" is indistinguishable from "the download failed", so a real artifact-service failure would produce a green run that silently drops a refresh | **medium** | per-leg status markers, always uploaded, downloaded with no failure tolerance: the job derives the expected archive set, skips the archive download when the set is empty, and fails hard when an expected archive does not arrive. `continue-on-error` is **not** used anywhere in this job (§5.8); acceptance criteria 25-26 |
| R13 | Separating ctest from the build may push `build_wasm` past `timeout-minutes: 45` | low | raise to 60 (§5.7) |
| R14 | Removing `build_wasm.yaml`'s `workflow_dispatch` removes the ability to run wasm alone from the UI | low | intentional — `build_bdk.yaml` is the single manual entry; a dispatch with both boxes unticked builds and tests both legs and commits nothing (§5.7) |
| R15 | The secp256k1 verification-table substitution remains unverified against upstream's static tables (`module/typesbdk/wasm/CMakeLists.txt:97-121`) | out of scope | untouched by this refactor; the parity suites move as-is and keep their current coverage |

Open points for the maintainer to confirm before or during implementation. All are genuine choices;
none of them is a choice about *whether* to do the work:

- **O1** — Keep the toolchain-free node runner (C4), or leave ctest as the only route? This is the
  only optional commit in the set.
- **O2** — `examples/backend/index.mjs`: full rewrite against the typed-array API, or the minimum
  stale-ABI header comment (§7.5)? The file must change either way.
- **O3** — Move the build/ABI prose out of `examples/README.md` into the new
  `module/typesbdk/wasm/README.md`, or fix it in place (§7.6)?
- **O4** — Layout documentation form: `module/typesbdk/wasm/README.md` (preferred) or a header
  comment block in `module/typesbdk/wasm/CMakeLists.txt` (§7.1)? Either satisfies the requirement
  that it live in `module/typesbdk/wasm/`; it changes the expected file count in criterion 1.
