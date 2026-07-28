if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "The BDK WASM target requires Emscripten")
endif()
if(BDK_BUILD_CORE)
  message(FATAL_ERROR "The WASM module is standalone: configure with -DBDK_BUILD_CORE=OFF")
endif()

# Boost: header-only import, module-owned. Enforcing the pinned version here
# gives a direct emcmake configure (not routed through build.sh) the same
# reproducibility guarantee build.sh provides.
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
