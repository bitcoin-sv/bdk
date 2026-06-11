#################################################################
#  Date             10/06/2026                                  #
#  Author           nChain's employees                          #
#                                                               #
#  Copyright (c) 2026 nChain Limited. All rights reserved       #
#################################################################

find_package(Threads)
include(FindBoostHelper)
HelpFindBoost(thread filesystem program_options chrono)

# Keep this in sync with rust-bdk's Cargo "bench" feature. CMake controls
# which symbols enter libbdkffi, while Cargo controls which bench targets build;
# the two build systems cannot enforce this coupling mechanically.
option(BDKFFI_ENABLE_BENCH "Build benchmark-only bdkffi C ABI helpers" OFF)

foreach(comp IN ITEMS thread filesystem program_options chrono)
    if(NOT TARGET Boost::${comp})
        message(FATAL_ERROR "Boost::${comp} is required but not found")
    endif()
endforeach()

createIncrementVersion(BDK_RUST_VERSION_MAJOR ${BDK_VERSION_MAJOR} 0 "BDK Rust major version")
createIncrementVersion(BDK_RUST_VERSION_MINOR ${BDK_VERSION_MINOR} 0 "BDK Rust minor version")
createIncrementVersion(BDK_RUST_VERSION_PATCH ${BDK_VERSION_PATCH} 0 "BDK Rust patch version")
set(BDK_RUST_VERSION_STRING "${BDK_RUST_VERSION_MAJOR}.${BDK_RUST_VERSION_MINOR}.${BDK_RUST_VERSION_PATCH}")

