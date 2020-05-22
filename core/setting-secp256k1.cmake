#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## Build secp256k1 requires locating bsv source code
if(NOT DEFINED SCRYPT_BSV_ROOT_DIR)#
    message(FATAL_ERROR "Unable to locate bsv source code by SCRYPT_BSV_ROOT_DIR")
endif()
# This file is mainly copied from ${SCRYPT_BSV_ROOT_DIR}/sv/src/secp256k1/CMakeLists.txt with slightl modifications
#######################################################################


if(NOT MSVC)
  ## secp256k1 use a different set of compilation flags
  scrypt_add_compiler_flag(-pedantic -Wshadow -Wno-unused-function -Wno-nonnull -Wno-overlength-strings)
  scrypt_add_c_compiler_flag(-std=c89 -Wno-long-long)
endif()

# Default visibility is hidden on all targets.
set(CMAKE_C_VISIBILITY_PRESET hidden)

## Configure build constants ##############################################
# We don't actually need GMP
set(USE_NUM_NONE 1)
set(USE_FIELD_INV_BUILTIN 1)
set(USE_SCALAR_INV_BUILTIN 1)

# We check if amd64 asm is supported.
check_c_source_compiles("
    #include <stdint.h>
    int main() {
        uint64_t a = 11, tmp;
        __asm__ __volatile__(\"movq \$0x100000000,%1; mulq %%rsi\" : \"+a\"(a) : \"S\"(tmp) : \"cc\", \"%rdx\");
        return 0;
    }
" USE_ASM_X86_64)

# We make sure __int128 is defined
include(CheckTypeSize)
check_type_size(__int128 SIZEOF___INT128)
if(SIZEOF___INT128 EQUAL 16)
    set(HAVE___INT128 1)
else()
    # If we do not support __int128, we should be falling back
    # on 32bits implementations for field and scalar.
endif()

if (MSVC)
    # MSVC compiler does not support inline assembly nor __int128. Use C implementation, but fail with error for other compilers
    set(USE_FIELD_10X26 1)
    set(USE_SCALAR_8X32 1)
else()
    # Detect if we are on a 32 or 64 bits plateform and chose
    # scalar and filed implementation accordingly
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # 64 bits implementationr require either __int128 or asm support.
        if (HAVE___INT128 OR USE_ASM_X86_64)
            set(USE_SCALAR_4X64 1)
            set(USE_FIELD_5X52 1)
        else()
            message(SEND_ERROR "Compiler does not support __int128 or insline assembly")
        endif()
    else()
        set(USE_SCALAR_8X32 1)
        set(USE_FIELD_10X26 1)
    endif()
endif()

## Executable internal to secp256k1 need to have the HAVE_CONFIG_H define set.
## For convenience, we wrap this into a function.
## TODO : might require latter
function(link_secp256k1_internal NAME)
  target_link_libraries(${NAME} secp256k1)
  #target_compile_definitions(${NAME} PRIVATE HAVE_CONFIG_H SECP256K1_BUILD)
  target_compile_definitions(${NAME} PRIVATE HAVE_CONFIG_H)
endfunction(link_secp256k1_internal)

################################################################################
## Build secp256k1 libraries ###################################################
set(SECP256K1_ROOT "${SCRYPT_BSV_ROOT_DIR}/src/secp256k1")
set(SECP256K1_CPP_FILE "${SECP256K1_ROOT}/src/secp256k1.c")
set(SECP256K1_PUBLIC_HEADER_DIR "${SECP256K1_ROOT}/include")

# Generate the config
set(LIBSECP256K1_CONFIG_FILE_IN "${SECP256K1_ROOT}/src/libsecp256k1-config.h.cmake.in")
set(LIBSECP256K1_CONFIG_FILE "${SCRYPT_GENERATED_HPP_DIR}/libsecp256k1-config.h")
configure_file(${LIBSECP256K1_CONFIG_FILE_IN} ${LIBSECP256K1_CONFIG_FILE} ESCAPE_QUOTES)

### Install and nice presentation in IDE
file(GLOB_RECURSE SECP256K1_PUBLIC_HEADERS "${SECP256K1_PUBLIC_HEADER_DIR}/*.h") # TO INSTALL
file(GLOB SECP256K1_PRIVATE_HEADERS "${SECP256K1_ROOT}/src/*.h")
#SECP256K1_CPP_FILE

add_library(secp256k1 "${SECP256K1_CPP_FILE}" "${LIBSECP256K1_CONFIG_FILE}" ${SECP256K1_PUBLIC_HEADERS} ${SECP256K1_PRIVATE_HEADERS} ${LIBSECP256K1_CONFIG_FILE})
target_include_directories(secp256k1 PRIVATE "${SECP256K1_ROOT}" "${SECP256K1_ROOT}/src")
target_include_directories(secp256k1 INTERFACE "${SECP256K1_PUBLIC_HEADER_DIR}")
#target_compile_definitions(secp256k1 PRIVATE HAVE_CONFIG_H SECP256K1_BUILD)
target_compile_definitions(secp256k1 PRIVATE HAVE_CONFIG_H ENABLE_MODULE_ECDH ENABLE_MODULE_MULTISET ENABLE_MODULE_RECOVERY)
set_property(TARGET secp256k1 PROPERTY FOLDER "core")

## Log list of secp256k1 source files
message(STATUS "Build secp256k1 source code in ${SECP256K1_ROOT}")
foreach(_secp256k1_file ${SECP256K1_PUBLIC_HEADERS} ${SECP256K1_PRIVATE_HEADERS} ${SECP256K1_CPP_FILE})
  if(SCRYPT_LOG_BSV_FILES)
    message(STATUS "    [${_secp256k1_file}]")
  endif()
  ## Set the nice structure in IDE
  get_filename_component(_file_ext "${_secp256k1_file}" EXT)
  if(${_file_ext} MATCHES ".cpp" OR ${_file_ext} MATCHES ".c")
    source_group(TREE ${SECP256K1_ROOT} PREFIX "bitcoin SRC" FILES "${_secp256k1_file}")
  else()
    source_group(TREE ${SECP256K1_ROOT} PREFIX "bitcoin HDR" FILES "${_secp256k1_file}")
  endif()
endforeach()
source_group("_generated" FILES "${LIBSECP256K1_CONFIG_FILE}")
###############################################

#######################################################################################################################
## Install secp256k1 the *.h header files should be kept as secp256k1 structure    ####################################
install(FILES ${LIBSECP256K1_CONFIG_FILE} DESTINATION "include/secp256k1" COMPONENT secp256k1)
foreach(_secp256k1_pubhdr ${SECP256K1_PUBLIC_HEADERS})
  install(FILES ${_secp256k1_pubhdr} DESTINATION "include/secp256k1/include" COMPONENT secp256k1)
endforeach()
install(TARGETS secp256k1 DESTINATION "lib" COMPONENT secp256k1)
###############################################


if(NOT MSVC)
  ## Remove specific compiler flags added at the begining of this file
  scrypt_remove_compiler_flags(-pedantic -Wshadow -Wno-unused-function -Wno-nonnull -Wno-overlength-strings -std=c89 -Wno-long-long)
endif()
