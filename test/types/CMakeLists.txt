#################################################################
#  Independent TypeScript/WASM application-level tests          #
#                                                               #
#  Mirrors test/golang and test/rust: a separate consumer of     #
#  what module/typesbdk/wasm ships. This directory owns EVERY    #
#  CTest registration for the wasm module; the module directory  #
#  owns only target definitions.                                #
#                                                               #
#  Added from the root CMakeLists.txt if(BDK_BUILD_WASM) block,  #
#  NOT from test/CMakeLists.txt: that file belongs to the native #
#  build (BDK_BUILD_CORE=ON) and requires                       #
#  Boost::unit_test_framework, neither of which exists in the    #
#  standalone Emscripten configure.                             #
#                                                               #
#  No build target is defined here. The wasm ALL target is       #
#  therefore byte-for-byte unaffected by this directory, and     #
#  test code cannot perturb the shipped artifacts.               #
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

# node is a TEST dependency, resolved here where it is used. It must be on PATH at
# CONFIGURE time or the ten node-based registrations below are simply absent (see
# README.md); build.sh does not preflight it, so CI asserts the twelve entries by
# name after configuring.
find_program(BDK_NODE_EXECUTABLE NAMES node NO_CMAKE_FIND_ROOT_PATH)

# Fixture: bring the optimized sidecars up to date before any functional/size test
# reads them, so a standalone `ctest` can never validate a raw/stale artifact.
# It is itself a CTest test, so it carries the wasm label and a generous timeout.
add_test(NAME wasm_build_optimized
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
    --target bdk_wasm_optimized bdk_wasm_browser_optimized
             bdk_wasm_umd_optimized bdk_wasm_slim_umd_optimized)
set_tests_properties(wasm_build_optimized PROPERTIES
  FIXTURES_SETUP wasm_optimized LABELS "wasm;fixture" TIMEOUT 1800)

