# The WASM module owns every deviation from the canonical native core build.
# Keeping the profile here makes core's source list and target setup independent
# of any particular language binding.
if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "The BDK WASM target requires Emscripten")
endif()

set(BDK_WASM_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(BDK_CORE_FIND_OPENSSL OFF)
set(BDK_CORE_LINK_LIBRARIES)
set(BDK_CORE_EXCLUDED_BSV_SOURCES
  "${BDK_BSV_ROOT_DIR}/src/big_int.cpp"
  "${BDK_BSV_ROOT_DIR}/src/random.cpp"
  "${BDK_BSV_ROOT_DIR}/src/support/cleanse.cpp"
)
set(BDK_CORE_ADDITIONAL_SOURCES
  "${BDK_WASM_SOURCE_DIR}/big_int_boost.cpp"
  "${BDK_WASM_SOURCE_DIR}/memory_cleanse_wasm.cpp"
)
set(BDK_CORE_PRIVATE_COMPILE_DEFINITIONS
  BOOST_HAS_PTHREADS
  DISABLE_LOGGING
)

# Error strings from Boost and BSV can contain `__FILE__`. Normalize the most
# specific roots first so artifacts are byte-identical across checkout paths.
add_compile_options(
  "-ffile-prefix-map=${BDK_BSV_ROOT_DIR}=/bitcoin-sv"
  "-ffile-prefix-map=${BOOST_ROOT}=/boost"
  "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=/bdk"
)

function(bdk_configure_secp256k1_targets)
  # Reconstruct the large fixed-base verification table at runtime while
  # preserving the full W15 hot path and the compact signing table.
  set_property(TARGET secp256k1_precomputed PROPERTY SOURCES
    "${BDK_WASM_SOURCE_DIR}/secp256k1_runtime_precomputed.c"
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
    "${BDK_WASM_SOURCE_DIR}/secp256k1_runtime.c"
  )
  set_property(TARGET secp256k1 PROPERTY SOURCES ${_secp256k1_sources})
endfunction()
