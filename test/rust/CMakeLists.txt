#################################################################
#  Independent Rust application-level tests                     #
#                                                               #
#  Mirrors test/golang: separate consumers that link ONLY the   #
#  merged libbdkffi archive produced by module/rustbdk (the     #
#  MergeBDKFFI target / BDKFFI_MERGED_ARCHIVE). If they build    #
#  and pass, the core archive is self-contained and usable      #
#  from an application — which is all module/rustbdk ships.      #
#################################################################

# --- C++ application smoke -------------------------------------------------
# A minimal C++ consumer of the bdkffi_ C ABI. It links ONLY the single merged
# archive; the third-party objects (secp256k1/OpenSSL/Boost/univalue) are inside
# it, so only the genuine system libs stay explicit. Linking one self-contained
# archive resolves the bdk_core <-> BSV-application references within that
# archive, so no link-group / per-linker special-casing is needed on any
# platform. Proves the merged core archive links and runs from a C++ app.
if(TARGET MergeBDKFFI AND BDKFFI_MERGED_ARCHIVE)
  find_package(Threads REQUIRED)
  add_executable(bdkffi_smoke ${CMAKE_CURRENT_SOURCE_DIR}/smoke.cpp)
  add_dependencies(bdkffi_smoke MergeBDKFFI)
  target_include_directories(bdkffi_smoke PRIVATE ${CMAKE_SOURCE_DIR}/module/rustbdk/capi/include)
  target_link_libraries(bdkffi_smoke PRIVATE
      ${BDKFFI_MERGED_ARCHIVE}
      Threads::Threads ${CMAKE_DL_LIBS} m)
  set_target_properties(bdkffi_smoke PROPERTIES FOLDER "test/rust" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
  add_test(NAME bdkffi_smoke COMMAND bdkffi_smoke)
else()
  message(STATUS "MergeBDKFFI / BDKFFI_MERGED_ARCHIVE not available; skipping C++ bdkffi_smoke")
endif()

# --- Rust linkage + behaviour suite ----------------------------------------
# A standalone Cargo package that consumes rust-bdk by path; bdk-sys/build.rs
# links the merged archive. If `cargo test` passes, the libbdkffi link closure
# has resolved from a Rust application. Pass the build dir so build.rs can find
# the archive in the build tree if the in-source install was disabled.
find_program(CARGO_EXECUTABLE NAMES cargo)
if(NOT CARGO_EXECUTABLE)
  message(WARNING "Missing cargo, skip building Rust tests")
  return()
endif()

add_test(NAME test_RustBDKLinkage
  COMMAND ${CARGO_EXECUTABLE} test --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
)
set_tests_properties(test_RustBDKLinkage PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  ENVIRONMENT "CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR};BDK_BUILD_DIR=${CMAKE_BINARY_DIR}")