if(BDK_NODE_EXECUTABLE)
  add_test(NAME wasm_functional_node
    COMMAND "${BDK_NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test.mjs"
            "${BDK_WASM_DIST_DIR}/bdk-core.mjs"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME wasm_functional_browser
    COMMAND "${BDK_NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test.mjs"
            "${BDK_WASM_DIST_DIR}/bdk-core.browser.mjs"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME wasm_functional_umd
    COMMAND "${BDK_NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test-umd.mjs"
            "${BDK_WASM_DIST_DIR}/bdk-core.umd.js"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME wasm_functional_slim_umd
    COMMAND "${BDK_NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test-umd.mjs"
            "${BDK_WASM_DIST_DIR}/bdk-core.slim.umd.js"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  set_tests_properties(
    wasm_functional_node wasm_functional_browser
    wasm_functional_umd wasm_functional_slim_umd
    PROPERTIES LABELS "wasm;functional" TIMEOUT 300
               FIXTURES_REQUIRED wasm_optimized)
else()
  message(WARNING
    "node not found: the wasm functional CTest tests were not registered. "
    "node must be on PATH at configure time; see test/types/README.md.")
endif()

# Size gate: sum loader+wasm per variant in decimal bytes, report EVERY oversized
# bundle and fail once. Generated into the build tree (no new tracked source);
# dist_dir and the ceiling are passed with -D so nothing reads CMakeCache.txt.
set(_wasm_size_gate_script "${CMAKE_CURRENT_BINARY_DIR}/wasm_size_gate.cmake")
file(WRITE "${_wasm_size_gate_script}" [==[
set(bundles
  "bdk-core:mjs" "bdk-core.browser:mjs" "bdk-core.umd:js" "bdk-core.slim.umd:js")
set(oversized "")
foreach(entry IN LISTS bundles)
  string(REPLACE ":" ";" parts "${entry}")
  list(GET parts 0 name)
  list(GET parts 1 loader)
  file(SIZE "${dist_dir}/${name}.${loader}" loader_bytes)
  file(SIZE "${dist_dir}/${name}.wasm" wasm_bytes)
  math(EXPR total "${loader_bytes} + ${wasm_bytes}")
  if(total GREATER max_bundle_bytes)
    message(WARNING "${name} is ${total} bytes; maximum is ${max_bundle_bytes}")
    list(APPEND oversized "${name}")
  endif()
endforeach()
if(oversized)
  string(REPLACE ";" " " oversized_str "${oversized}")
  message(FATAL_ERROR "Oversized wasm bundles (max ${max_bundle_bytes} bytes): ${oversized_str}")
endif()
]==])

add_test(NAME wasm_size_gate
  COMMAND "${CMAKE_COMMAND}"
    "-Ddist_dir=${BDK_WASM_DIST_DIR}" "-Dmax_bundle_bytes=300000"
    -P "${_wasm_size_gate_script}")
set_tests_properties(wasm_size_gate PROPERTIES
  LABELS "wasm;sizegate" TIMEOUT 60 FIXTURES_REQUIRED wasm_optimized)

# ---------------------------------------------------------------------------
# secp256k1 runtime-table parity tests. The WASM build reconstructs libsecp256k1's
# fixed-base verification tables at runtime instead of shipping the static const
# ones (see module/typesbdk/wasm/secp256k1_runtime.c and
# secp256k1_runtime_precomputed.c); these tests check that reconstruction. The
# suite executables they run are EXCLUDE_FROM_ALL targets defined by the module
# directory and built on demand by the fixture below, so nothing here is published
# and the eight committed artifacts and the size gate are unaffected.
# ---------------------------------------------------------------------------

# Iteration count forwarded to the suites (argv[1] of tests.c main). Higher values
# deepen the randomised coverage at the cost of runtime; overridable from the cache.
set(WASM_SECP256K1_TEST_ITERS "4" CACHE STRING
  "Iteration count for the WASM secp256k1 unit-suite parity tests.")

if(BDK_NODE_EXECUTABLE)
  # Fixture: build the two suite executables (and, transitively, the patched
  # tests.c) before either suite test reads them.
  add_test(NAME wasm_secp256k1_build
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
      --target wasm_secp256k1_tests wasm_secp256k1_noverify_tests)
  set_tests_properties(wasm_secp256k1_build PROPERTIES
    FIXTURES_SETUP wasm_secp256k1_build LABELS "wasm;secp256k1;fixture" TIMEOUT 3600)

  # $<TARGET_FILE:...> resolves to each suite's emitted .js wherever the build's
  # global runtime-output directory places it (Emscripten locates the sibling .wasm
  # relative to the .js, so no working directory is needed). Targets are global, so
  # the expression resolves from this directory even though the module directory
  # defines them.
  add_test(NAME wasm_secp256k1_tests
    COMMAND "${BDK_NODE_EXECUTABLE}"
      "$<TARGET_FILE:wasm_secp256k1_tests>" "${WASM_SECP256K1_TEST_ITERS}")
  add_test(NAME wasm_secp256k1_noverify_tests
    COMMAND "${BDK_NODE_EXECUTABLE}"
      "$<TARGET_FILE:wasm_secp256k1_noverify_tests>" "${WASM_SECP256K1_TEST_ITERS}")
  set_tests_properties(wasm_secp256k1_tests wasm_secp256k1_noverify_tests PROPERTIES
    LABELS "wasm;secp256k1;parity" TIMEOUT 3600
    FIXTURES_REQUIRED wasm_secp256k1_build)

  # (2) First-call tweak-add regression. bdk_tweak_public_key_add reaches the
  #     fixed-base tables but does not itself prime them, so on a fresh instance it
  #     returns a wrong point (see module/typesbdk/wasm/txvalidator_wasm.cpp and
  #     test-first-call.mjs's header). The script itself inverts the exit semantics
  #     (green only when every invariant holds AND the known bug reproduces; red when
  #     the bug is fixed or anything else breaks), so WILL_FAIL is deliberately NOT
  #     used -- it would mask real setup/prepared-path regressions.
  add_test(NAME wasm_secp256k1_first_call_parity
    COMMAND "${BDK_NODE_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/test-first-call.mjs" "${BDK_WASM_DIST_DIR}/bdk-core.mjs"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  set_tests_properties(wasm_secp256k1_first_call_parity PROPERTIES
    LABELS "wasm;parity;functional" TIMEOUT 300
    FIXTURES_REQUIRED wasm_optimized)

  # (3) Whole-pipeline verification parity over a shared corpus. The corpus fixture
  #     signs a spread of edge and deterministic-random key/digest inputs with the
  #     shipped module and records the definitional verdicts (valid must verify,
  #     tampered must be rejected); the parity test confirms the module verifies them
  #     through the substituted tables. See verify-corpus-gen.mjs's header for scope.
  add_test(NAME wasm_secp256k1_verify_corpus
    COMMAND "${BDK_NODE_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/verify-corpus-gen.mjs"
      "${BDK_WASM_DIST_DIR}/bdk-core.mjs"
      "${CMAKE_CURRENT_BINARY_DIR}/wasm_secp256k1_corpus.json"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  set_tests_properties(wasm_secp256k1_verify_corpus PROPERTIES
    FIXTURES_SETUP wasm_secp256k1_verify_corpus LABELS "wasm;secp256k1;fixture"
    TIMEOUT 300 FIXTURES_REQUIRED wasm_optimized)

  add_test(NAME wasm_secp256k1_verify_parity_node
    COMMAND "${BDK_NODE_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/test-verify-parity.mjs"
      "${BDK_WASM_DIST_DIR}/bdk-core.mjs"
      "${CMAKE_CURRENT_BINARY_DIR}/wasm_secp256k1_corpus.json"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  set_tests_properties(wasm_secp256k1_verify_parity_node PROPERTIES
    LABELS "wasm;parity;functional" TIMEOUT 300
    FIXTURES_REQUIRED "wasm_optimized;wasm_secp256k1_verify_corpus")

  # The independent native-oracle leg (wasm_secp256k1_verify_parity_native) is not
  # registered here. The wasm module's CMake project is Emscripten-only (it aborts
  # under a non-Emscripten toolchain), so it produces no native binary to act as an
  # oracle, and the test loop does not run separate native builds. The corpus above
  # instead grounds its expected verdicts in RFC 6979 deterministic signatures
  # (valid -> accept, tampered -> reject), which still exercises the substituted
  # verification tables end to end. A native cross-check can be added when a native
  # build harness is available in the same tree.
else()
  message(WARNING
    "node not found: the wasm secp256k1 parity CTest tests were not registered. "
    "node must be on PATH at configure time; see test/types/README.md.")
endif()
