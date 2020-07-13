#################################################################
#  Date             31/01/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

## Build leveldb requires locating bsv source code
if(NOT DEFINED SESDK_BSV_ROOT_DIR)#
    message(FATAL_ERROR "Unable to locate bsv source code by SESDK_BSV_ROOT_DIR")
endif()

macro(FindSHLWAPI)
# Try to find the SHLWAPI librairy
# SHLWAPI_FOUND - system has SHLWAPI lib
# SHLWAPI_INCLUDE_DIR - the SHLWAPI include directory
# SHLWAPI_LIBRARY - Libraries needed to use SHLWAPI
  if(SHLWAPI_INCLUDE_DIR AND SHLWAPI_LIBRARY)
    # Already in cache, be silent
    set(SHLWAPI_FIND_QUIETLY TRUE)
  endif()
  find_path(SHLWAPI_INCLUDE_DIR NAMES shlwapi.h)
  find_library(SHLWAPI_LIBRARY NAMES shlwapi)
  message(STATUS "SHLWAPI lib: " ${SHLWAPI_LIBRARY})
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(GMP DEFAULT_MSG SHLWAPI_INCLUDE_DIR SHLWAPI_LIBRARY)
  mark_as_advanced(SHLWAPI_INCLUDE_DIR SHLWAPI_LIBRARY)
  set(SHLWAPI_LIBRARIES ${SHLWAPI_LIBRARY})
  set(SHLWAPI_INCLUDE_DIRS ${SHLWAPI_INCLUDE_DIR})
endmacro()

# This file is mainly copied from ${SESDK_BSV_ROOT_DIR}/sv/src/leveldb/CMakeLists.txt with slightl modifications
#######################################################################

if(NOT MSVC)
  ## leveldb use a different set of compilation flags
  scrypt_add_compiler_flag(-Wno-sign-compare -Wno-implicit-fallthrough)
  scrypt_add_c_compiler_flag(-Wno-strict-prototypes)
  scrypt_remove_compiler_flags(-Wstrict-prototypes)
endif()

set(SESDK_BSV_LEVELDB_ROOT "${SESDK_BSV_ROOT_DIR}/src/leveldb")

include(CheckIncludeFileCXX)
check_include_file_cxx("atomic" LEVELDB_ATOMIC_PRESENT)

include_directories("${SESDK_BSV_LEVELDB_ROOT}")

# Leveldb library
add_library(leveldb
  "${SESDK_BSV_LEVELDB_ROOT}/db/builder.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/c.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/dbformat.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/db_impl.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/db_iter.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/dumpfile.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/filename.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/log_reader.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/log_writer.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/memtable.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/repair.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/table_cache.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/version_edit.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/version_set.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/db/write_batch.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/table/block_builder.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/table/block.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/table/filter_block.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/table/format.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/table/iterator.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/table/merger.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/table/table_builder.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/table/table.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/table/two_level_iterator.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/arena.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/bloom.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/cache.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/coding.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/comparator.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/crc32c.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/env.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/env_posix.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/filter_policy.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/hash.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/histogram.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/logging.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/options.cc"
  "${SESDK_BSV_LEVELDB_ROOT}/util/status.cc"
)

# The SSE4.2 optimized CRC32 implementation.
add_library(leveldb-sse4.2 "${SESDK_BSV_LEVELDB_ROOT}/port/port_posix_sse.cc")
target_link_libraries(leveldb leveldb-sse4.2)

# The libmemenv library.
add_library(memenv "${SESDK_BSV_LEVELDB_ROOT}/helpers/memenv/memenv.cc")

# Select the proper port: posix or Windows.
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(LEVELDB_PLATFORM WINDOWS)
    set(LEVELDB_OS WINDOWS)
    target_sources(leveldb
        PRIVATE
          "${SESDK_BSV_LEVELDB_ROOT}/util/env_win.cc"
          "${SESDK_BSV_LEVELDB_ROOT}/port/port_win.cc"
    )
    target_compile_definitions(leveldb
        PRIVATE
            WINVER=0x0500
            __USE_MINGW_ANSI_STDIO=1
    )

    #FindSHLWAPI()
    #target_link_libraries(leveldb ${SHLWAPI_LIBRARY})
    #target_include_directories(leveldb PUBLIC ${SHLWAPI_INCLUDE_DIR})
    target_link_libraries(leveldb shlwapi)
