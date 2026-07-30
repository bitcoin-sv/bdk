#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
build_dir="${BDK_WASM_BUILD_DIR:-$repo_root/build-wasm}"
jobs="${BDK_WASM_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)}"

for command in cmake emcmake emcc em++ emar emranlib make node ctest; do
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
# local one). The resolved path is read back from the CMake cache after the
# configure step (see below) for the secp256k1 sub-build.

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
# runs inside CMake, fed from a pristine snapshot of each linker output) plus the
# in-tree secp256k1 curve-test executables; benchmarks are off for wasm. No
# bitcoin-sv path is needed here and nothing reads CMakeCache.txt.
dist_dir="$build_dir/module/typesbdk/wasm/dist"
cmake --build "$build_dir" --parallel "$jobs"

# Validate. CTest owns the functional suites and the size gate; it runs them
# against the freshly built dist artifacts, gates them behind the optimize
# fixture, and reports correctness before the size contract. Default on; opt out
# with BDK_WASM_RUN_TESTS=0. Publishing requires validation, so refuse to publish
# when tests were skipped. Runs are cwd-based: the repo floor is CMake 3.16, which
# has neither --test-dir nor --no-tests=error, so a non-empty registration is
# asserted with a `ctest -N` count first (a zero-test build is not "validated").
if [[ "${BDK_WASM_RUN_TESTS:-1}" == 1 ]]; then
  ntests="$( cd "$build_dir" && ctest -N | grep -c 'Test #' || true )"
  [[ "${ntests:-0}" -ge 1 ]] || { echo "No CTest tests registered; refusing to report validated." >&2; exit 1; }
  ( cd "$build_dir" && ctest --output-on-failure )
  summary_label="Built and validated:"
elif [[ "${BDK_WASM_UPDATE_COMMITTED_ARTIFACTS:-0}" == 1 ]]; then
  echo "Refusing to publish with BDK_WASM_RUN_TESTS=0: publishing requires validation." >&2
  exit 1
else
  summary_label="Built (CTest validation skipped via BDK_WASM_RUN_TESTS=0):"
fi

# Publishing the committed artifacts is an explicit opt-in. A plain build stays
# entirely in the build tree and never writes the tracked files, so a valid but
# off-pin local environment cannot leave committable-looking output behind. CI
# (or a deliberate regen under the pinned environment) sets this flag; the size
# gate above has already rejected any over-ceiling bundle before it can reach the
# tracked tree.
if [[ "${BDK_WASM_UPDATE_COMMITTED_ARTIFACTS:-0}" == 1 ]]; then
  install -m 0644 "$dist_dir/bdk-core.mjs" "$script_dir/bdk-core.mjs"
  install -m 0644 "$dist_dir/bdk-core.wasm" "$script_dir/bdk-core.wasm"
  install -m 0644 "$dist_dir/bdk-core.browser.mjs" "$script_dir/bdk-core.browser.mjs"
  install -m 0644 "$dist_dir/bdk-core.browser.wasm" "$script_dir/bdk-core.browser.wasm"
  install -m 0644 "$dist_dir/bdk-core.umd.js" "$script_dir/bdk-core.umd.js"
  install -m 0644 "$dist_dir/bdk-core.umd.wasm" "$script_dir/bdk-core.umd.wasm"
  install -m 0644 "$dist_dir/bdk-core.slim.umd.js" "$script_dir/bdk-core.slim.umd.js"
  install -m 0644 "$dist_dir/bdk-core.slim.umd.wasm" "$script_dir/bdk-core.slim.umd.wasm"
fi

echo "$summary_label"
ls -lh \
  "$dist_dir/bdk-core.mjs" "$dist_dir/bdk-core.wasm" \
  "$dist_dir/bdk-core.browser.mjs" "$dist_dir/bdk-core.browser.wasm" \
  "$dist_dir/bdk-core.umd.js" "$dist_dir/bdk-core.umd.wasm" \
  "$dist_dir/bdk-core.slim.umd.js" "$dist_dir/bdk-core.slim.umd.wasm"