set(BDK_VERSION_RUST_CPP_IN ${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp.in CACHE INTERNAL "Template file for Rust C ABI version")
set(BDK_VERSION_RUST_CPP ${BDK_GENERATED_CPP_DIR}/rustbdk/capi/version.cpp CACHE INTERNAL "Source file for Rust C ABI version")
get_filename_component(BDK_VERSION_RUST_CPP_DIR "${BDK_VERSION_RUST_CPP}" DIRECTORY)
file(MAKE_DIRECTORY "${BDK_VERSION_RUST_CPP_DIR}")
configure_file(${BDK_VERSION_RUST_CPP_IN} ${BDK_VERSION_RUST_CPP} @ONLY)

set(_BDKFFI_HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bdkffi/alloc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bdkffi/asm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bdkffi/bdkffi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bdkffi/error_strings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bdkffi/txerror.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bdkffi/txvalidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bdkffi/validatebatch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/bdkffi/version.h
    "${BSV_APPLICATION_HDR_FILES}"
)

set(_BDKFFI_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/alloc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/asm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/error_strings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/txvalidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/validatebatch.cpp
    ${BDK_VERSION_RUST_CPP}
    "${BSV_APPLICATION_SRC_FILES}"
)

if(BDKFFI_ENABLE_BENCH)
    list(APPEND _BDKFFI_HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/bdkffi/bench.h)
    list(APPEND _BDKFFI_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/bench.cpp)
endif()

set(_BDKFFI_UNIVALUE_LIB)
if(BDK_BUILD_UNIVALUE)
    set(_BDKFFI_UNIVALUE_LIB univalue)
endif()

add_library(static-bdkffi STATIC ${_BDKFFI_HEADER_FILES} ${_BDKFFI_SOURCE_FILES})
target_link_libraries(static-bdkffi PUBLIC
    bdk_core secp256k1 ${_BDKFFI_UNIVALUE_LIB}
    OpenSSL::Crypto OpenSSL::SSL
    Boost::thread Boost::filesystem Boost::program_options Boost::chrono
    ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(static-bdkffi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR})
if(BDKFFI_ENABLE_BENCH)
    target_compile_definitions(static-bdkffi PRIVATE BDKFFI_ENABLE_BENCH)
endif()
set_target_properties(static-bdkffi PROPERTIES FOLDER "module/rustbdk" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
source_group("_generated" FILES "${BDK_VERSION_RUST_CPP}")

if(NOT DEFINED BUILD_TESTING OR BUILD_TESTING)
    add_executable(bdkffi_smoke ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke.cpp)
    target_link_libraries(bdkffi_smoke PRIVATE static-bdkffi)
    target_include_directories(bdkffi_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(bdkffi_smoke PROPERTIES FOLDER "module/rustbdk/test" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
    add_test(NAME bdkffi_smoke COMMAND bdkffi_smoke)

    if(CARGO_EXECUTABLE)
        # Precondition: a prior cmake --build of BUILD_MODULE_RUST must produce libbdkffi, otherwise build.rs link discovery fails; README coverage is Stage 8.
        add_test(
            NAME rustbdk_cargo_test
            COMMAND "${CARGO_EXECUTABLE}" test --manifest-path module/rustbdk/Cargo.toml
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        set_tests_properties(rustbdk_cargo_test PROPERTIES
            ENVIRONMENT "CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR};BDK_BUILD_DIR=${CMAKE_BINARY_DIR}")
    else()
        add_test(
            NAME rustbdk_cargo_test
            COMMAND ${CMAKE_COMMAND} -E echo "cargo not found: skipping rustbdk cargo tests")
        set_tests_properties(rustbdk_cargo_test PROPERTIES
            SKIP_REGULAR_EXPRESSION "cargo not found")
    endif()
endif()

install(TARGETS static-bdkffi DESTINATION "lib" COMPONENT RustModules)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/bdkffi DESTINATION "include" COMPONENT RustModules FILES_MATCHING PATTERN "*.h")

if(UNIX)
    set(listArchive)

    foreach(impTarget IN ITEMS OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono)
        get_property(vImported TARGET ${impTarget} PROPERTY IMPORTED)
        if(${vImported})
            get_property(vLocation TARGET ${impTarget} PROPERTY LOCATION)
            list(APPEND listArchive ${vLocation})
        else()
            message(FATAL_ERROR "${impTarget} is not an imported target")
        endif()
    endforeach()

    set(buildtype "release")
    set(debugPostfix "")
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        set(buildtype "debug")
        set(debugPostfix "d")
    endif()
    set(buildDir ${CMAKE_BINARY_DIR}/x${BDK_SYSTEM_BUILD_ARCHI}/${buildtype})

    set(_BDKFFI_BUILD_TARGETS secp256k1 bdk_core static-bdkffi)
    if(BDK_BUILD_UNIVALUE)
        list(APPEND _BDKFFI_BUILD_TARGETS univalue)
    endif()
    foreach(buildTarget IN LISTS _BDKFFI_BUILD_TARGETS)
        set(buildFile "${buildDir}/lib${buildTarget}${debugPostfix}.a")
        list(APPEND listArchive ${buildFile})
    endforeach()

    foreach(archive IN LISTS listArchive)
        get_filename_component(archiveName ${archive} NAME_WE)
        set(objDir "${buildDir}/merge_bdkffi/${archiveName}")
        file(MAKE_DIRECTORY "${objDir}")

        add_custom_command(TARGET static-bdkffi POST_BUILD
            COMMAND ${CMAKE_AR} x ${archive}
            WORKING_DIRECTORY "${objDir}"
            COMMENT "Extract object files from [${archive}]")
    endforeach()

    if(DEFINED CUSTOM_BDKFFI_OS_ARCH)
        set(OsArch "${CUSTOM_BDKFFI_OS_ARCH}")
        message("OsArch=${CUSTOM_BDKFFI_OS_ARCH} was used by custom value")
    else()
        set(OsArch "${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}")
        message("OsArch=${OsArch} used by default values read from the system")
    endif()
    string(TOLOWER ${OsArch} os_arch)
    set(mergedArchive "libbdkffi_${os_arch}.a")

    add_custom_target(MergeBDKFFI ALL
        COMMAND ${CMAKE_AR} rcs "${mergedArchive}" "${buildDir}/merge_bdkffi/*/*.o"
        DEPENDS static-bdkffi
        WORKING_DIRECTORY ${buildDir}
        COMMENT "Merge all in one ${mergedArchive}")

    install(FILES "${buildDir}/${mergedArchive}" DESTINATION "lib/bdkffi" COMPONENT RustModules)

    if(BUILD_MODULE_RUST_INSTALL_INSOURCE AND CARGO_EXECUTABLE)
        add_custom_command(TARGET MergeBDKFFI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/module/rustbdk/bdk-sys/lib"
            COMMAND ${CMAKE_COMMAND} -E copy "${buildDir}/${mergedArchive}" "${CMAKE_SOURCE_DIR}/module/rustbdk/bdk-sys/lib/${mergedArchive}"
            COMMENT "Install insource ${mergedArchive}")
    elseif(BUILD_MODULE_RUST_INSTALL_INSOURCE)
        message(STATUS "cargo not found: skipping Rust module install-insource copy")
    endif()
endif()