else()
    set(LEVELDB_PLATFORM POSIX)
    target_sources(leveldb PRIVATE "${SESDK_BSV_LEVELDB_ROOT}/port/port_posix.cc")

    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(leveldb Threads::Threads)

    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(LEVELDB_OS LINUX)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(LEVELDB_OS MACOSX)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "(Solaris|SunOS)")
        set(LEVELDB_OS SOLARIS)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
        set(LEVELDB_OS FREEBSD)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
        set(LEVELDB_OS NETBSD)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
        set(LEVELDB_OS OPENBSD)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
        set(LEVELDB_OS DRAGONFLYBSD)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
        set(LEVELDB_OS ANDROID)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "HPUX")
        # No idea what's the proper system name is here.
        set(LEVELDB_OS HPUX)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "iOS")
        # No idea what's the proper system name is here.
        set(LEVELDB_OS IOS)
    else()
        # Unknown plateform, assume linux.
        set(LEVELDB_OS LINUX)
    endif()
endif()

# Right now this is not used but the latest version of leveldb uses this
# so we might as well be ready for it.
if (HAVE_CRC32C)
  target_link_libraries(leveldb crc32c)
endif (HAVE_CRC32C)
if (HAVE_SNAPPY)
  target_link_libraries(leveldb snappy)
endif (HAVE_SNAPPY)

# Configure all leveldb libraries.
function(configure_leveldb_lib LIB)
    target_include_directories(${LIB} PUBLIC "${SESDK_BSV_LEVELDB_ROOT}/include")
    target_compile_definitions(${LIB}
        PUBLIC
            OS_${LEVELDB_OS}
            LEVELDB_PLATFORM_${LEVELDB_PLATFORM}
    )
    if(LEVELDB_ATOMIC_PRESENT)
        target_compile_definitions(${LIB} PUBLIC LEVELDB_ATOMIC_PRESENT)
    endif(LEVELDB_ATOMIC_PRESENT)
endfunction()

configure_leveldb_lib(leveldb)
configure_leveldb_lib(leveldb-sse4.2)
configure_leveldb_lib(memenv)
set_property(TARGET leveldb PROPERTY FOLDER "core/leveldb")
set_property(TARGET leveldb-sse4.2 PROPERTY FOLDER "core/leveldb")
set_property(TARGET memenv PROPERTY FOLDER "core/leveldb")

# Check support for SSE4.2 and act accordingly.
set(CMAKE_REQUIRED_FLAGS -msse4.2)
check_c_source_compiles("
    #include <stdint.h>
    #if defined(_MSC_VER)
    #include <intrin.h>
    #elif defined(__GNUC__) && defined(__SSE4_2__)
    #include <nmmintrin.h>
    #endif
    int main() {
        uint64_t l = 0;
        l = _mm_crc32_u8(l, 0);
        l = _mm_crc32_u32(l, 0);
        l = _mm_crc32_u64(l, 0);
        return l;
    }
" ENABLE_HWCRC32)
if(ENABLE_HWCRC32)
    target_compile_definitions(leveldb-sse4.2 PRIVATE LEVELDB_PLATFORM_POSIX_SSE)
    target_compile_options(leveldb-sse4.2 PRIVATE -msse4.2)
endif()




#######################################################################################################################
## Install leveldb the *.h header files should be kept as leveldb structure        ####################################
file(GLOB_RECURSE LEVELDB_PUBLIC_HEADERS "${SESDK_BSV_LEVELDB_ROOT}/include/*.h") # TO INSTALL
install(FILES ${LEVELDB_PUBLIC_HEADERS} DESTINATION "include/leveldb" COMPONENT leveldb)
install(TARGETS leveldb leveldb-sse4.2 memenv DESTINATION "lib" COMPONENT leveldb)
###############################################


if(NOT MSVC)
  ## leveldb restore compiler flags as it is
  scrypt_add_c_compiler_flag(-Wstrict-prototypes)
  scrypt_remove_compiler_flags(-Wno-sign-compare -Wno-implicit-fallthrough -Wno-strict-prototypes)
endif()
