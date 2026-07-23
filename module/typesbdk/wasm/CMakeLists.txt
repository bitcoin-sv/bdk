if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "The BDK WASM target requires Emscripten")
endif()

function(add_bdk_wasm_target target_name output_name environment export_es6 suffix)
  add_executable(${target_name} txvalidator_wasm.cpp txvalidator_wasm.h txvalidator_wasm.js)
  # The WASM binding includes BDK headers that transitively include Boost.
  # Keep this usage requirement on the binding target rather than widening
  # bdk_core's public native link interface.
  target_link_libraries(${target_name} PRIVATE bdk_core Boost::boost)
  target_compile_features(${target_name} PRIVATE cxx_std_20)
  target_compile_options(${target_name} PRIVATE -O3 -fexceptions)
  target_link_options(${target_name} PRIVATE
    -O3
    -fexceptions
    "SHELL:--post-js ${CMAKE_CURRENT_SOURCE_DIR}/txvalidator_wasm.js"
    "SHELL:-s EXPORTED_FUNCTIONS=['_malloc','_free','_bdk_verify_script_main','_bdk_verify_script','_bdk_verify_script_batch','_bdk_verify_spend','_bdk_verify_spend_batch']"
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
add_bdk_wasm_target(bdk_wasm "bdk-core" "node" 1 ".mjs")
add_bdk_wasm_target(bdk_wasm_browser "bdk-core.browser" "web,worker" 1 ".mjs")
add_bdk_wasm_target(bdk_wasm_umd "bdk-core.umd" "web,worker" 0 ".js")
