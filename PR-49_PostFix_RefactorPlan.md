# PR #49 post-merge refactor plan — standalone WASM module build

**Base PR:** https://github.com/bitcoin-sv/bdk/pull/49 — "Build and optimize the BDK WASM
verifier", head `96c79dfc678890e20a59e0b2ca214e0136788c48`,
**assumed accepted and merged as-is.** This document is the full refactor plan for a follow-up PR
**stacked on top of it** that fixes the build architecture. This checkout *is* the PR-49 head, so
every file/line reference below was verified against the merged state.

**Relationship to `PR-49_RefactorPlan.md` (the prior analysis):** this plan **supersedes its
ask 1** (root profile-safety guards) with a stronger structural inversion — the standalone-module
build removes the shared-core-mutation hazard entirely instead of guarding it — and **folds in
ask 0** (the native `test_big_int_boost` regression, top priority here), **ask 5** (Boost version
enforcement on the direct wasm configure path) and **ask 6** (no toolchain identifiers in shared
cmake — the `EMSCRIPTEN` branch leaves `FindBoostHelper.cmake` altogether). Asks 2–4
(`VerifySpend` native coverage, `consensusParameters` table hardening, the reverse-dependency
comment) are functional/test asks orthogonal to the build architecture; ask 4's comment is folded
into commit 5 below, asks 2–3 remain tracked separately and are unaffected by this refactor.
Where the prior analysis is still valid (solution-A/-B trade-offs, the physical OpenSSL
constraint on `big_int.cpp`/`random.cpp`), this plan stays consistent with it.

This is a local working document: local paths are used freely.

---

## 1. Premise and motivation

PR-49 delivers a WASM verifier whose size optimization is a legitimate product need: the Node
loader+WASM pair went from ~3 MB to ~256 KB (no OpenSSL, runtime-reconstructed secp256k1 tables,
no Embind), which is what makes browser/SDK deployment viable. That optimization is kept intact.

What must change is *how* it is wired into the build. The PR threads module-specific mechanisms
through shared build code:

- a global core "profile": `cmake/BDKCoreConfig.cmake` declares five extension-point variables
  (`BDK_CORE_FIND_OPENSSL`, `BDK_CORE_LINK_LIBRARIES`, `BDK_CORE_EXCLUDED_BSV_SOURCES`,
  `BDK_CORE_ADDITIONAL_SOURCES`, `BDK_CORE_PRIVATE_COMPILE_DEFINITIONS`) plus an overridable hook
  function `bdk_configure_secp256k1_targets()`;
- the root `CMakeLists.txt` (lines 71–78) includes a module-owned override file
  (`module/typesbdk/wasm/core-overrides.cmake` or
  `module/typesbdk/benchmark/core-overrides.cmake`) **before** `add_subdirectory(core)`, so the
  module's profile mutates the one shared `bdk_core` target;
- `cmake/BDKInit.cmake:146` gates OpenSSL discovery on `BDK_CORE_FIND_OPENSSL`;
- `cmake/modules/FindBoostHelper.cmake:149-162` carries an `EMSCRIPTEN` branch that fabricates a
  header-only `Boost::boost` target;
- `core/setting-secp256k1.cmake:31` calls the overridable hook so a module can rewrite the
  secp256k1 target's sources;
- `core/CMakeLists.txt:61-62,79,96` splices the profile variables into the shared core's source
  list, link line and compile definitions.

The shared `bdk_core` is therefore **mutated by a module's profile** rather than left canonical.

**Observed regression (empirically verified):** a plain native build with `-DBDK_BUILD_WASM=OFF`
fails at compile time with
`fatal error: boost/multiprecision/cpp_int.hpp: No such file or directory`. Cause: the new
unconditional `test_big_int_boost` target (`test/core/CMakeLists.txt:147-155`) compiles
`module/typesbdk/wasm/big_int_boost.cpp` natively against `Boost::boost`, and the project's
OFFICIAL prebuilt dependency package (`.github/workflows/prebuild_dependancies.yaml:57`,
`BOOST_INCLUDE_LIBRARIES="chrono;filesystem;program_options;system;test;thread;circular_buffer;multi_index;property_tree;signals2;uuid;variant"`)
does not install Boost multiprecision headers. A WASM-only PR changed what a native build
requires; the default native build is broken on the project's own documented dependency set.

**Architecture rule this refactor enforces:** core is the upstream-tracking trunk; modules depend
on core, extend and link it, but must **never modify the core build**. Aggressive wasm
optimization stays — but as a fully **standalone module build** that assembles its **own** core
variant from a shared, reusable core *recipe*, leaving the canonical native `bdk_core` untouched
by anyone.

---

## 2. Target architecture

### 2.1 The fundamental idea

1. **Split `core/CMakeLists.txt` into two parts:**
   - **(a) a reusable recipe** — a new `core/bdk-core-recipe.cmake` that owns everything needed
     to turn the curated bitcoin-sv lists (`BSV_MINIMAL_SRC_FILES` / `BSV_MINIMAL_HDR_FILES`,
     populated by `cmake/modules/FindBSVSourceHelper.cmake`) plus the `core/` extras into a core
     library target: `bitcoin-config.h` generation, the secp256k1 sub-build, univalue, include
     dirs, Boost definitions, warning workarounds — exposed through one parameterized factory
     function **`bdk_add_core_library(<target> ...)`**;
   - **(b) a thin native part** — `core/CMakeLists.txt` shrinks to: find Boost, include the
     recipe, call `bdk_add_core_library(bdk_core LINK_LIBRARIES OpenSSL::Crypto OpenSSL::SSL)`,
     then the existing install/export blocks.
2. **WASM becomes a fully standalone build:**
   - new root flag **`BDK_BUILD_CORE`** (default `ON`) can deactivate the shared core build
     entirely;
   - **`BDK_BUILD_WASM`** (default `OFF`) activates the typesbdk wasm module (executed as
     `BDK_BUILD_TYPES`, renamed by Owner Addendum 6 — §2.2); it replaces the PR-49 overlay flag
     of the same name, whose reads were removed completely in commit 3 (§5.7);
   - the wasm invocation is `-DBDK_BUILD_CORE=OFF -DBDK_BUILD_WASM=ON`;
   - `module/typesbdk/wasm/CMakeLists.txt` includes the recipe, takes the ready lists, adjusts
     them with plain `list(REMOVE_ITEM ...)` / `list(APPEND ...)` (drop `big_int.cpp`,
     `random.cpp`, `support/cleanse.cpp`; add `big_int_boost.cpp`, `memory_cleanse_wasm.cpp`),
     builds **its own core variant `bdk_core_wasm`** via the factory, performs its secp256k1
     source surgery on the sub-build it now instantiates itself, and links the four binding
     targets against `bdk_core_wasm` — all under the Emscripten toolchain, all inside the module
     directory;
   - the wasm-specific sources (`big_int_boost.cpp`, `memory_cleanse_wasm.cpp`,
     `secp256k1_runtime.c`, `secp256k1_runtime_precomputed.c/.h`) **stay in
     `module/typesbdk/wasm/`** exactly where they are today.
