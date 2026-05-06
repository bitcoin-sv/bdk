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

add_executable(example_txvalidator_create_validate "${CMAKE_CURRENT_SOURCE_DIR}/example_txvalidator_create_validate.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(example_txvalidator_create_validate PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(example_txvalidator_create_validate PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(example_txvalidator "${CMAKE_CURRENT_SOURCE_DIR}/example_txvalidator.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(example_txvalidator PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(example_txvalidator PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(bench_validatetransaction "${CMAKE_CURRENT_SOURCE_DIR}/bench_validatetransaction.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(bench_validatetransaction PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(bench_validatetransaction PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(bench_validatetransaction_single "${CMAKE_CURRENT_SOURCE_DIR}/bench_validatetransaction_single.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(bench_validatetransaction_single PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(bench_validatetransaction_single PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(bench_validatetransaction_batch "${CMAKE_CURRENT_SOURCE_DIR}/bench_validatetransaction_batch.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(bench_validatetransaction_batch PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(bench_validatetransaction_batch PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(bench_num2bin_attack_validate "${CMAKE_CURRENT_SOURCE_DIR}/bench_num2bin_attack_validate.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(bench_num2bin_attack_validate PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(bench_num2bin_attack_validate PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(example_num2bin_memoryattack_validate "${CMAKE_CURRENT_SOURCE_DIR}/example_num2bin_memoryattack_validate.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(example_num2bin_memoryattack_validate PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(example_num2bin_memoryattack_validate PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_executable(example_special_opcode_validate "${CMAKE_CURRENT_SOURCE_DIR}/example_special_opcode_validate.cpp" "${BSV_APPLICATION_HDR_FILES}" "${BSV_APPLICATION_SRC_FILES}")
target_link_libraries(example_special_opcode_validate PRIVATE bdk_core secp256k1 univalue OpenSSL::Crypto OpenSSL::SSL Boost::thread Boost::filesystem Boost::program_options Boost::chrono ${CMAKE_DL_LIBS})
set_target_properties(example_special_opcode_validate PROPERTIES  FOLDER "module/example" DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})