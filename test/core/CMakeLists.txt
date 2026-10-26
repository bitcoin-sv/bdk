#################################################################
#  Date             23/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

include(FindBoostHelper)
HelpFindBoost(thread filesystem program_options chrono)
foreach(comp IN LISTS "filesystem" "chrono" "thread" "program_options")
    if(NOT TARGET Boost::${comp})
        message(FATAL "Boost::${comp} is required but not found")
    endif()
endforeach()

## Test setting requires locating bsv source code
if(NOT DEFINED BDK_BSV_ROOT_DIR)#
  message(FATAL_ERROR "Unable to locate bsv source code by BDK_BSV_ROOT_DIR")
endif()

if(NOT Boost_FOUND)
  message(WARNING "Unable to find boost for C++ unit test, skip testing core")
  return()
endif()

include(FindPythonHelper)
HelpFindPython()
#bdkPrintPythonInfo()#Debug Log

if(NOT (Python_FOUND AND Python_EXECUTABLE AND Python_Development_FOUND))
    message(WARNING "Unable to find Python on the system which is required to generate test files")
    return()
endif()

# This file is mainly copied from ${BDK_BSV_ROOT_DIR}/sv/src/test/CMakeLists.txt with slightl modifications
#######################################################################
message("Building BSV tests from ${BDK_BSV_ROOT_DIR} ...")

set(BDK_BSV_TEST_ROOT "${BDK_BSV_ROOT_DIR}/src/test")
set(BDK_BSV_TESTSUITE_MODULE "${BDK_BSV_ROOT_DIR}/cmake/modules/TestSuite.cmake")
set(BDK_BSV_TEST_HEADER_GENERATOR "${BDK_BSV_TEST_ROOT}/data/generate_header.py")

function(gen_json_header NAME)
  set(HEADERS "")
  foreach(f ${ARGN})
    set(_h_file "${BDK_GENERATED_HPP_DIR}/${f}.h")
    set(_json_file "${BDK_BSV_TEST_ROOT}/${f}")
    # Get the proper name for the test variable.
    get_filename_component(TEST_NAME ${f} NAME_WE)

    add_custom_command(
      OUTPUT ${_h_file}
      COMMAND ${Python_EXECUTABLE}
      ARGS "${BDK_BSV_TEST_HEADER_GENERATOR}" "${TEST_NAME}" "${_json_file}" > ${_h_file}
      MAIN_DEPENDENCY ${_json_file}
      DEPENDS "${BDK_BSV_TEST_HEADER_GENERATOR}"
      VERBATIM
    )

    list(APPEND HEADERS ${_h_file})
  endforeach(f)
  set(${NAME} "${HEADERS}" PARENT_SCOPE)
endfunction()

## Generate directory containing generated json header file
file(MAKE_DIRECTORY "${BDK_GENERATED_HPP_DIR}/data")

gen_json_header(JSON_HEADERS
  data/script_tests.json
  data/base58_keys_valid.json
  data/base58_encode_decode.json
  data/base58_keys_invalid.json
  data/tx_invalid.json
  data/tx_valid.json
  data/sighash.json
)
message(STATUS "JSON_HEADERS generated for BSV tests")
foreach(_f ${JSON_HEADERS})
  if(BDK_LOG_BSV_FILES)
    message("       [${_f}]")
  endif()
endforeach()

####################################################################################################################
## Test files *cpp here are mainly extract of bsv source code ######################################################

message(status "File ${BDK_BSV_ROOT_DIR}/src/logging.cpp initially included in core, but as it depend to Boost::filesytem")
message(status "we remove it from core and put it where it is needed only")

## first-party sources get the warning set (+ -Werror), bitcoin-sv sources keep upstream's
bdkApplySourceWarningPolicy()

