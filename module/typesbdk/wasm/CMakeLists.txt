if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "The BDK WASM target requires Emscripten")
endif()
if(BDK_BUILD_CORE)
  message(FATAL_ERROR "The WASM module is standalone: configure with -DBDK_BUILD_CORE=OFF")
endif()

# Boost: header-only import, module-owned. The pinned version is enforced here,
# in the build system, so every build path -- the build.sh facility script and a
# direct emcmake configure alike -- shares one reproducibility contract.
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
      "Set BOOST_ROOT to a prepared wasm Boost 1.85.0 package: the prebuilt depcy "
      "package (dependancies_wasm.tar.gz), or a local Boost 1.85.0 install whose "
      "directory contains boost/ directly or under include/.")
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
  # Byte-repro layout rule: the prefix map below must anchor on the directory
  # containing boost/ DIRECTLY, even when the caller supplied an install prefix.
  set(BOOST_ROOT "${BDK_BOOST_INCLUDE_DIR}")
endif()

# Error strings from Boost and BSV can contain `__FILE__`. Normalize the most
# specific roots first so artifacts are byte-identical across checkout paths.
# Directory scope covers every target created below: the core variant, the
# secp256k1 sub-build and the bindings.
add_compile_options(
  "-ffile-prefix-map=${BDK_BSV_ROOT_DIR}=/bitcoin-sv"
  "-ffile-prefix-map=${BOOST_ROOT}=/boost"
  "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=/bdk"
)

# secp256k1 curve suites: build them in-tree as CTest tests (was a second CMake
# project in build.sh). Guarded so a command-line -DSECP256K1_BUILD_TESTS=OFF can
# skip the build cost; default ON keeps "a green build implies green tests".
# These MUST precede the recipe include: the recipe includes setting-secp256k1.cmake
# at file scope, so setting them after the include is silently ineffective.
if(NOT DEFINED SECP256K1_BUILD_TESTS)
  set(SECP256K1_BUILD_TESTS ON)
endif()
if(NOT DEFINED SECP256K1_BUILD_EXHAUSTIVE_TESTS)
  set(SECP256K1_BUILD_EXHAUSTIVE_TESTS ON)
endif()
set(SECP256K1_BUILD_BENCHMARK OFF)   # wasm: no benchmarks (the old sub-project set this too)

# The module's own core variant, built from the shared recipe. The recipe
# instantiates the secp256k1 sub-build under this directory's binary dir.
include("${CMAKE_SOURCE_DIR}/core/bdk-core-recipe.cmake")
bdk_add_core_library(bdk_core_wasm
  EXCLUDE_BSV_SOURCES
    "${BDK_BSV_ROOT_DIR}/src/big_int.cpp"          # OpenSSL BIGNUM, unbuildable without OpenSSL
    "${BDK_BSV_ROOT_DIR}/src/random.cpp"           # OpenSSL RAND
    "${BDK_BSV_ROOT_DIR}/src/support/cleanse.cpp"  # replaced by the wasm-safe cleanse
  ADDITIONAL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/big_int_boost.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/memory_cleanse_wasm.cpp"
  COMPILE_DEFINITIONS BOOST_HAS_PTHREADS DISABLE_LOGGING)

# Reconstruct the large fixed-base verification table at runtime while
# preserving the full W15 hot path and the compact signing table. The recipe
# instantiated secp256k1 from this directory's scope, so the module rewires
# targets it owns.
set_property(TARGET secp256k1_precomputed PROPERTY SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/secp256k1_runtime_precomputed.c"
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/src/precomputed_ecmult_gen.c"
)
target_include_directories(secp256k1_precomputed PRIVATE
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/src"
)
target_compile_options(secp256k1_precomputed PRIVATE -O3)
set_property(TARGET secp256k1_precomputed
  PROPERTY INTERPROCEDURAL_OPTIMIZATION OFF
)

