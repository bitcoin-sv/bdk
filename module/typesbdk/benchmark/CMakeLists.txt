add_executable(bdk_verify_benchmark verify_script_benchmark.cpp)
# The core's production logger is intentionally outside the BDK minimal source
# set. WASM disables these calls through __EMSCRIPTEN__; do the equivalent for
# this focused native benchmark and let the linker discard unused node helpers.
target_compile_definitions(bdk_core PRIVATE DISABLE_LOGGING)
target_link_libraries(bdk_verify_benchmark PRIVATE bdk_core)
target_compile_features(bdk_verify_benchmark PRIVATE cxx_std_20)
target_compile_options(bdk_verify_benchmark PRIVATE -O3)
if(APPLE)
  target_link_options(bdk_verify_benchmark PRIVATE "LINKER:-dead_strip")
elseif(UNIX)
  target_link_options(bdk_verify_benchmark PRIVATE "LINKER:--gc-sections")
endif()