add_executable(test_bsv
               "${BDK_BSV_TEST_ROOT}/jsonutil.h"
               "${BDK_BSV_TEST_ROOT}/scriptflags.h"
               "${MODIFIED_SCRIPTFLAGS_H}"
               "${BDK_BSV_TEST_ROOT}/sigutil.h"
               "${BDK_BSV_TEST_ROOT}/jsonutil.cpp"
               "${BDK_BSV_TEST_ROOT}/scriptflags.cpp"
               "${BDK_BSV_TEST_ROOT}/sigutil.cpp"
               ${JSON_HEADERS}
               "${CMAKE_CURRENT_SOURCE_DIR}/script_tests_modified.cpp"
               "${BDK_BSV_TEST_ROOT}/bn_op_tests.cpp"
#               "${BDK_BSV_TEST_ROOT}/config_tests.cpp"
               "${BDK_BSV_TEST_ROOT}/scriptflags.cpp"
               "${BDK_BSV_TEST_ROOT}/scriptnum_tests.cpp"
               "${BSV_APPLICATION_HDR_FILES}"
               "${BSV_APPLICATION_SRC_FILES}"
               "${BDK_BSV_ROOT_DIR}/src/script/scriptcache.cpp"
               "${BDK_BSV_ROOT_DIR}/src/script/sigcache.cpp"
               #"${BDK_BSV_TEST_ROOT}/opcode_tests.cpp
)

target_link_libraries(test_bsv PRIVATE bdk_core univalue OpenSSL::Crypto OpenSSL::SSL ${CMAKE_DL_LIBS}  ${CMAKE_THREAD_LIBS_INIT})
bdkLinkTargetToBoost(test_bsv Boost::thread Boost::chrono Boost::filesystem Boost::program_options)
if(WIN32)
  target_link_libraries(test_bsv PRIVATE Crypt32.lib Ws2_32)
endif()
target_include_directories(test_bsv PRIVATE "${BDK_BSV_TEST_ROOT}")

# Ensure HAVE_CONSENSUS_LIB is not defined for test_bsv
if(MSVC)
    target_compile_options(test_bsv PRIVATE "/UHAVE_CONSENSUS_LIB")
else()
    target_compile_options(test_bsv PRIVATE "-UHAVE_CONSENSUS_LIB")
endif()

bdk_add_unit_test(test_bsv)

####################################################################################
## Test for the bdk core part ######################################################
message("Building BDK core tests ...")

