#################################################################
#  Date             17/09/2026                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2026 nChain Limited. All rights reserved       #
#################################################################

find_package(Threads)
include(FindBoostHelper)
HelpFindBoost(thread filesystem program_options chrono)

message("Building GoBDK cgo shim tests ...")

# bdk_core carries only the minimal bsv sources; the application sources it calls into
# (logging, the text writers, ...) are compiled into each consumer, exactly as every
# target in test/core and module/example does. They cannot be picked up from
# static-GoBDK instead: the linker sees that archive before libbdk_core.a, so the
# application objects bdk_core pulls in afterwards would no longer be reachable.
# BSV_APPLICATION_*_FILES are cache variables set by HelpFindBSVSource() at the top
# level, so they are in scope here.
add_executable(test_gobdk_shim
               "${CMAKE_CURRENT_SOURCE_DIR}/test_gobdk_shim.cpp"
               "${BSV_APPLICATION_HDR_FILES}"
               "${BSV_APPLICATION_SRC_FILES}"
)

# static-GoBDK contributes the cgo shim objects under test. Its own copy of the
# application sources stays inert: every symbol those objects define is already defined
# by this executable, so no member of that part of the archive is ever pulled in.
target_link_libraries(test_gobdk_shim PRIVATE static-GoBDK bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

# static-GoBDK sets its include directories PRIVATE, so nothing is inherited through the
# link: the roots the test compiles against have to be listed here.
target_include_directories(test_gobdk_shim PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/core
  ${CMAKE_SOURCE_DIR}/module/gobdk
)

bdk_add_unit_test(test_gobdk_shim)

if(CMAKE_CONFIGURATION_TYPES)
  set_target_properties(test_gobdk_shim PROPERTIES FOLDER "test/module" DEBUG_POSTFIX "_d")# On dual build mode system, need to add postfix _d
else()
  set_target_properties(test_gobdk_shim PROPERTIES DEBUG_POSTFIX "")# Works well on Linux debug mode
endif()
