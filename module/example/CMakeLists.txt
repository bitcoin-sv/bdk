#################################################################
#  Date             18/05/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################

find_package(Threads)
include(FindBoostHelper)
HelpFindBoost(filesystem program_options thread chrono)

foreach(comp IN LISTS "filesystem" "program_options" "thread" "chrono")
    if(NOT TARGET Boost::${comp})
        message(FATAL "Boost::${comp} is required but not found")
    endif()
endforeach()

# File ${BDK_BSV_ROOT_DIR}/src/logging.cpp initially included in core, but as it depend to Boost::filesytem
# we remove it from core and put it where it is needed only
add_executable(example_scriptengine_create_verify "${CMAKE_CURRENT_SOURCE_DIR}/example_scriptengine_create_verify.cpp" "${BDK_BSV_ROOT_DIR}/src/logging.cpp")
target_link_libraries(example_scriptengine_create_verify PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(example_scriptengine_create_verify PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(example_scriptengine "${CMAKE_CURRENT_SOURCE_DIR}/example_scriptengine.cpp" "${BDK_BSV_ROOT_DIR}/src/logging.cpp")
target_link_libraries(example_scriptengine PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(example_scriptengine PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})