add_executable(test_assembler "${CMAKE_CURRENT_SOURCE_DIR}/test_assembler.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(test_assembler PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_assembler)

add_executable(test_extended_tx "${CMAKE_CURRENT_SOURCE_DIR}/test_extended_tx.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(test_extended_tx PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_extended_tx)

add_executable(test_script_error "${CMAKE_CURRENT_SOURCE_DIR}/test_script_error.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(test_script_error PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_script_error)

add_executable(test_big_int "${CMAKE_CURRENT_SOURCE_DIR}/test_big_int.cpp")
target_link_libraries(test_big_int PRIVATE bdk_core)
bdk_add_unit_test(test_big_int)

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
target_include_directories(test_big_int_boost SYSTEM PRIVATE ${BSV_INCLUDE_DIRS})
target_link_libraries(test_big_int_boost PRIVATE Boost::boost)
target_compile_features(test_big_int_boost PRIVATE cxx_std_20)
bdk_add_unit_test(test_big_int_boost)

add_executable(test_txvalidator "${CMAKE_CURRENT_SOURCE_DIR}/test_txvalidator.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(test_txvalidator PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_txvalidator)
add_executable(test_txvalidator_internal "${CMAKE_CURRENT_SOURCE_DIR}/test_txvalidator_internal.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(test_txvalidator_internal PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_txvalidator_internal)
add_test(NAME test_txvalidator_pool_creation_retry
         COMMAND $<TARGET_FILE:test_txvalidator> --report_level=detailed
                 --run_test=test_txvalidator/pool_creation_failure_is_retried
                 -- --input-pool-cores=4)
add_test(NAME test_txvalidator_pool_below_two_cores
         COMMAND $<TARGET_FILE:test_txvalidator> --report_level=detailed
                 --run_test=test_txvalidator/pool_below_two_cores_is_not_retried
                 -- --input-pool-cores=1)
add_test(NAME test_txvalidator_pool_without_helper_threads
         COMMAND $<TARGET_FILE:test_txvalidator> --report_level=detailed
                 --run_test=test_txvalidator/pool_without_helper_threads_is_not_retried
                 -- --input-pool-cores=4)
## The same suite with every multi-input transaction verified on several threads,
## on a pool of at most 4 cores (CI-sized, on any machine)
add_test(NAME test_txvalidator_parallel_inputs
         COMMAND $<TARGET_FILE:test_txvalidator> --report_level=detailed -- --parallel-input-threshold=2 --input-pool-cores=4)
## Keep the helper hand-off regression cheap to repeat, including on 3-core CI.
add_test(NAME test_txvalidator_helper_release
         COMMAND $<TARGET_FILE:test_txvalidator> --report_level=detailed
                 --run_test=test_txvalidator/parallel_input_verification/leaving_helper_wakes_another_for_a_queued_job
                 -- --input-pool-cores=3)
## Coordinate release before publication, independently of the queued-job case.
add_test(NAME test_txvalidator_release_before_offer
         COMMAND $<TARGET_FILE:test_txvalidator> --report_level=detailed
                 --run_test=test_txvalidator/parallel_input_verification/release_before_job_publication_recruits_a_helper
                 -- --input-pool-cores=4)
## A hang in the input pool fails the run instead of waiting for the CI job timeout
get_directory_property(_bdk_core_tests TESTS)
foreach(_t IN LISTS _bdk_core_tests)
  if(_t MATCHES "^test_txvalidator")
    set_tests_properties(${_t} PROPERTIES TIMEOUT 600)
  endif()
endforeach()

add_executable(test_chronicle "${CMAKE_CURRENT_SOURCE_DIR}/test_chronicle.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(test_chronicle PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_chronicle)

add_executable(test_sigopcount "${CMAKE_CURRENT_SOURCE_DIR}/test_sigopcount.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(test_sigopcount PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_sigopcount)

add_executable(test_cancellation "${CMAKE_CURRENT_SOURCE_DIR}/test_cancellation.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(test_cancellation PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
bdk_add_unit_test(test_cancellation)

## Tests labelled `tsan` also run in the ThreadSanitizer CI job
## (.github/workflows/tsan.yaml), which builds bdk_tsan_tests and runs `ctest -L tsan`.
## To cover another test there: give it the label and add its target here.
## bdk_add_unit_test names the test test_cancellationd in a single-config Debug build.
foreach(_t test_cancellation test_cancellationd)
  if(TEST ${_t})
    set_tests_properties(${_t} PROPERTIES LABELS tsan)
  endif()
endforeach()
add_custom_target(bdk_tsan_tests)
## Keep wall-clock cache-performance assertions in the native suite; TSAN
## instruments the complete parallel-input suite through its own entry.
add_test(NAME test_txvalidator_parallel_inputs_tsan
         COMMAND $<TARGET_FILE:test_txvalidator> --report_level=detailed
                 --run_test=test_txvalidator/parallel_input_verification
                 -- --parallel-input-threshold=2 --input-pool-cores=4)
set_tests_properties(test_txvalidator_parallel_inputs_tsan PROPERTIES LABELS tsan TIMEOUT 600)
add_dependencies(bdk_tsan_tests test_cancellation test_txvalidator test_txvalidator_internal)
foreach(_t test_txvalidator_internal test_txvalidator_internald)
  if(TEST ${_t})
    set_tests_properties(${_t} PROPERTIES LABELS tsan TIMEOUT 600)
  endif()
endforeach()
add_executable(test_sha256_backends "${CMAKE_CURRENT_SOURCE_DIR}/test_sha256_backends.cpp")
target_link_libraries(test_sha256_backends PRIVATE bdk_core OpenSSL::Crypto)
bdk_add_unit_test(test_sha256_backends)

add_executable(test_sha256_select_race "${CMAKE_CURRENT_SOURCE_DIR}/test_sha256_select_race.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(test_sha256_select_race PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
bdk_add_unit_test(test_sha256_select_race)

## CPU count and cgroup limit, read from fixture trees under data/cgroup
add_executable(test_cpu_limit "${CMAKE_CURRENT_SOURCE_DIR}/test_cpu_limit.cpp")
target_link_libraries(test_cpu_limit PRIVATE bdk_core)
target_compile_definitions(test_cpu_limit PRIVATE BDK_CGROUP_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/cgroup")
bdk_add_unit_test(test_cpu_limit)

## The same suites again with the scalar SHA-256 transforms forced, so the
## fallback is proven on every runner as well as the hardware backend
foreach(_t test_sha256_backends test_sha256_select_race test_txvalidator test_bsv)
  add_test(NAME ${_t}_scalar COMMAND $<TARGET_FILE:${_t}> --report_format=HRF --report_level=detailed
           --log_format=JUNIT --log_sink=$<TARGET_FILE:${_t}>_scalar.xml)
  set_tests_properties(${_t}_scalar PROPERTIES ENVIRONMENT "BDK_SHA256_FORCE_SCALAR=1" TIMEOUT 600)
endforeach()
## Check the installed backend after each constructor, each in its own process.
## Load-time selection is guarded by the race test and Go/Rust at-load tests.
foreach(_case validator_by_network_has_sha256_backend:network_has_backend
              validator_by_chain_name_has_sha256_backend:chain_name_has_backend)
  string(REPLACE ":" ";" _parts "${_case}")
  list(GET _parts 0 _case_name)
  list(GET _parts 1 _suffix)
  add_test(NAME test_txvalidator_${_suffix}
           COMMAND $<TARGET_FILE:test_txvalidator> --run_test=test_txvalidator/${_case_name})
  set_tests_properties(test_txvalidator_${_suffix} PROPERTIES TIMEOUT 120)
endforeach()

## Hard stops for the new timed and multi-threaded suites
foreach(_t test_sha256_backends test_sha256_backendsd test_sha256_select_race test_sha256_select_raced)
  if(TEST ${_t})
    set_tests_properties(${_t} PROPERTIES TIMEOUT 300)
  endif()
endforeach()

if(CMAKE_CONFIGURATION_TYPES)
  set_target_properties(test_bsv PROPERTIES FOLDER "test/core/bsv" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_assembler PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_extended_tx PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_script_error PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_big_int PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")
  set_target_properties(test_big_int_boost PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")
  set_target_properties(test_txvalidator PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_txvalidator_internal PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")
  set_target_properties(test_chronicle PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_sigopcount PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_cancellation PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
  set_target_properties(test_sha256_backends PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")
  set_target_properties(test_sha256_select_race PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")
  set_target_properties(test_cpu_limit PROPERTIES FOLDER "test/core" DEBUG_POSTFIX "_d")
else()
  set_target_properties(test_bsv PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_assembler PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_extended_tx PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_script_error PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_big_int PROPERTIES DEBUG_POSTFIX "")
  set_target_properties(test_big_int_boost PROPERTIES DEBUG_POSTFIX "")
  set_target_properties(test_txvalidator PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_txvalidator_internal PROPERTIES DEBUG_POSTFIX "")
  set_target_properties(test_chronicle PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_sigopcount PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_cancellation PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
  set_target_properties(test_sha256_backends PROPERTIES DEBUG_POSTFIX "")
  set_target_properties(test_sha256_select_race PROPERTIES DEBUG_POSTFIX "")
  set_target_properties(test_cpu_limit PROPERTIES DEBUG_POSTFIX "")
endif()

## Timing tests in test_txvalidator are disabled by default and labelled
## `timing` in Boost; this entry runs only them. They skip themselves in a
## non-optimised (no NDEBUG) build, and run serially so a parallel ctest
## doesn't load the machine under them. `ctest -L timing` runs just these,
## `ctest -LE timing` everything else.
add_test(NAME test_txvalidator_timing
         COMMAND $<TARGET_FILE:test_txvalidator> --run_test=@timing --report_level=detailed)
set_tests_properties(test_txvalidator_timing PROPERTIES LABELS timing RUN_SERIAL TRUE TIMEOUT 600)
