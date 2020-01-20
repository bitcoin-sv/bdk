#################################################################
#  Date             20/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## Build secp256k1 requires locating bsv source code
if(NOT DEFINED SCRYPT_BSV_SRC_ROOT)#
    message(FATAL_ERROR "Unable to locate bsv source code by SCRYPT_BSV_SRC_ROOT")
endif()
# This file is mainly copied from ${SCRYPT_BSV_SRC_ROOT}/sv/src/secp256k1/CMakeLists.txt with slightl modifications
#######################################################################


if(UNIX)
  ## secp256k1 use a different set of compilation flags
  scryptAddCompilerFlags(-pedantic -Wshadow -Wno-unused-function -Wno-nonnull -Wno-nonnull-compare -fPIC -fvisibility=default)
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
#function(link_secp256k1_internal NAME)
#	target_link_libraries(${NAME} secp256k1)
#	target_compile_definitions(${NAME} PRIVATE HAVE_CONFIG_H SECP256K1_BUILD)
#endfunction(link_secp256k1_internal)



# Generate the config
set(LIBSECP256K1_CONFIG_IN "${SCRYPT_BSV_SRC_ROOT}/src/secp256k1/src/libsecp256k1-config.h.cmake.in")
set(LIBSECP256K1_CONFIG "${SCRYPT_GENERATED_HPP_DIR}/libsecp256k1-config.h")
configure_file(${LIBSECP256K1_CONFIG_IN} ${LIBSECP256K1_CONFIG} ESCAPE_QUOTES)

if(UNIX)
  ## restore the old compiler flags
  scryptRecoverCompilerFlags()
endif()