target_include_directories(secp256k1 PRIVATE
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/include"
  "${BDK_BSV_ROOT_DIR}/src/secp256k1/src"
)
target_compile_options(secp256k1 PRIVATE -O3)
get_target_property(_secp256k1_sources secp256k1 SOURCES)
list(REMOVE_ITEM _secp256k1_sources "secp256k1.c")
list(PREPEND _secp256k1_sources
  "${CMAKE_CURRENT_SOURCE_DIR}/secp256k1_runtime.c"
)
set_property(TARGET secp256k1 PROPERTY SOURCES ${_secp256k1_sources})

# Per-target link options for the in-tree secp256k1 curve suites only. The old
# sub-project passed these via a project-wide CMAKE_EXE_LINKER_FLAGS; in-tree that
# would leak onto the module's own four targets and CHANGE the shipped artifact
# bytes. Scope them to the three test targets so the committed artifacts are byte
# identical (R3). Targets are global, so this wasm scope reaches the sub-build's
# targets, exactly as it already rewires secp256k1/secp256k1_precomputed above.
foreach(_secp_test tests noverify_tests exhaustive_tests)
  if(TARGET ${_secp_test})
    target_link_options(${_secp_test} PRIVATE
      "SHELL:-s STACK_SIZE=8388608" "SHELL:-s ALLOW_MEMORY_GROWTH=1")
  endif()
endforeach()

# wasm-opt resolution mirrors the facility script exactly:
#   1. a non-empty, existing WASM_OPT override
#   2. else wasm-opt on PATH ONLY (a fresh `command -v`-equivalent probe)
#   3. else $EMSDK/upstream/bin/wasm-opt
# NOTE: after `source emsdk_env.sh`, wasm-opt is NOT on PATH and $EMSDK/upstream/bin
# is NOT added to PATH; only the binary exists on disk, so step 3 resolves it.
set(_bdk_wasm_opt "")

# (1) Explicit override -- honoured only when non-empty AND present. build.sh uses
#     `${WASM_OPT:-}` (a non-empty test); EXISTS (not an executable-bit check) is
#     sufficient because a bogus override fails loudly at build time when the
#     optimise edge invokes it -- never a silent artifact-correctness problem.
if(DEFINED ENV{WASM_OPT} AND NOT "$ENV{WASM_OPT}" STREQUAL "")
  if(EXISTS "$ENV{WASM_OPT}")
    set(_bdk_wasm_opt "$ENV{WASM_OPT}")
  else()
    message(FATAL_ERROR "WASM_OPT=[$ENV{WASM_OPT}] is set but does not exist")
  endif()
endif()

