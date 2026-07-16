if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "The BDK WASM target requires Emscripten")
endif()

add_executable(bdk_wasm txvalidator_wasm.cpp txvalidator_wasm.h)
target_link_libraries(bdk_wasm PRIVATE bdk_core)
target_compile_features(bdk_wasm PRIVATE cxx_std_20)
target_compile_options(bdk_wasm PRIVATE -O3 -fexceptions)
target_link_options(bdk_wasm PRIVATE
  --bind
  -O3
  -fexceptions
  "SHELL:-s MODULARIZE=1"
  "SHELL:-s EXPORT_ES6=1"
  "SHELL:-s EXPORT_NAME=createBdkModule"
  "SHELL:-s ENVIRONMENT=web,worker,node"
  "SHELL:-s ALLOW_MEMORY_GROWTH=1"
  "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
  "SHELL:-s ASSERTIONS=0"
  "SHELL:-s FILESYSTEM=0"
)

set_target_properties(bdk_wasm PROPERTIES
  OUTPUT_NAME "bdk-core"
  SUFFIX ".mjs"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}/dist"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/dist"
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_BINARY_DIR}/dist"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_BINARY_DIR}/dist"
)
