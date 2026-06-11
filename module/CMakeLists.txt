#################################################################
#  Date             12/03/2020                                  #
#  Author           Chi Thanh NGUYEN                            #
#                                                               #
#  Copyright (c) 2020 nChain Limited. All rights reserved       #
#################################################################


## Modules are build based on (link to) bdk-core
## They are considered as orthogonal extension applications, libraries that extends script's functionalities
##
## Modules should depend ONLY bdk-core, and NOT each other

if(NOT BDK_BUILD_MODULES)
  message(WARNING "Deactivated building modules. Set BDK_BUILD_MODULES=ON to activate.")
  return()
endif()

message(STATUS "Building modules ...")

add_subdirectory(example)

if(BUILD_MODULE_GOLANG)
	add_subdirectory(gobdk/bdkcgo)
else()
	message(STATUS "Building Golang module has been disabled")
endif()

if(BUILD_MODULE_RUST)
	# Entering the Rust module requires a working Rust toolchain. Unlike a soft
	# skip, a missing cargo/rustc here is a hard error: opting into
	# BUILD_MODULE_RUST means you must be able to build, link, and test it.
	# Reconfigure with -DBUILD_MODULE_RUST=OFF to build BDK without the Rust binding.
	if(NOT CARGO_EXECUTABLE OR NOT RUSTC_EXECUTABLE)
		message(FATAL_ERROR
			"BUILD_MODULE_RUST=ON but the Rust toolchain was not found "
			"(need both 'cargo' and 'rustc' on PATH). Install Rust via https://rustup.rs, "
			"ensure it is on PATH, or reconfigure with -DBUILD_MODULE_RUST=OFF.")
	endif()
	message(STATUS "Rust toolchain: cargo=${CARGO_EXECUTABLE} rustc=${RUSTC_EXECUTABLE}")
	add_subdirectory(rustbdk/capi)
else()
	message(STATUS "Building Rust module has been disabled")
endif()