# (2) wasm-opt on PATH ONLY, re-probed fresh each configure like `command -v`.
#     unset drops any prior hit so a reconfigure never reuses a stale path;
#     PATHS ENV PATH + NO_DEFAULT_PATH searches only the environment PATH dirs
#     (a bare find_program would consult CMake-specific program dirs first).
if(_bdk_wasm_opt STREQUAL "")
  unset(_bdk_wasm_opt_probe CACHE)
  find_program(_bdk_wasm_opt_probe NAMES wasm-opt
    PATHS ENV PATH NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
  if(_bdk_wasm_opt_probe)
    set(_bdk_wasm_opt "${_bdk_wasm_opt_probe}")
  endif()
endif()

# (3) The Emscripten SDK's bundled binary (the path that actually resolves).
if(_bdk_wasm_opt STREQUAL "" AND DEFINED ENV{EMSDK}
   AND EXISTS "$ENV{EMSDK}/upstream/bin/wasm-opt")
  set(_bdk_wasm_opt "$ENV{EMSDK}/upstream/bin/wasm-opt")
endif()

if(_bdk_wasm_opt STREQUAL "" OR NOT EXISTS "${_bdk_wasm_opt}")
  message(FATAL_ERROR
    "wasm-opt not found. Set WASM_OPT, put wasm-opt on PATH, or activate the "
    "pinned Emscripten SDK so $EMSDK/upstream/bin/wasm-opt exists.")
endif()
set(BDK_WASM_OPT "${_bdk_wasm_opt}")   # NORMAL variable: never written to / read from the cache

# The dist directory is RUNTIME_OUTPUT_DIRECTORY for every wasm target (set in
# add_bdk_wasm_target). Define it once so add_custom_command(OUTPUT ...) needs no
# generator expressions (not portable on the 3.16 floor).
set(BDK_WASM_DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")

function(add_bdk_wasm_target target_name output_name environment export_es6 suffix include_compat)
  add_executable(${target_name} txvalidator_wasm.cpp txvalidator_wasm.h txvalidator_wasm.js)
  # The WASM binding includes BDK headers that transitively include Boost.
  # Keep this usage requirement on the binding target rather than widening
  # the core variant's public native link interface.
  target_link_libraries(${target_name} PRIVATE bdk_core_wasm Boost::boost)
  target_include_directories(${target_name} PRIVATE
    "${BDK_BSV_ROOT_DIR}/src/secp256k1/include"
  )
  target_compile_features(${target_name} PRIVATE cxx_std_20)
  target_compile_options(${target_name} PRIVATE -O3 -fexceptions)
  set(exported_functions
    "'_malloc','_free','_bdk_prepare_verification','_bdk_prepare_signing','_bdk_verify_script','_bdk_verify_script_batch','_bdk_verify_spend','_bdk_verify_spend_batch','_bdk_sign_digest','_bdk_verify_digest','_bdk_verify_digest_batch','_bdk_public_key_from_private','_bdk_multiply_public_key','_bdk_tweak_public_key_add','_bdk_tweak_private_key_add'"
  )
  if(include_compat)
    string(APPEND exported_functions
      ",'_bdk_verification_table_snapshot_size','_bdk_export_verification_tables','_bdk_import_verification_tables','_bdk_verify_script_main'"
    )
  endif()
  target_link_options(${target_name} PRIVATE
    -O3
    -fexceptions
    "SHELL:--post-js ${CMAKE_CURRENT_SOURCE_DIR}/txvalidator_wasm.js"
    "SHELL:-s EXPORTED_FUNCTIONS=[${exported_functions}]"
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_ES6=${export_es6}"
    "SHELL:-s EXPORT_NAME=createBdkModule"
    "SHELL:-s ENVIRONMENT=${environment}"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
    "SHELL:-s MALLOC=emmalloc"
    "SHELL:-s ASSERTIONS=0"
    "SHELL:-s FILESYSTEM=0"
  )
  if(include_compat)
    target_sources(${target_name} PRIVATE
      txvalidator_wasm_compat.js
      txvalidator_wasm_snapshot.js
    )
    target_link_options(${target_name} PRIVATE
      "SHELL:--post-js ${CMAKE_CURRENT_SOURCE_DIR}/txvalidator_wasm_snapshot.js"
      "SHELL:--post-js ${CMAKE_CURRENT_SOURCE_DIR}/txvalidator_wasm_compat.js"
    )
  endif()

  set_target_properties(${target_name} PROPERTIES
    OUTPUT_NAME "${output_name}"
    SUFFIX "${suffix}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}/dist"
  )

  set(_canon "${BDK_WASM_DIST_DIR}/${output_name}.wasm")
  set(_raw   "${BDK_WASM_DIST_DIR}/${output_name}.raw.wasm")
  set(_stamp "${CMAKE_CURRENT_BINARY_DIR}/${output_name}.wasm.optimized.stamp")

  # (1) Snapshot Emscripten's freshly emitted, UN-optimized sidecar on every
  #     relink. Plain copy (not copy_if_different): a relink always overwrites the
  #     canonical .wasm with raw bytes, so the optimize edge must re-run afterwards
  #     even when the raw bytes are unchanged.
  add_custom_command(TARGET ${target_name} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "${_canon}" "${_raw}"
    BYPRODUCTS "${_raw}"
    VERBATIM
    COMMENT "capture raw ${output_name}.wasm")

  # (2) Tracked optimize edge: PRISTINE raw snapshot -> canonical .wasm.
  #     Reads _raw (never _canon), so it is idempotent (no double-optimize hazard,
  #     no need to force-relink); _canon is a declared OUTPUT, so a deleted/stale
  #     canonical .wasm is regenerated; _stamp is the mono-writer timestamp that
  #     makes "re-run after any relink" reliable.
  add_custom_command(
    OUTPUT "${_canon}" "${_stamp}"
    DEPENDS ${target_name} "${_raw}"
    COMMAND "${BDK_WASM_OPT}" "${_raw}"
      -O4 --converge
      --enable-bulk-memory --enable-bulk-memory-opt
      --enable-nontrapping-float-to-int --enable-sign-ext --enable-mutable-globals
      -o "${_canon}.opt"
    COMMAND "${CMAKE_COMMAND}" -E rename "${_canon}.opt" "${_canon}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${_stamp}"
    VERBATIM
    COMMENT "wasm-opt ${output_name}.wasm")

  # (3) An ALL target so a plain `cmake --build` reconciles the sidecar.
  add_custom_target(${target_name}_optimized ALL DEPENDS "${_stamp}")
endfunction()

# Keep platform-specific loader glue separate. Browser bundlers should never
# have to parse Node's dynamic `module` import, while Node retains its direct
# filesystem loader and both artifacts expose the exact same verifier ABI.
add_bdk_wasm_target(bdk_wasm "bdk-core" "node" 1 ".mjs" TRUE)
add_bdk_wasm_target(bdk_wasm_browser "bdk-core.browser" "web,worker" 1 ".mjs" TRUE)
add_bdk_wasm_target(bdk_wasm_umd "bdk-core.umd" "web,worker" 0 ".js" TRUE)
add_bdk_wasm_target(
  bdk_wasm_slim_umd "bdk-core.slim.umd" "web,worker" 0 ".js" FALSE
)
target_link_options(bdk_wasm_slim_umd PRIVATE
  "SHELL:-s MINIMAL_RUNTIME=1"
  "SHELL:--extern-pre-js ${CMAKE_CURRENT_SOURCE_DIR}/txvalidator_wasm_minimal_extern.js"
  "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/txvalidator_wasm_minimal_pre.js"
)
target_sources(bdk_wasm_slim_umd PRIVATE
  txvalidator_wasm_minimal_extern.js
  txvalidator_wasm_minimal_pre.js
)

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
    "build.sh preflights node before configuring, so its validated path always "
    "registers them.")
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

# In-source publish of the 8 committed artifacts. NOT in ALL: naming the target IS
# the opt-in, so no option() variable is needed and a plain build never publishes.
# copy_if_different is portable and leaves mtimes untouched when bytes are already
# identical (byte-repro friendly). DEPENDS the optimise targets so a direct
# invocation still gets final bytes. NOT install() -- the standalone configure
# deliberately emits zero install rules (root CMakeLists.txt:62-75).
add_custom_target(bdk_wasm_install_insource
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BDK_WASM_DIST_DIR}/bdk-core.mjs"          "${CMAKE_CURRENT_SOURCE_DIR}/bdk-core.mjs"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BDK_WASM_DIST_DIR}/bdk-core.wasm"         "${CMAKE_CURRENT_SOURCE_DIR}/bdk-core.wasm"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BDK_WASM_DIST_DIR}/bdk-core.browser.mjs"  "${CMAKE_CURRENT_SOURCE_DIR}/bdk-core.browser.mjs"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BDK_WASM_DIST_DIR}/bdk-core.browser.wasm" "${CMAKE_CURRENT_SOURCE_DIR}/bdk-core.browser.wasm"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BDK_WASM_DIST_DIR}/bdk-core.umd.js"       "${CMAKE_CURRENT_SOURCE_DIR}/bdk-core.umd.js"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BDK_WASM_DIST_DIR}/bdk-core.umd.wasm"      "${CMAKE_CURRENT_SOURCE_DIR}/bdk-core.umd.wasm"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BDK_WASM_DIST_DIR}/bdk-core.slim.umd.js"  "${CMAKE_CURRENT_SOURCE_DIR}/bdk-core.slim.umd.js"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${BDK_WASM_DIST_DIR}/bdk-core.slim.umd.wasm" "${CMAKE_CURRENT_SOURCE_DIR}/bdk-core.slim.umd.wasm"
  DEPENDS bdk_wasm_optimized bdk_wasm_browser_optimized
          bdk_wasm_umd_optimized bdk_wasm_slim_umd_optimized
  COMMENT "Publish the 8 committed wasm artifacts into the source tree")
