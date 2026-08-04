#!/usr/bin/env bash
set -euo pipefail

# Build the standalone WASM module. Build ONLY:
#
#   * validation is `ctest` in the build directory, which is where the tests are
#     registered (test/types/CMakeLists.txt):
#         module/typesbdk/wasm/build.sh
#         ( cd build-wasm && ctest --output-on-failure )
#
#   * publishing the eight committed artifacts into the source tree is
#         cmake --build build-wasm --target bdk_wasm_install_insource
#     invoked only by the CI commit path (.github/workflows/build_bdk.yaml).
#
# Environment: BDK_WASM_BUILD_DIR (default <repo>/build-wasm), BDK_WASM_JOBS,
# BDK_WASM_CLEAN (default 1). BDK_WASM_RUN_TESTS and
# BDK_WASM_UPDATE_COMMITTED_ARTIFACTS no longer exist.
#
# Preflight covers build tools only. `node` is needed to RUN the tests (all but
# the optimize fixture and the size gate are node scripts) and must be on PATH at
# CONFIGURE time for CMake to register them; `ctest` ships with cmake.

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
build_dir="${BDK_WASM_BUILD_DIR:-$repo_root/build-wasm}"
jobs="${BDK_WASM_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)}"

for command in cmake emcmake emcc em++ emar emranlib make; do
  command -v "$command" >/dev/null || {
    echo "Missing required command: $command" >&2
    echo "Activate Emscripten 4.0.23 with 'source /path/to/emsdk_env.sh' before running this script." >&2
    exit 1
  }
done

# bitcoin-sv is discovered by CMake, exactly like the native build: it honours
# ENV{BSV_ROOT} when set and otherwise resolves the sibling ../bitcoin-sv. The
# facility script neither pins a commit nor provisions a checkout; the pinned
# version is owned by the environment (CI's pinned checkout, or the developer's
# local one). build.sh needs no bitcoin-sv path of its own: CMake discovers it and
# the in-tree secp256k1 build reaches it directly, so nothing reads CMakeCache.txt.

# Boost is discovered, version-checked and layout-normalized entirely by CMake
# (module/typesbdk/wasm/CMakeLists.txt): it reads ENV{BOOST_ROOT}, accepts both a
# root that contains boost/ directly and one that contains include/boost/,
# enforces BOOST_VERSION 108500, and anchors the -ffile-prefix-map on the
# resolved include directory. The facility script neither downloads nor pins
# Boost; the environment supplies it (CI's depcy package, or a local install).

if [[ "${BDK_WASM_CLEAN:-1}" == 1 ]]; then
  cmake -E remove_directory "$build_dir"
fi

# Keep the full verification table so sustained ECDSA throughput never trades
# away speed for download size. Signing uses libsecp256k1's smallest supported,
# fully tested generator-table configuration.
EMSDK_QUIET=1 emcmake cmake -S "$repo_root" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
  -DBDK_BUILD_CORE=OFF \
  -DBDK_BUILD_WASM=ON \
  -DBDK_BUILD_MODULES=OFF \
  -DBDK_BUILD_CORE_TESTS=OFF \
  -DBUILD_MODULE_GOLANG=OFF \
  -DBUILD_MODULE_GOLANG_INSTALL_INSOURCE=OFF \
  -DBUILD_MODULE_RUST=OFF \
  -DBUILD_MODULE_RUST_INSTALL_INSOURCE=OFF \
  -DBDK_INSTALL_CORE_ARCHIVE=OFF \
  -DBDK_INSTALL_BSV_HEADERS=OFF \
  -DBDK_LOG_BSV_FILES=OFF \
  -DSECP256K1_ASM=OFF \
  -DSECP256K1_BUILD_BENCHMARK=OFF \
  -DSECP256K1_ECMULT_WINDOW_SIZE=15 \
  -DSECP256K1_ECMULT_GEN_KB=2 \
  -DSECP256K1_TEST_OVERRIDE_WIDE_MULTIPLY=int64

# One build. ALL builds the four module targets and their optimize edges (wasm-opt
# runs inside CMake, fed from a pristine snapshot of each linker output). The
# secp256k1 parity-suite executables are EXCLUDE_FROM_ALL and are built on demand
# by their CTest fixture; benchmarks are off for wasm. No bitcoin-sv path is needed
# here and nothing reads CMakeCache.txt.
dist_dir="$build_dir/module/typesbdk/wasm/dist"
cmake --build "$build_dir" --parallel "$jobs"

echo "Built:"
ls -lh \
  "$dist_dir/bdk-core.mjs" "$dist_dir/bdk-core.wasm" \
  "$dist_dir/bdk-core.browser.mjs" "$dist_dir/bdk-core.browser.wasm" \
  "$dist_dir/bdk-core.umd.js" "$dist_dir/bdk-core.umd.wasm" \
  "$dist_dir/bdk-core.slim.umd.js" "$dist_dir/bdk-core.slim.umd.wasm"