3. **Net effect — every PR-49 out-of-module mechanism is removed:**
   - `cmake/BDKCoreConfig.cmake` — **deleted**;
   - root override-includes (`CMakeLists.txt:71-78`) — **removed**;
   - the five `BDK_CORE_*` extension-point variables in `core/CMakeLists.txt:61-62,79,96` —
     **removed** (the recipe factory's explicit parameters replace them);
   - `cmake/BDKInit.cmake:146-150` OpenSSL gate — **reworked**: discovery stays at the root (see
     §5.5 for why it cannot move into `core/`) but is gated on the native core build
     (`if(BDK_BUILD_CORE)`), never on a core-profile variable;
   - `core/setting-secp256k1.cmake:31` hook call and the whole
     `bdk_configure_secp256k1_targets()` mechanism — **removed** (the module configures its own
     secp256k1 instantiation in its own directory);
   - `cmake/modules/FindBoostHelper.cmake:149-162` `EMSCRIPTEN` branch — **removed** (reverted to
     the pre-PR helper); the wasm module creates its header-only `Boost::boost` itself, now with
     the pinned-version assertion the PR lacked;
   - `module/typesbdk/wasm/core-overrides.cmake` — **deleted**, its content relocated into the
     wasm module's `CMakeLists.txt`;
   - the benchmark leaves the typesbdk tree entirely: `module/typesbdk/benchmark/` (its
     `CMakeLists.txt`, `core-overrides.cmake` and the `BDK_BUILD_NATIVE_VERIFY_BENCHMARK` flag)
     is **deleted**, and `verify_script_benchmark.cpp` moves into the existing native C++
     examples module `module/example/` as an ordinary `bench_*` executable linking the canonical
     `bdk_core` (§3.6, §5.9). The residual hazard the prior analysis flagged as ask 1 —
     `DISABLE_LOGGING` leaking into the shared core — disappears structurally: nothing declares
     core deviations anymore.

   The shared `bdk_core` is never mutated by anyone. The recipe factory has exactly **two call
   sites** — the thin native `core/CMakeLists.txt` (canonical `bdk_core`) and the standalone wasm
   module (`bdk_core_wasm`) — i.e. exactly **one specialized consumer**, and there is one core
   build recipe and zero drift surface.

### 2.2 Flag semantics after the refactor

| Flag | Default | Meaning |
|---|---|---|
| `BDK_BUILD_CORE` | `ON` | Build the canonical native `bdk_core` (and everything that links it: modules, tests, docs). `OFF` skips core and force-disables all shared-core consumers. |
| `BDK_BUILD_WASM` (né `BDK_BUILD_TYPES`) | `OFF` | Build the typesbdk WASM module as a standalone build. Requires Emscripten and `BDK_BUILD_CORE=OFF`. **The single module flag.** Executed as `BDK_BUILD_TYPES`, then **renamed by Owner Addendum 6** ("TYPES" collides with CMake build-type terminology); the standalone semantics and guards are unchanged under the new name. |
| `BDK_BUILD_WASM` (original PR-49 meaning) | *(removed in commit 3; name reused by Owner Addendum 6)* | The PR-49 overlay flag was deleted with no reads remaining (§5.7). Owner Addendum 6 later reused the name for the renamed standalone flag (row above) — so the name is live again, now with the standalone semantics: `=ON` without Emscripten is a configure error, `=OFF` matches the default. |
| `BDK_REQUIRE_BIGINT_PARITY` | *(removed — Owner Addendum 5)* | Was: CI enforcement of the parity suite. Superseded: multiprecision is a required native dependency, the probe is a two-way hard gate (`FATAL_ERROR` when absent), and the option plus the CI shim were removed. |
| `BDK_BUILD_CORE_ONLY` | `OFF` | Unchanged meaning (build only core). Contradictory with `BDK_BUILD_CORE=OFF` → `FATAL_ERROR`. |

(`BDK_BUILD_NATIVE_VERIFY_BENCHMARK` is deleted along with `module/typesbdk/benchmark/`; the
VerifyScript benchmark becomes a regular executable in `module/example/`, built by the default
native modules build — §3.6, §5.9. No flag involved.)

Canonical invocations:

```bash
# Native (unchanged; builds core, modules incl. module/example benchmarks, tests).
# The historical -DBDK_BUILD_WASM=OFF is tolerated: the variable is simply never
# read and CMake prints its unused-variable notice.
cmake ../bdk -DCMAKE_BUILD_TYPE=Release && make -j8 && make test

# WASM standalone (via module/typesbdk/wasm/build.sh, or directly:)
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release \
  -DBDK_BUILD_CORE=OFF -DBDK_BUILD_WASM=ON \
  -DBSV_ROOT=... -DBOOST_ROOT=... \
  -DSECP256K1_ASM=OFF -DSECP256K1_ECMULT_WINDOW_SIZE=15 -DSECP256K1_ECMULT_GEN_KB=2 \
  -DSECP256K1_TEST_OVERRIDE_WIDE_MULTIPLY=int64 -DSECP256K1_BUILD_BENCHMARK=OFF
```

---

## 3. Concrete CMake sketches

These are working sketches grounded in the current files; the implementation may polish wording
but must keep the structure, names and parameter sets exactly as specified.

### 3.1 `core/bdk-core-recipe.cmake` (new file)

```cmake
## Reusable BDK core build recipe.
## Included by exactly two call sites, one per build tree: core/CMakeLists.txt
## (canonical native bdk_core) or module/typesbdk/wasm/CMakeLists.txt (the
## standalone bdk_core_wasm variant). Prerequisites at include time:
##   - HelpFindBSVSource() has run (BDK_BSV_ROOT_DIR, BSV_MINIMAL_*_FILES, BSV_INCLUDE_DIRS)
##   - bdkInitCMake() has run (BDK_GENERATED_*_DIR, build settings)
##   - a Boost::boost target exists
## The includer's CMAKE_CURRENT_BINARY_DIR hosts the secp256k1 sub-build.
## NO include guard: sibling directory scopes cannot see each other's variables,
## so a guard variable would be useless; the side-effecting setting-*.cmake files
## carry defensive TARGET guards instead.

set(BDK_CORE_RECIPE_DIR "${CMAKE_CURRENT_LIST_DIR}")   # == <repo>/core wherever included from

## bitcoin-config.h generation (location-independent: uses BDK_BSV_ROOT_DIR and
## BDK_GENERATED_HPP_DIR only)
include("${BDK_CORE_RECIPE_DIR}/setting-bitcoin.cmake")

## secp256k1 sub-build: instantiated once per build tree (TARGET-guarded, §5.2)
include("${BDK_CORE_RECIPE_DIR}/setting-secp256k1.cmake")

if(BDK_BUILD_UNIVALUE)
  include("${BDK_CORE_RECIPE_DIR}/setting-univalue.cmake")   # TARGET-guarded, §5.1
  set(UNIVALUE_LIB univalue)
else()
  set(UNIVALUE_LIB "")
endif()
## NOTE: setting-leveldb.cmake is deliberately NOT part of the recipe. No core
## variant links leveldb (bdk_core's link line is secp256k1/univalue/Boost only),
## so it stays a side build owned by the thin native core/CMakeLists.txt — §5.10.

## Factory: build one core library variant from the curated recipe.
## The four parameters replace four of the five removed profile variables 1:1;
## the fifth (OpenSSL discovery) is deliberately a caller concern (§5.3, §5.5).
## Do not add parameters.
function(bdk_add_core_library target)
  cmake_parse_arguments(ARG "" ""
    "EXCLUDE_BSV_SOURCES;ADDITIONAL_SOURCES;LINK_LIBRARIES;COMPILE_DEFINITIONS" ${ARGN})
  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "bdk_add_core_library: unknown arguments [${ARG_UNPARSED_ARGUMENTS}]")
  endif()

  ## Generated version TU (same output path for every variant; identical content)
  set(BDK_VERSION_CPP_IN "${BDK_CORE_RECIPE_DIR}/BDKVersion.cpp.in")
  set(BDK_VERSION_CPP "${BDK_GENERATED_CPP_DIR}/BDKVersion.cpp")
  configure_file(${BDK_VERSION_CPP_IN} ${BDK_VERSION_CPP})

  ## Local copies of the global curated lists; the caller's exclusions apply here only
  set(_bsv_src_files ${BSV_MINIMAL_SRC_FILES})
  set(_bsv_hdr_files ${BSV_MINIMAL_HDR_FILES})
  if(ARG_EXCLUDE_BSV_SOURCES)
    list(REMOVE_ITEM _bsv_src_files ${ARG_EXCLUDE_BSV_SOURCES})
  endif()

  ## core/ extras are pinned to the recipe dir, NOT to the caller's dir
  file(GLOB_RECURSE _core_extra_hdr_files
       "${BDK_CORE_RECIPE_DIR}/*.hpp" "${BDK_CORE_RECIPE_DIR}/*.h")
  file(GLOB_RECURSE _core_extra_src_files "${BDK_CORE_RECIPE_DIR}/*.cpp")
  list(APPEND _core_extra_src_files ${ARG_ADDITIONAL_SOURCES})

  ## (source_group + BDK_LOG_BSV_FILES logging loops move here, unchanged content)

  add_library(${target}
    ${BITCOIN_CONFIG_FILE} ${BDK_VERSION_CPP}
    ${_core_extra_hdr_files} ${_bsv_hdr_files}
    ${_core_extra_src_files} ${_bsv_src_files})
  target_include_directories(${target} PUBLIC ${BSV_INCLUDE_DIRS} "${BDK_CORE_RECIPE_DIR}")
  target_link_libraries(${target} PRIVATE
    secp256k1 ${UNIVALUE_LIB} Boost::boost ${ARG_LINK_LIBRARIES})
  target_compile_definitions(${target} PUBLIC HAVE_CONFIG_H)
  target_compile_definitions(${target} PRIVATE
    BOOST_ALL_NO_LIB BOOST_SP_USE_STD_ATOMIC BOOST_AC_USE_STD_ATOMIC
    ${ARG_COMPILE_DEFINITIONS})

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    ## source property is directory-scoped: applies where the variant is created
    set_source_files_properties("${BDK_BSV_ROOT_DIR}/src/script/sign.cpp"
      PROPERTIES COMPILE_FLAGS "-Wno-stringop-overread")
  endif()
  if(WIN32)
    target_link_libraries(${target} PRIVATE Crypt32.lib Ws2_32)
  endif()
  set_property(TARGET ${target} PROPERTY FOLDER "core")

  ## Export the extra-header list for the caller's install/unified-header blocks
  set(BDK_CORE_EXTRA_HDR_FILES ${_core_extra_hdr_files} PARENT_SCOPE)
endfunction()
```

### 3.2 `core/CMakeLists.txt` (thin native part)

```cmake
include(FindBoostHelper)
HelpFindBoost()
if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost is required to build bdk_core")
endif()
if(NOT DEFINED BDK_BSV_ROOT_DIR)
  message(FATAL_ERROR "Unable to locate bsv source code by BDK_BSV_ROOT_DIR")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/bdk-core-recipe.cmake")

## LevelDB is a native-only side build (no core variant links it) — it stays
## here in the thin native part, outside the reusable recipe (§5.10).
if(BDK_BUILD_LEVELDB)
  include("${CMAKE_CURRENT_SOURCE_DIR}/setting-leveldb.cmake")
endif()

## Canonical native core: OpenSSL-linked, no exclusions, no extra defs.
## (OpenSSL discovery ran at root — §5.5.)
bdk_add_core_library(bdk_core
  LINK_LIBRARIES OpenSSL::Crypto OpenSSL::SSL)

## Install/export blocks: keep the existing lines 101-152 verbatim
## (BDK_INSTALL_CORE_ARCHIVE target install; BDK_INSTALL_BSV_HEADERS loops over
##  BSV_MINIMAL_HDR_FILES [global cache] and BDK_CORE_EXTRA_HDR_FILES [now
##  received from the factory via PARENT_SCOPE]; bdk.in unified header).
```

### 3.3 Root `CMakeLists.txt` (changed region)

```cmake
option(BDK_BUILD_CORE "Build the canonical native bdk_core library" ON)
## Executed as BDK_BUILD_TYPES; renamed BDK_BUILD_WASM by Owner Addendum 6
## ("TYPES" collides with CMake build-type terminology) — same semantics.
option(BDK_BUILD_WASM
  "Build the TypeScript/JavaScript WASM module (standalone; requires Emscripten and BDK_BUILD_CORE=OFF)" OFF)
## [Superseded by Owner Addendum 5] The sketch originally added a third option
## here (BDK_REQUIRE_BIGINT_PARITY, CI enforcement of the parity suite). It was
## implemented, then removed again: multiprecision is a required native
## dependency and the §3.7 probe is a two-way hard gate.

## NOTE: the PR-49 overlay flag of the same name was removed in commit 3 with
## no reads remaining; Owner Addendum 6 later reused the BDK_BUILD_WASM name
## for this standalone flag (§5.7).

## Standalone-contract guards
if(BDK_BUILD_WASM)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "BDK_BUILD_WASM requires the Emscripten toolchain (use emcmake or build.sh)")
  endif()
  if(BDK_BUILD_CORE)
    message(FATAL_ERROR "BDK_BUILD_WASM is a standalone build: configure with -DBDK_BUILD_CORE=OFF")
  endif()
endif()
if(NOT BDK_BUILD_CORE)
  if(BDK_BUILD_CORE_ONLY)
    message(FATAL_ERROR "BDK_BUILD_CORE_ONLY=ON contradicts BDK_BUILD_CORE=OFF")
  endif()
  ## Everything that links the shared core is force-disabled, loudly.
  foreach(_opt BDK_BUILD_MODULES BDK_BUILD_CORE_TESTS
               BUILD_MODULE_GOLANG BUILD_MODULE_GOLANG_INSTALL_INSOURCE
               BUILD_MODULE_RUST BUILD_MODULE_RUST_INSTALL_INSOURCE)
    if(${_opt})
      message(STATUS "BDK_BUILD_CORE=OFF: forcing ${_opt}=OFF (it depends on the shared bdk_core)")
      set(${_opt} OFF CACHE BOOL "" FORCE)
    endif()
  endforeach()
endif()

## DELETED: include(cmake/BDKCoreConfig.cmake) and the conditional
## core-overrides.cmake includes (former lines 71-78).

include(cmake/BDKInit.cmake)
bdkInitCMake()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core)   # unchanged; headers used by all consumers

if(BDK_BUILD_CORE)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core)
endif()

if(BDK_BUILD_WASM)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/module/typesbdk/wasm)
endif()

## DELETED: the BDK_BUILD_NATIVE_VERIFY_BENCHMARK option and its add_subdirectory
## block (former lines 42 and 105-110) — the benchmark is now an ordinary
## module/example executable built by the regular modules build (§3.6).

## The existing `if(NOT BDK_BUILD_CORE_ONLY)` block (module/, test/, documentation/)
## additionally requires BDK_BUILD_CORE (its options were force-disabled above, but
## the subdirectory adds must also be skipped):
if(NOT BDK_BUILD_CORE_ONLY AND BDK_BUILD_CORE)
  ...unchanged...
endif()
```

### 3.4 `cmake/BDKInit.cmake` (OpenSSL gate rework)

```cmake
  ## was: if(BDK_CORE_FIND_OPENSSL)
  ## OpenSSL is required by the native core and by the native modules/tests;
  ## imported targets are directory-scoped, so discovery must run at root scope
  ## to stay visible to core/, module/ and test/ alike (§5.5).
  if(BDK_BUILD_CORE)
    include(FindOpenSSLHelper)
    HelpFindOpenSSL()
  endif()
```

### 3.5 `module/typesbdk/wasm/CMakeLists.txt` (standalone rewrite — new head)

```cmake
if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "The BDK WASM target requires Emscripten")
endif()
if(BDK_BUILD_CORE)
  message(FATAL_ERROR "The WASM module is standalone: configure with -DBDK_BUILD_CORE=OFF")
endif()

## ---- Boost: header-only import, module-owned (was FindBoostHelper's EMSCRIPTEN
## branch; now also enforces the pinned version, closing the gate build.sh alone
## used to provide) ----
if(NOT TARGET Boost::boost)
  if(CUSTOM_BOOST_ROOT)
    set(BOOST_ROOT ${CUSTOM_BOOST_ROOT})
  elseif(DEFINED ENV{BOOST_ROOT})
    set(BOOST_ROOT $ENV{BOOST_ROOT})
  endif()
  find_path(BDK_BOOST_INCLUDE_DIR boost/version.hpp
    PATHS "${BOOST_ROOT}" "${BOOST_ROOT}/include"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
  if(NOT BDK_BOOST_INCLUDE_DIR)
    message(FATAL_ERROR
      "Unable to find boost/version.hpp below BOOST_ROOT=[${BOOST_ROOT}]. "
      "Point BOOST_ROOT at the prebuilt wasm Boost package (dependancies_wasm.tar.gz), "
      "or let build.sh provision the pinned minimal Boost install.")
  endif()
  file(STRINGS "${BDK_BOOST_INCLUDE_DIR}/boost/version.hpp" _bdk_boost_version_line
       REGEX "^#define BOOST_VERSION [0-9]+")
  string(REGEX MATCH "[0-9]+" _bdk_boost_version "${_bdk_boost_version_line}")
  if(NOT _bdk_boost_version STREQUAL "108500")
    message(FATAL_ERROR
      "The reproducible WASM build requires Boost 1.85.0 (BOOST_VERSION 108500); "
      "found [${_bdk_boost_version}] under [${BDK_BOOST_INCLUDE_DIR}]")
  endif()
  add_library(Boost::boost INTERFACE IMPORTED GLOBAL)
  set_target_properties(Boost::boost PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${BDK_BOOST_INCLUDE_DIR}")
  set(Boost_FOUND TRUE)
endif()

## ---- Reproducible __FILE__ paths (was in core-overrides.cmake at root scope;
## directory scope now covers every target this module creates: the core
## variant, the secp256k1 sub-build and the bindings — §5.8) ----
add_compile_options(
  "-ffile-prefix-map=${BDK_BSV_ROOT_DIR}=/bitcoin-sv"
  "-ffile-prefix-map=${BOOST_ROOT}=/boost"
  "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=/bdk")

## ---- The module's own core variant, from the shared recipe ----
include("${CMAKE_SOURCE_DIR}/core/bdk-core-recipe.cmake")
bdk_add_core_library(bdk_core_wasm
  EXCLUDE_BSV_SOURCES
    "${BDK_BSV_ROOT_DIR}/src/big_int.cpp"          # OpenSSL BIGNUM — physically unbuildable without OpenSSL
    "${BDK_BSV_ROOT_DIR}/src/random.cpp"           # OpenSSL RAND
    "${BDK_BSV_ROOT_DIR}/src/support/cleanse.cpp"  # replaced by wasm-safe cleanse
  ADDITIONAL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/big_int_boost.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/memory_cleanse_wasm.cpp"
  COMPILE_DEFINITIONS BOOST_HAS_PTHREADS DISABLE_LOGGING)

## ---- secp256k1 wasm specialization (was the bdk_configure_secp256k1_targets
## override). The recipe instantiated secp256k1 under THIS directory's binary
## dir, so the module rewires targets it owns: ----
set_property(TARGET secp256k1_precomputed PROPERTY SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/secp256k1_runtime_precomputed.c"
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/src/precomputed_ecmult_gen.c")
target_include_directories(secp256k1_precomputed PRIVATE
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/src")
target_compile_options(secp256k1_precomputed PRIVATE -O3)
set_property(TARGET secp256k1_precomputed PROPERTY INTERPROCEDURAL_OPTIMIZATION OFF)
target_include_directories(secp256k1 PRIVATE
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/include"
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/src")
target_compile_options(secp256k1 PRIVATE -O3)
get_target_property(_secp256k1_sources secp256k1 SOURCES)
list(REMOVE_ITEM _secp256k1_sources "secp256k1.c")
list(PREPEND _secp256k1_sources "${CMAKE_CURRENT_SOURCE_DIR}/secp256k1_runtime.c")
set_property(TARGET secp256k1 PROPERTY SOURCES ${_secp256k1_sources})

## ---- Binding targets: the existing add_bdk_wasm_target() function and the four
## target instantiations stay byte-identical EXCEPT the link line:
##   target_link_libraries(${target_name} PRIVATE bdk_core_wasm Boost::boost)
```

### 3.6 VerifyScript benchmark → `module/example/` (module deleted, no flag)

`module/typesbdk/benchmark/` is **deleted entirely** (its `CMakeLists.txt`,
`core-overrides.cmake`, and the `BDK_BUILD_NATIVE_VERIFY_BENCHMARK` flag). A native C++
benchmark does not belong in the wasm module tree; it joins the existing native examples module
`module/example/`, which already hosts four `bench_*` executables
(`bench_validatetransaction`, `bench_validatetransaction_single`,
`bench_validatetransaction_batch`, `bench_num2bin_attack_validate`) built by the regular native
modules build (`module/CMakeLists.txt` adds `example` unconditionally under
`BDK_BUILD_MODULES`), so the executable lands in the normal build output and can be run anytime.

Changes:

- **Move (and rename)** `module/typesbdk/benchmark/verify_script_benchmark.cpp` →
  `module/example/bench_verifyscript.cpp`. Content unchanged; the rename follows the module's
  source-name = target-name convention (`bench_validatetransaction.cpp` →
  `bench_validatetransaction`, etc.) and matches the file name that
  `module/gobdk/cgobench/README.md:11` already anticipates — though that README line's
  *directory* part is wrong (it says `../../../example/…`, which from
  `module/gobdk/cgobench/` climbs three levels to the repo root; `module/example/` is only two
  levels up) and gets corrected to `../../example/bench_verifyscript.cpp` in the README task.
- **Boost usage requirements (load-bearing, do not simplify away):** the source includes
  `<txvalidator.hpp>` (on the global include path via the root `include_directories(core)`) and
  `<utilstrencodings.h>` (served by `bdk_core`'s PUBLIC `BSV_INCLUDE_DIRS`). But
  `core/txvalidator.hpp:14` includes `<taskcancellation.h>`, and bitcoin-sv's
  `src/taskcancellation.h:13` includes `<boost/chrono.hpp>` (and uses
  `boost::chrono::thread_clock`), while `bdk_core` links `Boost::boost` **PRIVATE** and
  publishes no Boost usage requirement. The target must therefore link `Boost::chrono` itself —
  its imported target carries the Boost include directories (compile side) and the compiled
  chrono library (link side). `module/example/CMakeLists.txt` already provides it: line 10 runs
  `HelpFindBoost(filesystem program_options thread chrono)`, and every sibling target links
  `Boost::chrono`.
- **Append to `module/example/CMakeLists.txt`** (after the last target, line 48) — minimal real
  needs: no `BSV_APPLICATION_*` sources, no univalue/OpenSSL links, but `Boost::chrono` is
  required as explained above:

```cmake
## VerifyScript micro-benchmark. Links the CANONICAL bdk_core (logging compiled
## in) — this measures the code path real native consumers (gobdk, rustbdk) run.
## Boost::chrono supplies the Boost headers+lib needed by txvalidator.hpp's
## taskcancellation.h include (bdk_core keeps Boost PRIVATE).
add_executable(bench_verifyscript "${CMAKE_CURRENT_SOURCE_DIR}/bench_verifyscript.cpp")
target_link_libraries(bench_verifyscript PRIVATE bdk_core Boost::chrono)
target_compile_features(bench_verifyscript PRIVATE cxx_std_20)
if(NOT MSVC)
  target_compile_options(bench_verifyscript PRIVATE -O3)
endif()
if(APPLE)
  target_link_options(bench_verifyscript PRIVATE "LINKER:-dead_strip")
elseif(UNIX)
  target_link_options(bench_verifyscript PRIVATE "LINKER:--gc-sections")
endif()
set_target_properties(bench_verifyscript PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
```

  (The C++20/`-O3`/gc-sections options are carried over from the deleted module's target, with
  one adjustment: the deleted module was native-GCC/Clang-only by construction, whereas
  `module/example` also configures under MSVC — `module/CMakeLists.txt` adds it
  unconditionally — so the `-O3` carry-over gains a `NOT MSVC` guard; MSVC Release optimization
  comes from the standard `/O2` configuration flags like the sibling targets. Note the deleted
  module's own `target_link_libraries(bdk_verify_benchmark PRIVATE bdk_core)` line had this
  same latent Boost gap — it was masked only by environments whose Boost sits on a default
  include path; the relocation fixes it instead of copying it.)

**`DISABLE_LOGGING` trade-off — deliberate and documented:** the old benchmark variant compiled
its core with `DISABLE_LOGGING` so measurements excluded node log calls. The relocated benchmark
links the **canonical** `bdk_core` with logging compiled in — which measures the code path real
native consumers actually run, i.e. a MORE representative baseline, and is what makes the
zero-core-mutation architecture possible. If a logging-free measurement is ever needed, the
documented one-off is a separate local build tree, e.g.
`cmake ../bdk -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-DDISABLE_LOGGING`, discarded after
use — never a committed build mode (§10).

### 3.7 `test/core/CMakeLists.txt` — the regression fix (top priority)

**[Reworked by Owner Addendum 5 — two-way gate, no flag, no skip mode.]** Boost
multiprecision is a REQUIRED part of the native dependency contract (header-only, carried by
the official prebuilt packages), so the probe has exactly two outcomes: headers present → the
target is added exactly as today; absent → `FATAL_ERROR` with an actionable message. There is
no `WARNING`-skip branch and no `BDK_REQUIRE_BIGINT_PARITY` option. *(The originally planned
three-way graceful gate — loud local skip + CI-only enforcement flag — was implemented in
commit 5, then superseded.)* Replace the unconditional `test_big_int_boost` block (current
lines 147–155) with:

```cmake
## The big-int parity suite deliberately compiles a module source: the Boost
## bigint backend is module-owned (module/typesbdk/wasm/big_int_boost.cpp), and
## proving backend parity against the OpenSSL core can only happen in a native
## test that links both side by side.
## Boost multiprecision is a REQUIRED part of the native dependency contract
## (header-only, carried by the official prebuilt dependency packages), so a
## missing header is a hard configure error, never a silent skip.
set(_bdk_multiprecision_found FALSE)
get_target_property(_bdk_boost_iface_dirs Boost::boost INTERFACE_INCLUDE_DIRECTORIES)
foreach(_dir IN LISTS _bdk_boost_iface_dirs Boost_INCLUDE_DIRS)
  if(_dir AND EXISTS "${_dir}/boost/multiprecision/cpp_int.hpp")
    set(_bdk_multiprecision_found TRUE)
    break()
  endif()
endforeach()
if(NOT _bdk_multiprecision_found)
  message(FATAL_ERROR
    "boost/multiprecision/cpp_int.hpp was not found in the Boost include "
    "directories. Boost multiprecision is a required native dependency: it backs "
    "the big-int parity suite (test_big_int_boost). Use the refreshed prebuilt "
    "dependency packages from the depcy release, or add 'multiprecision' to the "
    "BOOST_INCLUDE_LIBRARIES of your own Boost install.")
endif()

add_executable(
  test_big_int_boost
  "${CMAKE_CURRENT_SOURCE_DIR}/test_big_int.cpp"
  "${CMAKE_SOURCE_DIR}/module/typesbdk/wasm/big_int_boost.cpp"
)
target_include_directories(test_big_int_boost PRIVATE ${BSV_INCLUDE_DIRS})
target_link_libraries(test_big_int_boost PRIVATE Boost::boost)
target_compile_features(test_big_int_boost PRIVATE cxx_std_20)
bdk_add_unit_test(test_big_int_boost)
```

The two `set_target_properties(test_big_int_boost ...)` lines (current lines 175 and 185)
stay unconditional — the target always exists once configure passes.

---

## 4. Per-file change inventory

Every entry below was verified against the current tree (the PR-49 head).

### Deleted

| File | Content today | Why it goes |
|---|---|---|
| `cmake/BDKCoreConfig.cmake` | 11 lines: the 5 `BDK_CORE_*` profile defaults + empty `bdk_configure_secp256k1_targets()` | Profile mechanism replaced by explicit factory parameters |
| `module/typesbdk/wasm/core-overrides.cmake` | 59 lines: Emscripten guard, `BDK_CORE_*` overrides, root-scope `-ffile-prefix-map` options, secp256k1 hook override | Content relocated into `module/typesbdk/wasm/CMakeLists.txt` (§3.5) |
| `module/typesbdk/benchmark/` (whole directory: `CMakeLists.txt`, `core-overrides.cmake`, `verify_script_benchmark.cpp`) | The PR-49 native benchmark module: 9-line CMakeLists linking `bdk_core`, plus a 3-line override appending `DISABLE_LOGGING` to the shared core's private defs | A native benchmark does not belong in the wasm module tree, and no module may mutate the shared core. The source moves to `module/example/` (§3.6); the `BDK_BUILD_NATIVE_VERIFY_BENCHMARK` flag is deleted with it |

### Created

| File | Content |
|---|---|
| `core/bdk-core-recipe.cmake` | The reusable recipe + `bdk_add_core_library()` factory (§3.1). Owns: `BDK_CORE_RECIPE_DIR` capture, `setting-bitcoin.cmake` / `setting-secp256k1.cmake` / `setting-univalue.cmake` includes (NOT `setting-leveldb.cmake` — §5.10), curated-list handling, extras glob, target creation, includes/links/defs, GNU `sign.cpp` workaround, Win32 libs, IDE folder, `PARENT_SCOPE` export of `BDK_CORE_EXTRA_HDR_FILES`, the `BDK_LOG_BSV_FILES` logging and `source_group` loops. |

### Modified

| File | Exact change |
|---|---|
| `CMakeLists.txt` (root) | Remove line 41 (`option(BDK_BUILD_WASM ...)`), line 42 (`option(BDK_BUILD_NATIVE_VERIFY_BENCHMARK ...)`), lines 71–78 (BDKCoreConfig + override includes) and lines 105–110 (the benchmark `add_subdirectory` block). **No read of `BDK_BUILD_WASM` remains anywhere** — no tripwire (§5.7). Add `BDK_BUILD_CORE` and `BDK_BUILD_TYPES` options and the guard block (§3.3); (`BDK_REQUIRE_BIGINT_PARITY` was also added here as executed, then removed — Owner Addendum 5; the module flag was later renamed `BDK_BUILD_WASM` — Owner Addendum 6). Gate `add_subdirectory(core)` (line 99) on `BDK_BUILD_CORE`; change line 101 gate from `BDK_BUILD_WASM` to the new module flag; add `AND BDK_BUILD_CORE` to the `if(NOT BDK_BUILD_CORE_ONLY)` block at line 112. |
| `cmake/BDKInit.cmake` | Lines 146–150: replace `if(BDK_CORE_FIND_OPENSSL)` with `if(BDK_BUILD_CORE)` (§3.4, rationale §5.5). |
| `cmake/modules/FindBoostHelper.cmake` | Delete lines 146–162 (the `EMSCRIPTEN AND NOT TARGET Boost::boost` header-only branch and its comment) — full revert of the PR-49 hunk; the helper carries no toolchain identifier again. |
| `core/CMakeLists.txt` | Shrinks from 152 lines to the thin native part (§3.2): keeps the Boost lookup (lines 8–13), the `BDK_BSV_ROOT_DIR` check (16–18), then includes the recipe and calls the factory; deletes the moved bodies (version-cpp lines 20–23, list splicing 25–71 including the `BDK_CORE_EXCLUDED_BSV_SOURCES`/`BDK_CORE_ADDITIONAL_SOURCES` splices at 61–62, target creation 73–99 including `${BDK_CORE_LINK_LIBRARIES}` at 79 and `${BDK_CORE_PRIVATE_COMPILE_DEFINITIONS}` at 96); keeps the install/unified-header blocks (101–152) with `BDK_CORE_EXTRA_HDR_FILES` now provided by the factory via `PARENT_SCOPE`. |
| `core/setting-secp256k1.cmake` | Add `if(TARGET secp256k1)\n  return()\nendif()` after the header (idempotency, §5.2); delete line 31 (`bdk_configure_secp256k1_targets()`); everything else (MSVC coverage fix, `SECP256K1_BUILD_TESTS OFF`, `add_subdirectory(... ${CMAKE_CURRENT_BINARY_DIR}/secp256k1)`, IDE folders, install block) unchanged. |
| `core/setting-univalue.cmake` | Add `if(TARGET univalue)\n  return()\nendif()` after the `BDK_BSV_ROOT_DIR` check (defensive idempotency — every build tree has exactly one recipe include today, the guard keeps a future second consumer from colliding silently). |
| `core/setting-leveldb.cmake` | Add `if(TARGET leveldb)\n  return()\nendif()` after the `BDK_BSV_ROOT_DIR` check (belt-and-suspenders — its `add_library(leveldb)` at line 50, `add_library(leveldb-sse4.2)` at line 92 and `add_library(memenv)` at line 96 are unguarded today). The include itself stays in the thin native `core/CMakeLists.txt`, outside the recipe (§5.10). |
| `core/setting-bitcoin.cmake` | No functional change (verified location-independent: it reads only `BDK_BSV_ROOT_DIR` and `BDK_GENERATED_HPP_DIR`, both cache variables, and sets `BITCOIN_CONFIG_FILE` at include scope). Optionally add a one-line comment stating it is include-location-independent. |
| `module/typesbdk/wasm/CMakeLists.txt` | Rewrite the head per §3.5 (guards, module-owned Boost import + 108500 assertion, `-ffile-prefix-map` options, recipe include, `bdk_core_wasm`, secp256k1 surgery). In `add_bdk_wasm_target()` (line 10) change `bdk_core` → `bdk_core_wasm`. The exported-functions list, link options, the four target instantiations and the slim-UMD extras stay byte-identical. |
| `module/typesbdk/wasm/build.sh` | Lines 130–131: replace `-DBDK_BUILD_CORE_ONLY=ON` and `-DBDK_BUILD_WASM=ON` with `-DBDK_BUILD_CORE=OFF` and `-DBDK_BUILD_TYPES=ON` (flag since renamed back to `BDK_BUILD_WASM`, now with the standalone semantics — Owner Addendum 6); keep the explicit `-DBDK_BUILD_MODULES=OFF -DBDK_BUILD_CORE_TESTS=OFF -DBUILD_MODULE_*=OFF` lines (redundant with the root force-off, but explicit is better for a pinned reproducible script). **Boost self-provisioning rewritten (lines 84–94):** the `bootstrap.sh && b2 headers` full-header stage is removed; when no `BOOST_ROOT` is given, the script installs the pinned minimal set via `cmake -DBOOST_INCLUDE_LIBRARIES="$BDK_WASM_BOOST_LIBS"` from the once-downloaded cached tarball (§8) — `BDK_WASM_BOOST_LIBS` is set verbatim to the P0.1 converged survivor list (known and CI-proven before commit 2 exists, §6 Phase 0). Everything else (BSV pin, wasm-opt pass, secp256k1 test suites, size ceilings, vector tests, artifact install) unchanged. |
| `module/example/CMakeLists.txt` | Append the `bench_verifyscript` target after line 48, per §3.6: links canonical `bdk_core` **plus `Boost::chrono`** (required — `txvalidator.hpp` → `taskcancellation.h` → `boost/chrono.hpp`, and `bdk_core` keeps Boost PRIVATE; the component target is already found by line 10's `HelpFindBoost(... chrono)`); carries over the C++20/-O3(`NOT MSVC`)/gc-sections options; follows the file's `FOLDER "module/example"` / `DEBUG_POSTFIX` convention. |
| `module/example/bench_verifyscript.cpp` | **Moved (renamed)** here from `module/typesbdk/benchmark/verify_script_benchmark.cpp`, content unchanged; the new name matches the module's source-name = target-name convention and the pre-existing pointer in `module/gobdk/cgobench/README.md:11`. |
| `module/gobdk/cgobench/README.md` | Fix the C++-benchmark sections to the relocated reality: **correct line 11's source pointer to `../../example/bench_verifyscript.cpp`** — the current `../../../example/bench_verifyscript.cpp` resolves from `module/gobdk/cgobench/` to repo-root `example/`, which does not exist; `module/example/` is two levels up, not three. Replace the stale usage snippets — the binary takes **positional** `iterations` (default 1000) and `samples` (default 9) arguments (`bench_verifyscript 100000 9`), not the documented `-i`/`-c`/`--iterations`/`--disable-consensus` flags, and there is no consensus toggle; fix the build instruction (`cmake --build <build> --target bench_verifyscript` from the build root, binary under the normal output dir). Leave the recorded Go/C++ result tables untouched (historical data). |
| `.gitignore` | Remove line 7 (`/build-native-benchmark/`): the dedicated native-benchmark build tree belonged to the rejected flag path and no longer exists — `bench_verifyscript` builds inside the regular build tree. The `build-wasm*/` ignores stay. |
| `test/core/CMakeLists.txt` | Per §3.7 (Addendum-5 form): two-way multiprecision probe (present → target, absent → actionable `FATAL_ERROR`) + reverse-dependency comment; property lines unconditional. |
| `.github/workflows/build_bdk.yaml` | **[Superseded by Owner Addendum 5]** As executed in commit 5 this row added the parity flag + in-job provisioning step; both were removed again — the packages carry multiprecision permanently and a stale package fails configure loudly with the §3.7 message. The Windows steps (lines 152–167, incl. the configure at line 163) are **unreachable dead code** — the matrix (lines 27–31) has no Windows entry — and stay untouched (§11). |
| `.github/workflows/prebuild_dependancies.yaml` | **Changed on `master` in the preparatory commit P0.2, NOT in this PR** (§6 Phase 0): (a) the wasm minimal-Boost job whose `WASM_BOOST_INCLUDE_LIBRARIES` is the **P0.1 empirically converged survivor list**, verbatim (headers-only package `dependancies_wasm.tar.gz`) — never a static estimate; (b) `multiprecision` added to the native `BOOST_INCLUDE_LIBRARIES` (line 57) — folded into the same commit by explicit decision (same file, same theme, one workflow dispatch refreshes all assets before the PR), **now PERMANENT (Owner Addendum 5)**: multiprecision is a declared native dependency and Phase Z (the planned purity revert) is cancelled. CI-verified at the P0.3 human checkpoint. |
| `.github/workflows/build_wasm.yaml` | Commit 5: add the step downloading `dependancies_wasm.tar.gz` (live since P0.3) from the `depcy` release and exporting `BOOST_ROOT` to its unpacked tree before `build.sh` — CI thereby validates the pinned minimal Boost set on every run (§8 item 2c) — plus the direct-configure smoke step for the standalone path (§9). The build.sh invocation, byte-compare and artifact upload steps stay unchanged. |
| `documentation/docs/architecture.md` | Line 150 region: replace "opt-in root CMake target (`BDK_BUILD_WASM=ON`)" with the standalone-module description + the WHY paragraph (§10). |
| `documentation/docs/build.md` | Flag table (lines ~143–153): add `BDK_BUILD_CORE` and the module flag (executed as `BDK_BUILD_TYPES`, now `BDK_BUILD_WASM` — Owner Addendum 6); note the name reuse and that Boost multiprecision is a required native dependency (Owner Addendum 5 — no parity flag, no optional/graceful wording). |
| `module/typesbdk/examples/README.md` | Update the wasm build commands to the new flags, and replace the native-benchmark configure block (lines ~82–98: the dedicated `build-native-benchmark` tree with `-DBDK_BUILD_NATIVE_VERIFY_BENCHMARK=ON`) with the new reality — `bench_verifyscript` is built by the regular native build; document the `DISABLE_LOGGING` trade-off note (§3.6, §10). |

### Explicitly unchanged (so the reviewer knows they were considered)

- `cmake/modules/FindBSVSourceHelper.cmake` — the curated lists stay exactly as they are,
  including PR-49's `src/support/cleanse.cpp` addition (line 193; a blessed native-core delta:
  native core references it via `CDataStream`/`zeroafterfree.h`). The lists are stored via
  `bdkAppendToGlobalSet` as `CACHE INTERNAL`, hence visible from any directory — no change needed
  for module-side reuse.
- `core/txvalidator.{cpp,hpp}` and all functional C++ — out of scope for this refactor.
- The eight committed wasm artifacts (`bdk-core*.{mjs,js,wasm}`) — expected byte-identical; §5.8.
- `test/CMakeLists.txt`, `module/CMakeLists.txt` — no reference to any removed mechanism
  (verified by grep); the root guard block handles their activation.

---

## 5. Little details — engineering subsections

### 5.1 Recipe extraction engineering (location independence, file by file)

The current `core/CMakeLists.txt` and its includes assume `core/` is the current directory in
exactly these places (all verified):

| File | Current-directory assumption | Fix |
|---|---|---|
| `core/CMakeLists.txt:21` | `${CMAKE_CURRENT_SOURCE_DIR}/BDKVersion.cpp.in` | Factory uses `${BDK_CORE_RECIPE_DIR}/BDKVersion.cpp.in`; output path `${BDK_GENERATED_CPP_DIR}/BDKVersion.cpp` is already location-independent (global cache dir). |
| `core/CMakeLists.txt:29-38` | `include("${CMAKE_CURRENT_SOURCE_DIR}/setting-*.cmake")` | Recipe includes them via `${BDK_CORE_RECIPE_DIR}/...`. `BDK_CORE_RECIPE_DIR` is captured from `CMAKE_CURRENT_LIST_DIR` at recipe include time — CMake 3.16-compatible (no `CMAKE_CURRENT_FUNCTION_LIST_DIR`, which needs 3.17). |
| `core/CMakeLists.txt:58-59` | `file(GLOB_RECURSE ... "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" ...)` | Factory globs `${BDK_CORE_RECIPE_DIR}/*` so the extras are always the `core/` sources, never the caller's directory. |
| `core/CMakeLists.txt:76` | `target_include_directories(... ${CMAKE_CURRENT_SOURCE_DIR})` | Factory uses `${BDK_CORE_RECIPE_DIR}`. |
| `core/setting-bitcoin.cmake` | **None** — reads `BDK_BSV_ROOT_DIR` (cache) and writes `${BDK_GENERATED_HPP_DIR}/config/bitcoin-config.h` (cache dir); sets `BITCOIN_CONFIG_FILE` as a normal variable at include scope, which the recipe/factory sees. Compiler probes (`check_include_files`, `check_symbol_exists`, `check_builtin_exist`) run against whatever toolchain is active — correct for both native and Emscripten. | No change. |
| `core/setting-secp256k1.cmake:30` | `add_subdirectory("${BDK_BSV_ROOT_DIR}/src/secp256k1" ${CMAKE_CURRENT_BINARY_DIR}/secp256k1)` — the explicit binary-dir argument makes this instantiable from any directory; at include-from-module time it resolves to the **module's** binary dir, which is exactly the desired "the module instantiates its own secp256k1". | Keep; add the TARGET guard (§5.2) and drop the hook call (line 31). |
| `core/setting-univalue.cmake` | **None** (all paths derived from `BDK_BSV_ROOT_DIR`). The `univalue` target is a real target — globally visible once created. | Add the TARGET guard only. |

**List propagation:** `BSV_MINIMAL_SRC_FILES` / `BSV_MINIMAL_HDR_FILES` / `BSV_INCLUDE_DIRS` are
`CACHE INTERNAL` globals (populated by `bdkAppendToGlobalSet`, `cmake/BDKTools.cmake:266`), so
they are readable from any directory without `PARENT_SCOPE` plumbing. The factory takes **local
copies** before `list(REMOVE_ITEM ...)` — a plain `set()`/`list()` on a cache-variable name
creates a scope-local shadow and never writes back to the cache, so one variant's exclusions can
never leak into another variant or a later reconfigure. (The current PR already relies on this
shadowing at `core/CMakeLists.txt:61`; the factory makes it explicit and per-call.)

**Scope model:** the recipe file is included at *directory scope* by its consumer — `core/` in a
native tree, `module/typesbdk/wasm/` in a wasm tree; exactly one include per build tree.
Variables it sets (`BDK_CORE_RECIPE_DIR`, `UNIVALUE_LIB`, `BITCOIN_CONFIG_FILE`) live in that
consumer's scope; function definitions are global but resolve those variables at call time in
the caller's scope — which is always the scope that just included the recipe. There is
deliberately **no include guard** on the recipe file: sibling directory scopes don't inherit
each other's variables, so a guard variable would be invisible to (and useless against) an
include from another directory. Instead the side-effecting parts carry defensive
`if(TARGET ...) return()` guards inside `setting-secp256k1.cmake` / `setting-univalue.cmake`,
so a hypothetical future second consumer in one tree degrades to target reuse instead of a
duplicate-target configure error; re-running `setting-bitcoin.cmake` and redefining the factory
function are both harmless (identical outputs).

**Directory-scoped effects inside the factory:** `set_source_files_properties` (the GNU
`sign.cpp` `-Wno-stringop-overread` workaround) is directory-scoped in CMake 3.16; since the
factory is a function it executes in the caller's directory scope, the property lands exactly on
the directory building that variant. Same reasoning applies to the module's `add_compile_options`
call (§3.5): it now covers the core variant, the secp256k1 sub-build and the bindings because all
of them are created from the module's directory scope.

### 5.2 `add_subdirectory(secp256k1)` — single instantiation

Contract: **only whoever builds a core variant instantiates secp256k1**, and at most once per
build tree.

- Mechanism: with exactly one recipe include per build tree (native → `core/`, wasm → the
  module), single instantiation holds by construction. `core/setting-secp256k1.cmake`
  additionally gains a defensive `if(TARGET secp256k1) return() endif()` at the top so any
  future second consumer reuses the existing sub-build instead of hitting a duplicate-target
  error. `add_subdirectory` already passes an explicit binary dir
  (`${CMAKE_CURRENT_BINARY_DIR}/secp256k1`), so the sub-build lands under the includer's
  binary directory.
- Native default (`BDK_BUILD_CORE=ON`): `core/` includes the recipe → secp256k1 lives at
  `<build>/core/secp256k1`, exactly where it is today.
- WASM standalone (`BDK_BUILD_CORE=OFF`, `BDK_BUILD_WASM=ON`): only the wasm module includes the
  recipe → secp256k1 lives at `<build>/module/typesbdk/wasm/secp256k1`, built with the Emscripten
  toolchain and the `SECP256K1_*` cache options `build.sh` passes
  (`SECP256K1_ASM=OFF`, `ECMULT_WINDOW_SIZE=15`, `ECMULT_GEN_KB=2`,
  `TEST_OVERRIDE_WIDE_MULTIPLY=int64`, `BUILD_BENCHMARK=OFF` — cache variables, so they reach the
  sub-build regardless of which directory instantiates it). The module then rewires
  `secp256k1_precomputed`/`secp256k1` sources in place (§3.5) — no hook function needed, because
  the targets are created from the module's own scope.
- The IDE-folder loop and the secp256k1 header/target install block in
  `setting-secp256k1.cmake:34-64` run once (guard) and the install part remains gated on
  `BDK_INSTALL_BSV_HEADERS` (which `build.sh` sets `OFF`), so the wasm build still installs
  nothing.

### 5.3 Factory contract discipline

The factory covers all five removed profile variables. **Four** of them become factory
parameters 1:1; the **fifth (`BDK_CORE_FIND_OPENSSL`) is a deliberate, documented exception** —
it does not become a parameter, and this is a conscious deviation from the "~5 parameters
including find-openssl/no-openssl" sketch in the task brief:

| Removed profile variable | Factory replacement |
|---|---|
| `BDK_CORE_EXCLUDED_BSV_SOURCES` | `EXCLUDE_BSV_SOURCES <abs paths...>` |
| `BDK_CORE_ADDITIONAL_SOURCES` | `ADDITIONAL_SOURCES <abs paths...>` |
| `BDK_CORE_LINK_LIBRARIES` | `LINK_LIBRARIES <libs...>` |
| `BDK_CORE_PRIVATE_COMPILE_DEFINITIONS` | `COMPILE_DEFINITIONS <defs...>` |
| `BDK_CORE_FIND_OPENSSL` | **Deliberately not a parameter** — see rationale below. The OpenSSL/no-OpenSSL choice is still fully expressible per variant: an OpenSSL variant passes `LINK_LIBRARIES OpenSSL::Crypto OpenSSL::SSL`, a no-OpenSSL variant simply omits them. |

Rationale for the exception (it is technical, not stylistic): the old variable's only effect was
to gate `find_package(OpenSSL)` *discovery* in `BDKInit.cmake`, and discovery **cannot** move
into a factory called from a subdirectory — imported targets are directory-scoped, and sibling
consumers (`test/core` links `OpenSSL::Crypto` directly) would not see them (§5.5). Discovery
therefore stays at root scope gated on `BDK_BUILD_CORE`, and a factory `OPENSSL ON|OFF` flag
would be redundant state that could silently disagree with what `LINK_LIBRARIES` actually links.
One fact, one owner: discovery at root, linkage via `LINK_LIBRARIES`.

(The sixth PR-49 mechanism, the `bdk_configure_secp256k1_targets()` hook, is not a parameter
either: post-instantiation target surgery happens in the caller's own directory after the recipe
include — §5.2.)

**Parameter creep is forbidden.** The factory header comment must state: any future
specialization need must be expressed either (a) through the four existing parameters, or (b) as
post-hoc `target_*` calls the caller applies to its own variant in its own directory, or (c) as a
recipe-level change that benefits every variant. Adding a fifth parameter requires the same
review bar as editing core itself. This keeps the factory from re-growing into the profile
mechanism it replaces.

### 5.4 `test_big_int_boost` regression fix (top priority)

Full sketch in §3.7 (Addendum-5 form). Requirements it satisfies:

- **Hard requirement, actionable failure (Owner Addendum 5):** Boost multiprecision is a
  declared, required native dependency — header-only, ~2-3 MB, carried permanently by the
  refreshed official packages. A Boost install without it fails configure with a
  `FATAL_ERROR` naming the remedy (refreshed `depcy` packages, or add `multiprecision` to
  `BOOST_INCLUDE_LIBRARIES`). There is no skip mode and no CI flag. *(The originally planned
  model — graceful loud skip locally, `BDK_REQUIRE_BIGINT_PARITY=ON` + in-job provisioning in
  CI, Phase-Z package-purity revert — was implemented, then superseded by Addendum 5.)*
- **Probe correctness:** the probe iterates both `Boost::boost`'s
  `INTERFACE_INCLUDE_DIRECTORIES` and `Boost_INCLUDE_DIRS`, because depending on how Boost was
  found (CMake config package vs module) either may carry the include root. `EXISTS` on the
  concrete header is the exact failure condition observed.
- The suite itself (`test_big_int.cpp` compiled against `big_int_boost.cpp`) is unchanged, and
  the reverse dependency (a core test compiling a module source) is now explained by the comment
  block — closing the prior analysis' ask 4. The test file additionally documents WHY the
  parity suite exists: two backends of the consensus-critical `bsv::bint` arithmetic, one
  suite run identically against both.

### 5.5 OpenSSL discovery and imported-target visibility

A naive reading of "move the OpenSSL gate behind the native call site" would put
`HelpFindOpenSSL()` inside `core/CMakeLists.txt`. **That breaks the tests:** `find_package`
imported targets (`OpenSSL::Crypto`, `OpenSSL::SSL`) are *directory-scoped* — visible in the
creating directory and below. `test/core/CMakeLists.txt:111,132,136,...` links
`OpenSSL::Crypto OpenSSL::SSL` directly from a **sibling** directory of `core/`, so discovery in
`core/` scope would leave the test targets without the imported targets.

Decision: discovery **stays in `cmake/BDKInit.cmake`** (root scope, visible everywhere), and the
gate becomes simply `if(BDK_BUILD_CORE)` — every OpenSSL consumer (the canonical core, the
modules incl. `module/example`'s benchmarks, the tests) exists only when the native core is
built. Consequences, all verified:

- default native: identical discovery to today (gate true);
- wasm standalone: `BDK_BUILD_CORE=OFF` → **no OpenSSL discovery at all** under Emscripten (the
  current head achieves this via the wasm override setting `BDK_CORE_FIND_OPENSSL=OFF`; the new
  gate achieves it from the one root flag alone);
- no core-profile variable survives in `BDKInit.cmake`.

Boost is different and needs no such care: the existing pattern already re-runs
`HelpFindBoost(...)` per consuming directory (`core/CMakeLists.txt:8`, `test/CMakeLists.txt:8`,
`test/core/CMakeLists.txt:8`), and the wasm module's header-only import is created `GLOBAL`
(§3.5) so the recipe factory sees it from the same scope.

### 5.6 Boost discovery for the WASM module

- The `EMSCRIPTEN` branch is removed from `cmake/modules/FindBoostHelper.cmake` (full revert of
  the PR-49 hunk at lines 146–162); the shared helper again contains zero toolchain identifiers.
- The module creates `Boost::boost` itself (§3.5), honoring the same `CUSTOM_BOOST_ROOT` /
  `ENV{BOOST_ROOT}` precedence the helper uses, and **asserts `BOOST_VERSION == 108500`** by
  parsing `boost/version.hpp` — so a direct `emcmake cmake` invocation (not routed through
  `build.sh`) now enforces the same pinned Boost the reproducible build requires. `build.sh`'s
  own version check remains as the outer belt.
- Ordering note: the module's Boost import must precede the recipe include (the factory links
  `Boost::boost`), and the `-ffile-prefix-map=${BOOST_ROOT}` option must be added after
  `BOOST_ROOT` is finalized. The §3.5 sketch encodes this order.

### 5.7 The single module flag: `BDK_BUILD_WASM` (executed as `BDK_BUILD_TYPES`; the PR-49 overlay flag removed completely)

**[Owner Addendum 6: the flag was later renamed `BDK_BUILD_TYPES` → `BDK_BUILD_WASM`** —
"TYPES" collides with CMake build-type terminology. The standalone semantics and both guards
are unchanged under the new name; the "nothing reads `BDK_BUILD_WASM`" property below held
between commit 3 and the rename, and the name is now live again with the new meaning.]

Decision (owner-mandated): there is exactly **one** module flag — `BDK_BUILD_WASM`, executed
as `BDK_BUILD_TYPES` until the Addendum-6 rename. It activates
the typesbdk module build (today that module's only build product is the WASM verifier; if a
native TypeScript-support target ever appears it will live under the same flag, which is why the
flag is named after the module, not the toolchain).

`BDK_BUILD_WASM` is removed **completely**: the option is deleted and **no CMake file in the
tree reads the name** — no tripwire, no deprecation shim, no compatibility branch.
Consequences, stated honestly:

- A stale `-DBDK_BUILD_WASM=OFF` (the historical canonical native command, kept verbatim as
  gate 1) just defines a cache variable nothing reads; CMake prints its standard
  `Manually-specified variables were not used by the project: BDK_BUILD_WASM` notice at the end
  of the configure run. That notice is the accepted, documented behavior — it is also the only
  migration hint a caller gets.
- A stale `-DBDK_BUILD_WASM=ON` is equally inert: the build configures as a plain native build
  (no wasm targets appear). Anyone driving the wasm build through the old flag will notice the
  missing artifacts immediately, and the docs (§10) plus the same unused-variable notice point
  to the module flag. The owner explicitly accepts this trade-off in exchange for zero
  legacy-flag reads in the tree.
- `BDK_BUILD_CORE_ONLY` keeps its historical meaning (native: build only core). The wasm path no
  longer uses it (`build.sh` previously passed it; the standalone invocation replaces it with
  `BDK_BUILD_CORE=OFF`), and `BDK_BUILD_CORE_ONLY=ON` together with `BDK_BUILD_CORE=OFF` is a
  hard configure error (§3.3).
- Guard summary (all in §3.3): `BDK_BUILD_WASM` requires `EMSCRIPTEN` (root check *and* the
  module's own first-line check, since the module dir could in principle be added by hand);
  `BDK_BUILD_WASM` requires `BDK_BUILD_CORE=OFF`; `BDK_BUILD_CORE=OFF` force-disables every
  shared-core consumer with a `STATUS` line each.

### 5.8 Byte-reproducibility of the committed WASM artifacts

The refactor intends **byte-identical artifacts**: the same source set, the same compile
definitions (`BOOST_HAS_PTHREADS`, `DISABLE_LOGGING`), the same secp256k1 configuration and
surgery, the same `-O3`/link options and the same `wasm-opt` pass. Two things change and must be
watched:

1. **`-ffile-prefix-map` scope** moves from root directory scope (the override file was included
   from the root) to the module's directory scope. Because in the standalone build *every*
   compiled target (core variant, secp256k1, bindings) is created from the module's directory,
   coverage is unchanged. The flags' relative order on the command line may shift; prefix-map
   flags are order-independent among themselves for non-overlapping roots, and the three roots
   map the most-specific first exactly as today.
2. **Compile-flag ordering** on the core-variant TUs could differ (definitions now arrive via the
   factory argument instead of the profile variable). Definition order does not affect codegen.

CI's existing byte-compare (`build_wasm.yaml` "Verify committed artifacts are reproducible",
`git diff --exit-code` over the eight committed files) is the arbiter. If the gate-run diff is
non-empty, first diff the generated build command lines (`compile_commands.json` /
`VERBOSE=1`) against a pre-refactor build to find the flag drift and fix the CMake; only if a
benign, explainable difference remains (it should not) are the artifacts regenerated — and that
regeneration happens **only in the final fix commit** (§6), never in the code-only commits.

### 5.9 Benchmark relocation to `module/example/` (module and flag deleted)

Owner decision: a native C++ benchmark must not live inside the wasm module tree, and it does
not warrant a build flag or a core variant of its own. `module/typesbdk/benchmark/` and
`BDK_BUILD_NATIVE_VERIFY_BENCHMARK` are **deleted**; `verify_script_benchmark.cpp` joins the
existing native examples module `module/example/` as `bench_verifyscript`, next to the four
`bench_*` executables already there, built by the regular native modules build and linking the
**canonical `bdk_core`** (§3.6 has the exact target block).

- **No `DISABLE_LOGGING` core anywhere:** the relocated benchmark measures the canonical core
  with logging compiled in — the code path real native consumers (gobdk, rustbdk) actually run,
  i.e. a MORE representative baseline than the old logging-stripped variant. For an occasional
  logging-free measurement, the documented one-off is a throwaway local tree
  (`-DCMAKE_CXX_FLAGS=-DDISABLE_LOGGING`), never a committed build mode (§3.6, §10).
- **Ask 1 of the prior analysis closes structurally:** with no benchmark profile and no
  benchmark core variant, nothing anywhere declares core deviations natively; the only factory
  consumer besides the canonical core is the wasm module in its own standalone tree.
- **Always built, run anytime:** `module/example` is added unconditionally by
  `module/CMakeLists.txt` under the default `BDK_BUILD_MODULES=ON`, so `bench_verifyscript`
  lands in the normal build output of the canonical native build — no special configure step.
- The examples README's dedicated `build-native-benchmark` configure block becomes obsolete and
  is replaced by a one-liner (build normally, run `bench_verifyscript`) plus the trade-off note.

### 5.10 LevelDB handling

`core/setting-leveldb.cmake` is **kept out of the reusable recipe**. Grounds (verified):

- no core variant links leveldb — the core link line (`core/CMakeLists.txt:77-80` today, the
  factory tomorrow) is `secp256k1 ${UNIVALUE_LIB} Boost::boost` plus caller libraries; there is
  no `LEVELDB_LIB` anywhere. LevelDB is an optional native side build, not a core ingredient;
- the file is **not multi-include-safe**: `add_library(leveldb)` (line 50),
  `add_library(leveldb-sse4.2)` (line 92) and `add_library(memenv)` (line 96) are unguarded, and
  it additionally mutates directory compile flags via `bdk_add_compiler_flag`/
  `bdk_remove_compiler_flags` (lines 35–40, 215–219) — if it lived in the recipe, any future
  second recipe consumer in one tree with `BDK_BUILD_LEVELDB=ON` would collide on duplicate
  target names.

Therefore the `if(BDK_BUILD_LEVELDB) include(setting-leveldb.cmake)` block moves to the **thin
native** `core/CMakeLists.txt` (§3.2) — it configures at most once per build tree, in the same
directory it configures in today, so behavior is unchanged. As belt-and-suspenders the file also
gains an `if(TARGET leveldb) return() endif()` guard matching the univalue/secp256k1 pattern, so
even a future erroneous second include degrades to a no-op instead of a configure error.

---

## 6. Commit strategy

The sequence has two phases: a **preparatory phase** that derived and published the wasm Boost
package on `master` (independent of the refactor PR, with a hard human checkpoint — **now
completed and verified**, see Phase 0 below), then the **refactor PR's essential commits**.

Rules for the whole sequence:

- **Each commit contains code changes only** — CMake/scripts/workflows/tests/docs edits. No
  build runs, no regenerated artifacts, no committed binaries inside the commit steps. (The
  preparatory phase's *derivation* is implementation work, not a commit — its builds happen
  before any commit exists.)
- **Each commit is converged before the next one starts:** the author drafts the commit's
  content, the designated reviewer critiques it, and the commit is amended until the reviewer
  raises no unresolved critical point; only then does work on the next commit begin.
- **The native configure path is never broken mid-sequence.** (The wasm path may be transiently
  broken between commits 2 and 3 — it is only required to work at the end, when the gates run.)
- **Builds and tests for the refactor commits run only after commit 6** (§7 gates), followed by
  at most **one** final fix commit if a gate fails. Commits 1–6 never depend on a build result:
  the one build-derived input (the converged Boost list) is produced and published in the
  preparatory phase, before commit 1 exists.
- Commit messages are plain, thematic, imperative, in the repository's existing style, with no
  trailers or footers beyond the message itself.

### Phase 0 — preparatory wasm Boost package: ✅ COMPLETED AND VERIFIED (nothing left to do)

All three preparatory steps are done; no Phase-0 work remains. Record of completion:

- **P0.1 — Local derivation: EXECUTED.** Add-and-prune loop run to convergence on the PR-49
  head; results (8-component pinned list, per-component evidence, BOOST_ROOT layout rule) in
  `.pr49-plan-context/wasm_boost_derivation_results.md` and §8.
- **P0.2 — Preparatory commit: DONE.** The owner applied the `prebuild_dependancies.yaml`
  change directly to `master` (the `build-upload-wasm-dependencies` job pinned to the converged
  list, plus `multiprecision` in the native `BOOST_INCLUDE_LIBRARIES`) and dispatched the
  workflow on CI.
- **P0.3 — Checkpoint: CONFIRMED.** The CI dependencies run succeeded and
  `dependancies_wasm.tar.gz` is published on the `depcy` release. End-to-end consumption was
  then verified locally: the published package (6.5 MB, layout
  `dependancies_wasm/boost_1.85.0/boost/…`, `BOOST_VERSION 108500`) was downloaded, extracted,
  and a full clean wasm build at the PR-49 head ran against it with
  `BOOST_ROOT=<unpacked>/dependancies_wasm/boost_1.85.0` — exit 0, libsecp256k1 wasm suites
  passed, all four loaders' positive/corrupt vectors passed, and every committed artifact was
  reproduced **byte-for-byte**.

**How to use the prebuilt wasm dependencies** (for any wasm build, local or CI — this replaces
any need to build Boost yourself):

```bash
# one-time: fetch and unpack the published package
curl --fail --location -o /tmp/dependancies_wasm.tar.gz \
  "https://github.com/bitcoin-sv/bdk/releases/download/depcy/dependancies_wasm.tar.gz"
mkdir -p build-wasm-deps && tar -xzf /tmp/dependancies_wasm.tar.gz -C build-wasm-deps

# every wasm build: point BOOST_ROOT at the package's boost_1.85.0 dir
# (it contains boost/ DIRECTLY — required for artifact byte-reproducibility)
source /path/to/emsdk/emsdk_env.sh   # Emscripten 4.0.23
BOOST_ROOT="$PWD/build-wasm-deps/dependancies_wasm/boost_1.85.0" module/typesbdk/wasm/build.sh
```

The refactor PR branches from `master` at or after the preparatory commit, inherits the
workflow edits, and touches `prebuild_dependancies.yaml` no further. Commit 5 ships the
configure-time gate with the multiprecision-carrying native packages already live.
**[Owner Addendum 5 supersedes the earlier bridge note:] the native `multiprecision`
addition is PERMANENT** — multiprecision is a declared native dependency, Phase Z is
cancelled, and the CI flag + in-job provisioning shim were subsequently removed in favor of
the two-way hard gate (§3.7).

### Commit 1 — `build: extract reusable core recipe and library factory`

- Create `core/bdk-core-recipe.cmake` (§3.1).
- Shrink `core/CMakeLists.txt` to the thin native part (§3.2); the factory is called with
  explicit canonical native arguments (`LINK_LIBRARIES OpenSSL::Crypto OpenSSL::SSL`), so the
  still-present root profile variables become dead inputs — harmless this commit, removed in
  commit 3.
- `core/setting-secp256k1.cmake`: TARGET guard added, hook call removed.
- `core/setting-univalue.cmake`: TARGET guard added.
- `core/setting-leveldb.cmake`: TARGET guard added; its `BDK_BUILD_LEVELDB` include moves from
  the recipe scope to the thin native part (§5.10).
- `cmake/BDKInit.cmake`: OpenSSL gate switched from `BDK_CORE_FIND_OPENSSL` to
  `BDK_BUILD_CORE` (with `BDK_BUILD_CORE` not yet an option, an undefined variable is falsey —
  so this commit also adds the `BDK_BUILD_CORE` option to the root, default `ON`, with no other
  root changes).
- Native semantics after this commit: identical configure/build behavior (same discovery, same
  target, same flags). WASM via the old override path is already dead here (the hook call is
  gone) — accepted per the mid-sequence rule.

### Commit 2 — `build(wasm): make the typesbdk wasm module a standalone build`

- Rewrite `module/typesbdk/wasm/CMakeLists.txt` per §3.5 (guards, module-owned Boost import with
  the 108500 assertion, prefix maps, recipe include, `bdk_core_wasm`, secp256k1 surgery, binding
  link change).
- Delete `module/typesbdk/wasm/core-overrides.cmake`.
- Revert the `EMSCRIPTEN` branch in `cmake/modules/FindBoostHelper.cmake`.
- Update `module/typesbdk/wasm/build.sh`: the flag changes (§4 table), and **replace the
  `bootstrap.sh && b2 headers` full-header self-provisioning with the minimal
  `cmake -DBOOST_INCLUDE_LIBRARIES="$BDK_WASM_BOOST_LIBS"` install** (§8), with
  `BDK_WASM_BOOST_LIBS` set verbatim to the **P0.1 converged survivor list** — already known
  and CI-proven before this commit exists, so no provisional value ever appears in the tree.
- Native untouched by construction (all edits are module-local or Emscripten-only code paths).

### Commit 3 — `build: root wiring for the standalone wasm build`

- Root `CMakeLists.txt` per §3.3: remove the `BDK_BUILD_WASM` option (**no read of the name
  remains — no tripwire**, §5.7) + BDKCoreConfig/override includes; add the `BDK_BUILD_TYPES`
  option (since renamed `BDK_BUILD_WASM` — Owner Addendum 6) and the guard block (as executed
  this commit also added `BDK_REQUIRE_BIGINT_PARITY`,
  since removed — Owner Addendum 5); gate `add_subdirectory(core)` and the
  `NOT BDK_BUILD_CORE_ONLY` block on `BDK_BUILD_CORE`.
- Delete `cmake/BDKCoreConfig.cmake`.
- Native default path: all guards inert (`BDK_BUILD_CORE=ON`, module flag OFF — today
  `BDK_BUILD_WASM`), behavior
  identical.

### Commit 4 — `build: move the VerifyScript benchmark into module/example`

- `git mv module/typesbdk/benchmark/verify_script_benchmark.cpp
  module/example/bench_verifyscript.cpp` (content unchanged; renamed per the module's
  source-name = target-name convention and the existing cgobench README pointer); delete the
  rest of `module/typesbdk/benchmark/` (`CMakeLists.txt`, `core-overrides.cmake`).
- Append the `bench_verifyscript` target to `module/example/CMakeLists.txt` per §3.6 — linking
  `bdk_core Boost::chrono` (the Boost link is load-bearing: §3.6 explains the
  `txvalidator.hpp` → `taskcancellation.h` → `boost/chrono.hpp` chain that `bdk_core`'s PRIVATE
  Boost does not propagate).
- Root `CMakeLists.txt`: remove the `BDK_BUILD_NATIVE_VERIFY_BENCHMARK` option (line 42) and its
  `add_subdirectory` block (lines 105–110) — no read of that name remains either.
- `.gitignore`: remove line 7 (`/build-native-benchmark/`) — residue of the rejected dedicated
  benchmark tree.
- Replace the benchmark configure snippet in `module/typesbdk/examples/README.md` (the dedicated
  `build-native-benchmark` tree is obsolete: build normally, run `bench_verifyscript`) and add
  the `DISABLE_LOGGING` trade-off note (§3.6).
- Update the C++-benchmark sections of `module/gobdk/cgobench/README.md`: correct the source
  pointer at line 11 to `../../example/bench_verifyscript.cpp` (the current `../../../…` climbs
  to the repo root, one level too far); correct usage (positional `iterations`/`samples`, no
  consensus flag) and the build target/path — §4 inventory row.

### Commit 5 — `test: degrade big-int parity suite gracefully without Boost multiprecision`

- `test/core/CMakeLists.txt` per §3.7 — as executed: the original three-way graceful gate;
  **later reworked by Owner Addendum 5** to the two-way probe (present → target, absent →
  actionable `FATAL_ERROR`), no flag, unconditional property lines.
- (`.github/workflows/prebuild_dependancies.yaml` is **not touched by this PR** — both its
  edits, the wasm package job and the native multiprecision addition, landed on `master` in the
  preparatory commit P0.2 and were CI-verified at the P0.3 checkpoint.)
- `.github/workflows/build_wasm.yaml`: add the `dependancies_wasm.tar.gz` download +
  `BOOST_ROOT` export before `build.sh` (CI validation of the pinned set, §8 item 2c — the
  package is guaranteed live by the P0.3 checkpoint) and the standalone direct-configure smoke
  step (§9).
- `.github/workflows/build_bdk.yaml` — as executed: the parity flag on the unix configure and
  the in-job multiprecision provisioning step. **[Superseded by Owner Addendum 5: both removed
  again]** — the packages carry multiprecision permanently (Phase Z cancelled), so the two-way
  configure gate alone enforces the suite and a stale package fails loudly at configure. The
  Windows steps (lines 152–167) are unreachable dead code (the matrix has no Windows entry)
  and stay untouched (§11).

### Commit 6 — `docs: document the standalone wasm build architecture`

- `documentation/docs/architecture.md`, `documentation/docs/build.md`,
  `module/typesbdk/examples/README.md` remaining edits, per §10.

### End of sequence — validate, then at most one fix commit

(The derivation, the workflow pin and the asset publication all happened in Phase 0; nothing
remains to converge or refresh here.)

1. Run gate 1 (native) then gate 2 (wasm standalone) from §7 — gate 2 runs against the
   **published** `dependancies_wasm.tar.gz` package, so it locally validates exactly what
   CI validates.
2. Confirm the PR's own CI is green — in particular `build_bdk.yaml` must show
   `test_big_int_boost` **built and passing** (a skip is impossible: the §3.7 two-way gate has
   no skip mode — Owner Addendum 5), and `build_wasm.yaml` must build against the pinned
   minimal Boost package.
3. If anything fails, fix all findings in **one** final commit
   (`build: fix validation findings`), which is also the only commit allowed to regenerate
   committed wasm artifacts should the byte-compare demand it (§5.8). Re-run steps 1–2 after
   the fix commit. (If a gate exposes a wrong Boost component list — which P0.1's loop and
   P0.2's CI run make very unlikely — the workflow fix goes through the owner on `master`
   again, mirroring Phase 0, since the PR does not own `prebuild_dependancies.yaml`.)

### Phase Z — post-gates: restore native dependency-package purity — **CANCELLED (Owner Addendum 5)**

**CANCELLED by Owner Addendum 5:** Boost multiprecision is REQUIRED EVERYWHERE as a declared
part of the native dependency contract (header-only, ~2-3 MB, already in the refreshed
packages) — do not over-optimize the Boost install. `multiprecision` stays in the native
`BOOST_INCLUDE_LIBRARIES` permanently; the `BDK_REQUIRE_BIGINT_PARITY` option and the in-job
provisioning shim were subsequently REMOVED: the configure-time probe is now a two-way hard
gate (headers present → `test_big_int_boost` builds unconditionally; absent → `FATAL_ERROR`
with an actionable message). The original Phase-Z text below is kept for the record only.

Runs only after the refactor PR is **CI-proven** — gates passed, `build_bdk.yaml` green with
`test_big_int_boost` built and passing under `-DBDK_REQUIRE_BIGINT_PARITY=ON` (i.e. the
configure-time gate and the in-job provisioning shim demonstrably work end-to-end).

**Z.1 — Owner-executed revert on `master`:** remove `multiprecision` from the native
`BOOST_INCLUDE_LIBRARIES` in `prebuild_dependancies.yaml` (returning the list to the EXACT
pre-PR-49 component set) and re-dispatch the workflow to refresh the native
`dependancies_<os>_<arch>.tar.gz` packages. **The wasm package is untouched** — its own pinned
list legitimately contains `multiprecision` (`big_int_boost.cpp` is wasm-module code).

**Why (owner decision, recorded):** the native `multiprecision` addition was only ever a
**bridge** — it existed to un-break PR-49's unconditional `test_big_int_boost` target during
the window before commit 5's graceful gate + shim landed. The canonical native build needs no
multiprecision; a wasm-module test must not change the native dependency contract — the same
architecture principle as the build-graph rule ("modules must not modify the core build"),
applied to dependency packages. The recorded **CI-race incident of 2026-07-27** is the evidence
that environment-side fixes are fragile: jobs that executed at 00:31Z ran against the
pre-refresh packages and failed with missing `cpp_int.hpp`, while re-runs after the ~02:2xZ
asset refresh passed — asset state and job start time raced. The durable shape is code-side:
the configure-time gate (`BDK_REQUIRE_BIGINT_PARITY`) plus the in-job provisioning shim, which
carry their own guarantee on any package state.

**Z.2 — Sequencing caution (`--clobber` races):** `gh release upload --clobber` mutates
release assets **in place**, and the Jul-27 incident proves consumers racing a refresh get
whatever asset is live at download time. The revert dispatch should therefore happen at a
quiet moment (no in-flight CI consuming `depcy` assets), and any job that was running across
the refresh should be re-run afterwards. Nothing breaks if the timing slips — the shim
provisions the headers regardless — but clean sequencing avoids confusing mixed-state runs.

**Z.3 — Post-revert sanity:** one `build_bdk.yaml` run on the pure packages must stay green
(the shim now does the provisioning), and a local native build on a freshly downloaded pure
package must show the loud, graceful `test_big_int_boost` skip — the intended permanent
behavior for local developers without multiprecision.

---

## 7. Validation gates (run only at the very end)

### Gate 1 — native regular build (all modules, wasm off)

Exact canonical command (kept verbatim; after the Owner Addendum 6 rename `BDK_BUILD_WASM` is
a live option again, and `-DBDK_BUILD_WASM=OFF` simply matches its default — no
unused-variable notice, no behavior change):

```bash
rm -fR /home/ctnguyen/development/bitcoin-sv/build/* && \
rm -fR /home/ctnguyen/development/bitcoin-sv/bdk/test/golang/vendor/* && \
cd /home/ctnguyen/development/bitcoin-sv/build && \
cmake ../bdk -DCMAKE_BUILD_TYPE=Release -DBDK_BUILD_WASM=OFF ; make -j8 && make test
```

Pass criteria:

- configure succeeds (`-DBDK_BUILD_WASM=OFF` matches the renamed option's default — no
  unused-variable notice, no remark); the local Boost must carry the multiprecision headers — a required
  native dependency (Owner Addendum 5); without them configure fails with the actionable
  §3.7 `FATAL_ERROR` (no skip mode);
- full build succeeds (core, gobdk, rustbdk, tests, `module/example` incl. the relocated
  `bench_verifyscript`) — in particular **no** `boost/multiprecision/cpp_int.hpp` error;
- `make test` passes (including `test_big_int_boost` — it always builds; no skip mode exists);
- `bench_verifyscript` exists in the build output — proving in particular that its
  `Boost::chrono` usage requirements resolved against the official prebuilt Boost (the
  `txvalidator.hpp` → `taskcancellation.h` → `boost/chrono.hpp` chain compiles) — and runs
  (`bench_verifyscript 10 2` is a sufficient smoke invocation; positional
  iterations/samples); the `bdk_core` compile lines do **not** carry `DISABLE_LOGGING`;
- sanity greps on the configure output: no mention of the removed mechanisms; exactly one
  secp256k1 sub-build.

Additional native check (cheap, fresh configure-only run):

- `cmake ../bdk -DBDK_BUILD_WASM=ON` (native toolchain) → fails with the Emscripten guard.

### Gate 2 — WASM standalone build

Owner's local pinned toolchain (Emscripten SDK 4.0.23, matching the PR's pin). The gate runs
against the **published wasm Boost package** (live on the `depcy` release since the P0.3
checkpoint), so it locally validates exactly the artifact CI consumes; the P0.1 loop install
(`build-wasm-deps/boost-wasm/include` — the dir containing `boost/` directly, §8 layout rule)
or `build.sh`'s own pinned minimal self-provisioning are
equivalent fallbacks — same component set either way:

```bash
source /home/ctnguyen/DevTools/emsdk_github/emsdk_env.sh
cd /home/ctnguyen/development/bitcoin-sv/bdk
curl --fail --location -o /tmp/dependancies_wasm.tar.gz \
  "https://github.com/bitcoin-sv/bdk/releases/download/depcy/dependancies_wasm.tar.gz"
tar -xzf /tmp/dependancies_wasm.tar.gz -C build-wasm-deps
BOOST_ROOT="$PWD/build-wasm-deps/dependancies_wasm/boost_1.85.0" module/typesbdk/wasm/build.sh
git status --short module/typesbdk/wasm/   # expect: clean (byte-identical artifacts)
```

Pass criteria:

- `build.sh` completes end-to-end: standalone configure (`-DBDK_BUILD_CORE=OFF
  -DBDK_BUILD_WASM=ON`), build of the four binding targets against `bdk_core_wasm`, `wasm-opt`
  pass, secp256k1 wasm test suites (`tests`, `noverify_tests`, `exhaustive_tests`), the 300 KB
  bundle ceilings, and the vector suites (`test.mjs`, `test.mjs bdk-core.browser.mjs`,
  `test-umd.mjs`, `test-umd.mjs bdk-core.slim.umd.js`);
- the eight committed artifacts are **byte-identical** (`git diff --exit-code` over
  `module/typesbdk/wasm/bdk-core*`) — the minimal-vs-full Boost install cannot affect bytes:
  the compiled headers are identical files and `-ffile-prefix-map=${BOOST_ROOT}=/boost`
  normalizes the location; if the diff is non-empty, apply §5.8 before touching any artifact;
- direct-configure smoke (protects the non-`build.sh` path). Note: `build.sh` resolves
  `BSV_ROOT` inside its own process only and exports nothing to the calling shell, so this
  command is self-contained — it uses the concrete default locations (`deps_dir` defaults to
  `<repo>/build-wasm-deps`; `BSV_ROOT=$deps_dir/bitcoin-sv`) and the same minimal Boost
  install:

```bash
cd /home/ctnguyen/development/bitcoin-sv/bdk
emcmake cmake -S . -B /tmp/bdk-wasm-direct -DCMAKE_BUILD_TYPE=Release \
  -DBDK_BUILD_CORE=OFF -DBDK_BUILD_WASM=ON \
  -DBSV_ROOT="$PWD/build-wasm-deps/bitcoin-sv" \
  -DBOOST_ROOT="$PWD/build-wasm-deps/dependancies_wasm/boost_1.85.0" \
  -DSECP256K1_ASM=OFF -DSECP256K1_BUILD_BENCHMARK=OFF \
  -DSECP256K1_ECMULT_WINDOW_SIZE=15 -DSECP256K1_ECMULT_GEN_KB=2 \
  -DSECP256K1_TEST_OVERRIDE_WIDE_MULTIPLY=int64
cmake --build /tmp/bdk-wasm-direct --target bdk_wasm
```

  (If `BDK_WASM_DEPS_DIR` or `BSV_ROOT` were overridden when running `build.sh`, substitute
  the same paths here.)

Order is mandatory: gate 1 first, gate 2 second; then the single fix commit if needed, then both
gates again. The native packages carry the P0.2 multiprecision headers — permanently, per
Owner Addendum 5 — so gate 1's parity suite always builds; there is no post-gate Phase-Z
revert or re-check anymore (Phase Z is cancelled, §6).

---

## 8. Prebuilt-dependencies plan

`.github/workflows/prebuild_dependancies.yaml` changes:

1. **Native package `multiprecision` [DONE in P0.2 — PERMANENT per Owner Addendum 5]:**
   `multiprecision` was appended to the native `BOOST_INCLUDE_LIBRARIES` list (line 57), so
   the refreshed `dependancies_<os>_<arch>.tar.gz` packages carry the headers. **Owner
   Addendum 5 declares this permanent**: Boost multiprecision is a required part of the
   native dependency contract (header-only, ~2-3 MB), the planned Phase-Z purity revert is
   cancelled, and parity enforcement is the §3.7 two-way configure gate — no CI flag, no
   in-job shim. A stale package without the headers fails configure loudly with the
   actionable message. *(The original bridge-then-revert model described here was superseded
   before Phase Z ever ran.)*
2. **New wasm Boost job with an OPTIMAL minimal component set
   [DERIVED AND IMPLEMENTED — awaiting the owner's P0.2 push].** Staging *all* Boost headers is
   rejected as far too large a package — no full-header staging exists anywhere in the final
   state (not in the prebuild job, not in `build.sh`, not in CI shims). The wasm package is
   built exactly like the native one — the Boost CMake superproject with a pinned
   `BOOST_INCLUDE_LIBRARIES` list, which auto-resolves inter-library header dependencies — and
   the list was derived by the following three-step procedure. **Phase 0 step P0.1 has been
   EXECUTED** (2026-07-27, Emscripten 4.0.23, Boost 1.85.0 cached tarball, bitcoin-sv at
   `879fc8b`; full results and per-component evidence in
   `.pr49-plan-context/wasm_boost_derivation_results.md`), converging on the **pinned
   8-component list**:

   ```
   WASM_BOOST_INCLUDE_LIBRARIES = multiprecision;chrono;uuid;variant;thread;filesystem;signals2;multi_index
   ```

   Validation of the converged install: clean `build.sh` run (exit 0), libsecp256k1 wasm suites
   passed, positive + corrupt transaction vectors passed on all four loaders, and the committed
   artifacts reproduced **byte-for-byte** (subject to the BOOST_ROOT layout rule below).
   Package weight: 72 MB of headers, `lib/` stripped.

   **(a) Static estimate — scan the wasm compile set for direct boost includes.** Performed
   against the real tree; the compile set = `BSV_MINIMAL_*` files minus the three wasm-excluded
   sources, plus the `core/` extras, plus `big_int_boost.cpp` / `memory_cleanse_wasm.cpp` /
   `txvalidator_wasm.cpp`. The scan pattern must match **both** include forms,
   `#include <boost/…>` and `#include "boost/…"` — the tree really uses both (`uint256.h`
   uses the quoted form, and an angle-bracket-only scan misses it):

   | Component | Pulled in by (verified in the tree) |
   |---|---|
   | `multiprecision` | `module/typesbdk/wasm/big_int_boost.cpp` (`boost/multiprecision/cpp_int.hpp`) |
   | `chrono` | `src/taskcancellation.h:13` (`boost/chrono.hpp`) |
   | `date_time` | `src/utiltime.h` (`boost/date_time/posix_time/posix_time.hpp`) |
   | `uuid` | `src/serialize.h` (`boost/uuid/uuid.hpp`) |
   | `variant` | `src/script/standard.h` (`boost/variant.hpp`), `src/base58.cpp` (`boost/variant/apply_visitor.hpp`, `static_visitor.hpp`) |
   | `container_hash` | `src/uint256.h:21` (`#include "boost/functional/hash.hpp"`, quoted form; `boost::hash_range` at line 243). `uint256.h` is squarely in the wasm closure: included directly by the minimal TUs `base58.cpp`, `arith_uint256.cpp`, `script/interpreter.cpp` and by the curated headers `hash.h`, `primitives/transaction.h`. In Boost ≥1.67 the `boost/functional/hash*.hpp` forwarders are owned by the `container_hash` library; if 1.85's modular layout ships them elsewhere (e.g. `functional`), step (b) corrects the name empirically. |

   Explicitly ruled OUT of the estimate, with tree evidence (candidates a broader scan might
   suggest):

   | Candidate | Why it is not in the wasm compile set |
   |---|---|
   | `signals2`, `thread` (via `src/util.h:35-36`) | `util.h`'s only pullers among curated sources are `core_write.cpp`/`core_read.cpp`/`util.cpp` — all in `_application_src_files` (`FindBSVSourceHelper.cmake:277-296`, `core_read.cpp`/`core_write.cpp` at lines 279–280), compiled solely into native test/example executables, never into a core variant. No file in the wasm closure includes `util.h` (verified over `logging.h`, `chainparams*.cpp`, `random.h`, `taskcancellation.h`, `streams.h`, `core_io.h`). |
   | `algorithm` (via `src/core_read.cpp:15-18`, `boost/algorithm/string/…`) | Same: `core_read.cpp` is application-list only. |

   The **initial estimate** was therefore
   `multiprecision;chrono;date_time;uuid;variant;container_hash` — explicitly an *estimate*:
   the static scan covers the TUs and curated headers plus known closure headers, not the full
   preprocessor closure. Step (b) is the authority; nothing gets pinned from (a) alone.

   **Post-execution reconciliation (what the loop proved the scan could not see):** the real
   preprocessor closure is larger than the curated lists suggest — the executed loop had to ADD
   `thread` (`src/sync.h:11`, `boost/thread/condition_variable.hpp`), `filesystem`
   (`src/fs.h:11`, `boost/filesystem.hpp`), `signals2` (`src/util.h:35`) and `multi_index`
   (`src/mining/journal.h:9`), i.e. `util.h`, `sync.h`, `fs.h` and `mining/journal.h` ARE
   reached transitively by the wasm compile set even though no curated file includes them
   directly (which is why the ruled-out table above, correct at the direct-include level, did
   not hold for the full closure). It also PRUNED `date_time` and `container_hash`: their
   headers arrive as transitive dependencies of the listed components, and no wasm-closure file
   includes them directly. `chrono` and `variant` are kept explicitly even though `thread` and
   `signals2` would auto-resolve them — closure headers include them DIRECTLY, and relying on
   undocumented Boost inter-library dependencies would be fragile (the strict 6-component list
   also builds green today; the 8-component list is the robust pin). This is the (a)-estimates /
   (b)-decides design working exactly as intended.

   **(b) Local add-and-prune loop — converge the list empirically. [EXECUTED — P0.1 done]**
   (Ran in the preparatory phase P0.1, §6 — implementation work BEFORE any commit; valid on the
   pre-refactor tree because the wasm Boost consumption is fixed by the compiled source set,
   which the refactor does not change, and the current `build.sh` already honors a pre-provided
   `BOOST_ROOT`. Results: `.pr49-plan-context/wasm_boost_derivation_results.md`.) The procedure,
   kept here as the canonical recipe for any future re-derivation:

   ```bash
   deps=/home/ctnguyen/development/bitcoin-sv/bdk/build-wasm-deps
   # 1. Download the Boost tarball ONCE; keep it cached — never re-download in the loop.
   curl --fail --location --retry 3 -o "$deps/boost-1.85.0-cmake.tar.gz" \
     "https://github.com/boostorg/boost/releases/download/boost-1.85.0/boost-1.85.0-cmake.tar.gz"
   tar -xzf "$deps/boost-1.85.0-cmake.tar.gz" -C "$deps"
   LIBS="multiprecision;chrono;date_time;uuid;variant;container_hash"   # step (a) estimate
   # 2. ADD phase: install with the candidate list, build wasm against it, extend on error.
   #    cmake -B "$deps/boost-build" -S "$deps/boost-1.85.0" \
   #      -DBOOST_INCLUDE_LIBRARIES="$LIBS" -DCMAKE_INSTALL_PREFIX="$deps/boost-wasm"
   #    cmake --build "$deps/boost-build" --target install -j8
   #    BOOST_ROOT="$deps/boost-wasm/include" module/typesbdk/wasm/build.sh
   #    ^ BOOST_ROOT must be the directory containing boost/ DIRECTLY (the
   #      install's include dir) — see the byte-reproducibility layout rule in (c).
   #    → on `fatal error: boost/<X>/...: No such file or directory`, map the path to
   #      its owning Boost library, append it to LIBS, wipe "$deps/boost-wasm", and
   #      re-run the cmake install + build (NO re-download, NO re-extract).
   #    Repeat until the wasm build passes.
   # 3. PRUNE phase: the add phase proves sufficiency, not necessity (the superproject
   #    auto-resolves dependencies, so one selected component may already deliver
   #    another's headers). For EACH component C in the passing list, one at a time:
   #      remove C → wipe "$deps/boost-wasm" → re-install → rebuild wasm;
   #      if still green, KEEP the removal; if it fails, restore C.
   #    Iterate over every component once (order: largest/most-suspect first).
   # The add-and-prune survivor list IS the optimal set — this exact string is what
   # gets pinned (step (c)); nothing else may be pinned.
   ```

   **(c) Pin the CONVERGED list — only after (b) has completed.
   [IMPLEMENTED in the working tree — the P0.2 change to `prebuild_dependancies.yaml` is
   authored (unstaged) and awaits the owner's direct push to `master` and the P0.3 CI
   verification.]** The workflow never carries an unvalidated estimate, and there is no "pin
   now, correct later" path. The job ships **only the headers** (the superproject also builds
   native static libs the wasm build never uses — the whole staging prefix, `lib/` included, is
   discarded after the `boost/` tree is extracted from it).

   **CRITICAL byte-reproducibility layout rule (empirically established, see the derivation
   results):** the wasm build embeds Boost exception `__FILE__` strings normalized by
   `-ffile-prefix-map=${BOOST_ROOT}=/boost`, and the committed artifacts were built with a
   source-layout Boost — headers at `$BOOST_ROOT/boost/…` → `/boost/boost/…`. An extra
   `include/` level (`$BOOST_ROOT/include/boost/…` → `/boost/include/boost/…`) produces
   DIFFERENT bytes (verified: 4 `.wasm` artifacts diverged, then matched again once
   `BOOST_ROOT` pointed at the prefix's `include` dir). Therefore **`BOOST_ROOT` must always
   point at the directory containing `boost/` DIRECTLY**, and the package is laid out so its
   inner directory does exactly that: the archive unpacks to
   `dependancies_wasm/boost_1.85.0/boost/…`, i.e.
   `BOOST_ROOT=<unpacked>/dependancies_wasm/boost_1.85.0` (mirroring the native
   package's `boost_$BOOST_VERSION` inner-directory convention).

   As implemented in `.github/workflows/prebuild_dependancies.yaml` (following the file's
   existing conventions — `PACKAGE_NAME` set via `$GITHUB_ENV`, same Boost download URL scheme,
   same `TAG_NAME="depcy"` `gh release upload --clobber` mechanism, same step structure):

```yaml
  build-upload-wasm-dependencies:
    # Platform-independent Boost header package for the standalone wasm build:
    # one ubuntu job is enough. NO OpenSSL here — the wasm verifier never links it.
    runs-on: ubuntu-22.04
    env:
      BOOST_VERSION : 1.85.0
      # Minimal component set for the wasm verifier, converged empirically with
      # the add-and-prune derivation procedure (...). To change this list,
      # re-run that procedure and pin its output — never hand-edit.
      WASM_BOOST_INCLUDE_LIBRARIES : "multiprecision;chrono;uuid;variant;thread;filesystem;signals2;multi_index"
    steps:
    - name: Check out repository
      uses: actions/checkout@v4
    - name: Set Environment Variables
      run: |
        PACKAGE_NAME="dependancies_wasm"
        ...PACKAGE_NAME / BOOST_URL / INSTALL_DIR /
        WASM_BOOST_DIR=${{ github.workspace }}/$PACKAGE_NAME/boost_$BOOST_VERSION >> $GITHUB_ENV
    - name: Prepare Environment            # apt build-essential g++ wget; mkdir -p $WASM_BOOST_DIR
    - name: Build minimal Boost headers using cmake
      run: |
        wget -q $BOOST_URL -O "boost-$BOOST_VERSION-cmake.tar.gz" && tar -xzf ...
        cmake -B "./boost-$BOOST_VERSION/build" -S "./boost-$BOOST_VERSION" \
          -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
          -DCMAKE_INSTALL_PREFIX="${{ github.workspace }}/wasm-boost-staging" \
          -DBOOST_INCLUDE_LIBRARIES="$WASM_BOOST_INCLUDE_LIBRARIES"
        cmake --build "./boost-$BOOST_VERSION/build" -j$(nproc) --target install
        # headers only + the BOOST_ROOT layout rule: package carries boost/ directly
        mv "${{ github.workspace }}/wasm-boost-staging/include/boost" "$WASM_BOOST_DIR/boost"
        rm -rf "${{ github.workspace }}/wasm-boost-staging"
    - name: Create archive                  # tar -czf "$PACKAGE_NAME.tar.gz" "$PACKAGE_NAME"
    - name: Upload artifact using github cli   # TAG_NAME="depcy"; gh release upload --clobber
```

   **CI validates the exact package:** `build_wasm.yaml` downloads
   `dependancies_wasm.tar.gz` from the `depcy` release and exports
   `BOOST_ROOT=<unpacked>/dependancies_wasm/boost_1.85.0` — the directory containing
   `boost/` directly, per the layout rule — before running `build.sh` (which already honors a
   pre-provided `BOOST_ROOT` and validates version 108500). Every CI wasm build therefore
   compiles against **exactly the pinned component set**: if a future source change adds a new
   boost include, CI fails with the missing-header error and the fix is a reviewed re-run of
   the §8(b) procedure whose output re-pins the list — it can never rot silently.

   **`build.sh` self-provisioning uses the SAME minimal-install mechanism** (there is no
   full-header staging anywhere): when no `BOOST_ROOT` is provided, `build.sh` downloads the
   pinned Boost tarball once into its cached deps dir (it already does), then — instead of the
   former `bootstrap.sh && b2 headers` full-header stage, which is **removed** — runs the same
   `cmake -DBOOST_INCLUDE_LIBRARIES=…` headers install as the prebuild job, using a
   `BDK_WASM_BOOST_LIBS` variable pinned in the script to the identical §8(b) survivor list
   (`multiprecision;chrono;uuid;variant;thread;filesystem;signals2;multi_index`), and uses the
   install's **`include` directory** as its internal Boost root (layout rule). Local
   development therefore exercises the exact component set CI enforces, and a local
   missing-header failure reproduces the CI failure one-to-one.
3. **Explicitly NO wasm OpenSSL prebuild.** The wasm verifier does not link OpenSSL at all —
   that is the entire size win. The workflow must not grow an Emscripten OpenSSL job, and the
   plan documents this as a deliberate non-goal.

**Ship-time guarantee [reworked by Owner Addendum 5]:** the parity suite always builds because
multiprecision is permanently part of the native packages; a stale package fails configure
loudly with the §3.7 message. The two mechanisms below are the original (superseded) design,
kept for the record: mechanism 1 (the in-job shim) was implemented in commit 5 and later
removed; mechanism 2 (the package refresh) is now the permanent state rather than a bridge.

1. **In-job self-sufficiency (the hard guarantee):** commit 5 adds a provisioning step to
   `build_bdk.yaml` immediately before the unix configure step. If the downloaded dependency
   package lacks the multiprecision headers (i.e. it predates the refresh), the job installs
   **only the multiprecision component** via the same minimal `BOOST_INCLUDE_LIBRARIES`
   mechanism (no full-header staging here either) and merges that install's include tree into
   the package (same Boost 1.85.0, so overlapping headers are byte-identical):

   ```yaml
         - name: Ensure Boost multiprecision headers (big-int parity suite)
           run: |
             if [ ! -f "$BOOST_ROOT/include/boost/multiprecision/cpp_int.hpp" ]; then
               curl --fail --location --retry 3 -o boost-src.tar.gz \
                 "https://github.com/boostorg/boost/releases/download/boost-1.85.0/boost-1.85.0-cmake.tar.gz"
               tar -xzf boost-src.tar.gz
               cmake -B boost-mp-build -S boost-1.85.0 \
                 -DBOOST_INCLUDE_LIBRARIES=multiprecision \
                 -DCMAKE_INSTALL_PREFIX="$PWD/boost-mp"
               cmake --build boost-mp-build --target install -j"$(nproc)"
               cp -R "$PWD/boost-mp/include/boost/." "$BOOST_ROOT/include/boost/"
             fi
   ```

   **[Superseded by Owner Addendum 5: this shim was implemented in commit 5, then REMOVED.]**
   With multiprecision permanent in the packages there is nothing to provision in-job; the
   two-way configure gate alone carries the guarantee, and a stale package fails loudly at
   configure. (The Jul-27 CI-race incident remains the recorded motivation for preferring
   code-side guarantees over racing asset state — the hard gate preserves that property.)
2. **Package refresh (Phase 0 — now the PERMANENT state per Owner Addendum 5):** the
   multiprecision addition landed on `master` in the preparatory commit P0.2 and the owner's
   workflow dispatch refreshed the `depcy` release assets (tag-level, shared by all branches;
   the addition is a pure superset, safe for concurrent `master` builds) — confirmed at the
   P0.3 human checkpoint **before the refactor PR exists**. Owner Addendum 5 made this the
   permanent contract: the planned Phase-Z revert is cancelled and the packages carry the
   headers indefinitely.

`build_wasm.yaml` consuming `dependancies_wasm.tar.gz` is **mandatory**, not an
optimization: it is the mechanism that validates the pinned minimal component set on every CI
run (§8 item 2c). The `bdk-wasm-deps` actions cache keeps only the bitcoin-sv checkout; the
Boost package comes from the `depcy` release.

---

## 9. WASM regression-test plan

Protections that already exist and are **kept** (verified in the current tree):

- real-transaction vector suites: `test.mjs` (Node + browser artifact), `test-umd.mjs` (UMD +
  slim UMD), with `test-suite.mjs` as the shared body — run by `build.sh` on every build;
- secp256k1 wasm suites: `tests`, `noverify_tests`, `exhaustive_tests` cross-compiled and run
  under Node with pinned seeds — run by `build.sh`;
- bundle-size ceilings (300 KB per loader+wasm pair) — enforced by `build.sh`;
- CI byte-compare of all eight committed artifacts + artifact upload —
  `build_wasm.yaml` steps "Verify committed artifacts are reproducible" / "Upload validated
  module" (workflow path triggers already cover `CMakeLists.txt`, `cmake/**`, `core/**`,
  `module/typesbdk/wasm/**`, so recipe changes retrigger it).

New coverage for the **new build shape**:

0. **Pinned minimal Boost package in CI** — `build_wasm.yaml` gains, before the `build.sh`
   step, a download of `dependancies_wasm.tar.gz` from the `depcy` release and an
   `BOOST_ROOT=$PWD/dependancies_wasm/boost_1.85.0` export into `$GITHUB_ENV` (the
   directory containing `boost/` directly — §8 layout rule). Every CI wasm build then
   compiles against exactly the pinned `BOOST_INCLUDE_LIBRARIES` set (§8 item 2c) — a new
   boost include anywhere in the compile set fails CI with a missing-header error, forcing a
   reviewed re-run of the §8(b) derivation.
1. **Standalone configure path in CI** — add to `build_wasm.yaml`, after the `build.sh` step
   (reusing its dependency directory and the same pinned Boost package):

```yaml
      - name: Standalone configure smoke (direct emcmake, no build.sh)
        run: |
          source "$RUNNER_TEMP/emsdk/emsdk_env.sh"
          emcmake cmake -S . -B build-wasm-direct -DCMAKE_BUILD_TYPE=Release \
            -DBDK_BUILD_CORE=OFF -DBDK_BUILD_WASM=ON \
            -DBSV_ROOT="$PWD/build-wasm-deps/bitcoin-sv" \
            -DBOOST_ROOT="$BOOST_ROOT" \
            -DSECP256K1_ASM=OFF -DSECP256K1_BUILD_BENCHMARK=OFF \
            -DSECP256K1_ECMULT_WINDOW_SIZE=15 -DSECP256K1_ECMULT_GEN_KB=2 \
            -DSECP256K1_TEST_OVERRIDE_WIDE_MULTIPLY=int64
          cmake --build build-wasm-direct --target bdk_wasm --parallel
```

   This proves the recipe/factory path works without `build.sh`'s environment and catches any
   future re-coupling of the wasm build to root-side state.
2. **Native job on the official prebuilt package proving the regression fix** — this is the
   existing `build_bdk.yaml` matrix (it provisions the `depcy` packages, builds all modules, runs
   `ctest`). After this refactor it passes again by construction; with multiprecision a
   required native dependency and the §3.7 two-way gate (Owner Addendum 5) it additionally
   proves the parity suite *runs* — a skip is impossible by construction.
3. **Guard check (cheap, optional):** a CI step asserting that
   `cmake -DBDK_BUILD_WASM=ON` without Emscripten fails configure with the expected message
   (`if cmake ...; then exit 1; fi` pattern). Optional — include if the workflow-time budget
   allows. (There is deliberately no check for `BDK_BUILD_WASM`: nothing reads it, stale usage
   is inert by design — §5.7.)

---

## 10. Documentation tasks

All docs must state the WHY, not just the HOW. Canonical paragraph (adapt per location):

> The WASM verifier requires aggressive size optimization to be deployable in browsers and SDKs:
> no OpenSSL (which forces substituting the OpenSSL-backed `big_int.cpp`/`random.cpp` with a
> Boost-based bigint backend and a wasm-safe cleanse), runtime-reconstructed secp256k1
> verification tables, and a minimal runtime. Threading those deviations through the shared build
> would deform the regular build architecture — core is the upstream-tracking trunk that
> gobdk/rustbdk link, and its build must stay canonical. The WASM build is therefore a fully
> standalone module build: it reuses the shared core *recipe*
> (`core/bdk-core-recipe.cmake`, the same curated bitcoin-sv source lists and
> `bdk_add_core_library()` factory that produce the native `bdk_core`) to assemble its own
> `bdk_core_wasm` variant inside the module directory, and the canonical `bdk_core` is never
> modified by any module.

Per-file tasks:

1. `documentation/docs/architecture.md` (§"The typesbdk WASM verifier", line ~150): replace the
   "opt-in root CMake target (`BDK_BUILD_WASM=ON`)" sentence with the standalone description and
   the WHY paragraph; mention `bdk_core_wasm` and the recipe file by path.
2. `documentation/docs/build.md`: update the options table (add `BDK_BUILD_CORE` and the
   module flag — executed as `BDK_BUILD_TYPES`, now `BDK_BUILD_WASM` per Owner Addendum 6);
   state that `BDK_BUILD_NATIVE_VERIFY_BENCHMARK` no longer exists and that `BDK_BUILD_WASM`
   now carries the standalone semantics (the commit-3-era "nothing reads the name" wording is
   superseded by the rename); document the standalone
   wasm invocation and that the big-int parity suite needs Boost multiprecision headers —
   **a required native dependency (Owner Addendum 5)**: the official packages carry them, and
   a Boost install without them fails configure with an actionable error (no flag, no skip);
   document the minimal wasm Boost package (`dependancies_wasm.tar.gz`, its pinned
   `BOOST_INCLUDE_LIBRARIES` set — which legitimately DOES include multiprecision — and the §8
   derivation procedure to follow when extending it).
3. `module/typesbdk/examples/README.md`: update every configure snippet to the new flags
   (`-DBDK_BUILD_CORE=OFF -DBDK_BUILD_WASM=ON` for wasm, post-rename); replace the dedicated
   native-benchmark configure block with "build normally, run `bench_verifyscript` from
   `module/example`", including the logging trade-off note (§3.6: the canonical logging-enabled
   core is the representative baseline; a throwaway `-DCMAKE_CXX_FLAGS=-DDISABLE_LOGGING` tree
   for logging-free measurements); state that `build.sh` remains the reproducible wasm entry
   point.
4. `core/bdk-core-recipe.cmake` header comment (part of commit 1, but it *is* documentation):
   the contract — prerequisites, who may call the factory, the four parameters, the
   no-parameter-creep rule (§5.3), and the single-instantiation rule for secp256k1 (§5.2).
4b. `module/gobdk/cgobench/README.md` (lands in commit 4 with the relocation): its C++-benchmark
   sections anticipate the `bench_verifyscript.cpp` file name but with a wrong directory depth —
   line 11 says `../../../example/bench_verifyscript.cpp`, which from `module/gobdk/cgobench/`
   resolves to repo-root `example/` (nonexistent); **correct it to
   `../../example/bench_verifyscript.cpp`** (`module/example/` is two levels up). The usage
   snippets are also stale: the binary takes positional `iterations` (default 1000) and
   `samples` (default 9) arguments and has **no** `-i`/`-c`/`--disable-consensus` flags; also
   fix the build command to `cmake --build <build> --target bench_verifyscript`. Keep the
   recorded benchmark result tables as historical data.
5. `ReleaseNote.md`: one entry describing the build-architecture change, the flag changes
   (the standalone module flag introduced — executed as `BDK_BUILD_TYPES`, now
   `BDK_BUILD_WASM` per Owner Addendum 6; `BDK_BUILD_NATIVE_VERIFY_BENCHMARK`
   removed; benchmark relocated to `module/example`) and the restored native-build guarantee on
   the official dependency package.

---

## 11. Risks and watch-items

| Risk | Mitigation |
|---|---|
| Artifact byte-drift from flag-scope changes | §5.8 procedure; byte-compare is a hard gate; regeneration only in the final fix commit |
| Hidden consumers of removed variables (`BDK_CORE_*`, `bdk_configure_secp256k1_targets`) | Grep-verified: the only references in the tree are the files being edited/deleted (root, BDKInit, core/CMakeLists, setting-secp256k1, the two override files, BDKCoreConfig) |
| Imported-target visibility regressions (OpenSSL) | §5.5 keeps discovery at root scope, gated on `BDK_BUILD_CORE` |
| `test_big_int_boost` silently skipped everywhere | Superseded by Owner Addendum 5: multiprecision is a required native dependency (permanently in the packages), the probe is a two-way hard gate — the suite always builds, a missing header is a configure `FATAL_ERROR`, and no skip mode exists |
| `depcy` release-asset races (`gh release upload --clobber` mutates assets in place) | Recorded incident 2026-07-27: parity jobs at 00:31Z downloaded pre-refresh packages and failed on missing `cpp_int.hpp`; re-runs after the ~02:2xZ refresh passed. Mitigations (updated for Owner Addendum 5): the packages permanently carry multiprecision and the Phase-Z revert dispatch is cancelled, so no further native-package mutation is planned; a stale asset now fails configure loudly with the actionable §3.7 message instead of being silently masked |
| A future second recipe consumer colliding on secp256k1/univalue/leveldb targets | Defensive TARGET guards (§5.2, §5.1, §5.10); gate-1 configure output check "exactly one secp256k1 sub-build" |
| Pinned wasm Boost component list goes stale (new boost include appears in the compile set) | Cannot rot silently: `build_wasm.yaml` builds against exactly the pinned `dependancies_wasm.tar.gz` package (§8 item 2c), so a missing component is a loud CI missing-header failure; the fix is a reviewed re-run of the §8(b) add-and-prune derivation whose output re-pins the list |
| §8(a) static estimate is wrong (missed transitive/config-gated include, or a superfluous component) | Structurally harmless: the estimate never reaches any commit — the P0.1 add-and-prune loop both extends (add phase) and minimizes (prune phase) it before anything is written, and the only two list-bearing artifacts (the P0.2 workflow pin on `master` and commit 2's `BDK_WASM_BOOST_LIBS` in `build.sh`) are both set verbatim to the converged survivor list |
| Stale `-DBDK_BUILD_WASM=...` invocations | **Updated by Owner Addendum 6 (the name is live again as the renamed standalone flag):** `=OFF` — the canonical command, itself gate 1 — matches the default and configures normally (no unused-variable notice anymore); `=ON` without Emscripten now fails fast with the standalone guard message instead of being silently ignored |
| macOS/Linux native paths (Darwin Boost define, GNU `sign.cpp` workaround) | Preserved verbatim inside the recipe/factory; the `build_bdk.yaml` matrix (ubuntu-22.04-arm, ubuntu-22.04, macos-15) exercises them |
| Windows native paths (MSVC coverage fix, Crypt32/Ws2_32, `/UHAVE_CONSENSUS_LIB`) | **Residual risk, unchanged from status quo:** the code paths are preserved verbatim, but `build_bdk.yaml`'s Windows steps (lines 152–167) are unreachable dead code — the job matrix has no Windows entry, and `prebuild_dependancies.yaml` does not even produce the Windows dependency package the dead steps download. This PR neither adds nor removes Windows CI (out of scope); the Windows configure line is left untouched. If Windows CI is ever revived, the §3.7 two-way gate applies there too — the Windows dependency package must then carry multiprecision (a required native dependency, Owner Addendum 5). |
