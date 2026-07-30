#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
build_dir="${BDK_WASM_BUILD_DIR:-$repo_root/build-wasm}"
jobs="${BDK_WASM_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)}"

for command in cmake emcmake emcc em++ emar emranlib make node; do
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

# Reuse CMake's single bitcoin-sv discovery result instead of re-deriving it in
# the shell: the configure above wrote the resolved path to the cache as
# BDK_BSV_ROOT_DIR, and the secp256k1 curve-test sub-build below needs it for
# its -S source path. Fail loudly on an empty or bogus read so a broken cache
# surfaces here rather than as an opaque "-S /src/secp256k1" configure error.
bsv_root="$(sed -n 's/^BDK_BSV_ROOT_DIR:PATH=//p' "$build_dir/CMakeCache.txt")"
if [[ -z "$bsv_root" || ! -d "$bsv_root/src/secp256k1" ]]; then
  echo "could not read the resolved bitcoin-sv root from the CMake cache: $build_dir/CMakeCache.txt" >&2
  exit 1
fi

# wasm-opt now runs inside CMake as a tracked build edge: each link output is
# snapshotted raw and the optimized canonical .wasm is produced from that pristine
# snapshot. Building the four *_optimized targets drives the module targets, their
# raw snapshots and the optimize edges, leaving the final optimized bytes in dist.
dist_dir="$build_dir/module/typesbdk/wasm/dist"
cmake --build "$build_dir" \
  --target bdk_wasm_optimized bdk_wasm_browser_optimized \
           bdk_wasm_umd_optimized bdk_wasm_slim_umd_optimized \
  --parallel "$jobs"

if [[ "${BDK_WASM_RUN_SECP_TESTS:-1}" == 1 ]]; then
  secp_test_dir="$build_dir/secp256k1-tests"
  EMSDK_QUIET=1 emcmake cmake \
    -S "$bsv_root/src/secp256k1" \
    -B "$secp_test_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_EXE_LINKER_FLAGS=-sSTACK_SIZE=8388608 -sALLOW_MEMORY_GROWTH=1" \
    -DSECP256K1_ASM=OFF \
    -DSECP256K1_BUILD_BENCHMARK=OFF \
    -DSECP256K1_BUILD_CTIME_TESTS=OFF \
    -DSECP256K1_BUILD_EXAMPLES=OFF \
    -DSECP256K1_BUILD_EXHAUSTIVE_TESTS=ON \
    -DSECP256K1_BUILD_TESTS=ON \
    -DSECP256K1_ECMULT_WINDOW_SIZE=15 \
    -DSECP256K1_ECMULT_GEN_KB=2 \
    -DSECP256K1_ENABLE_MODULE_ECDH=ON \
    -DSECP256K1_ENABLE_MODULE_RECOVERY=ON \
    -DSECP256K1_TEST_OVERRIDE_WIDE_MULTIPLY=int64
  cmake --build "$secp_test_dir" \
    --target tests noverify_tests exhaustive_tests \
    --parallel "$jobs"
  # A parent package.json may declare `type: module`; Emscripten's standalone
  # test runners are CommonJS. The .cjs copies make their module format
  # explicit without modifying generated sources or depending on checkout path.
  for test_name in tests noverify_tests exhaustive_tests; do
    cmake -E copy "$secp_test_dir/src/$test_name.js" "$secp_test_dir/src/$test_name.cjs"
  done
  # 35 is the smallest scaling count that exercises every test, including
  # test_ecmult_constants_2bit, without making the cross-compiled CI run
  # unnecessarily long.
  node "$secp_test_dir/src/tests.cjs" 35 35010203040506070809000102030405
  node "$secp_test_dir/src/noverify_tests.cjs" 35 45010203040506070809000102030405
  node "$secp_test_dir/src/exhaustive_tests.cjs" 2 20010203040506070809000102030405
fi

# Functional tests run against the freshly built artifacts in the build tree,
# never the tracked source-tree copies, so a build is validated whether or not
# it goes on to publish the committed files. They come before the size gate:
# correctness is the more important signal and must always be exercised, while
# the size gate is a deployment-size contract reported last. Each test script is
# handed the dist module path so it loads the matching dist WASM alongside it.
node "$script_dir/test.mjs" "$dist_dir/bdk-core.mjs"
node "$script_dir/test.mjs" "$dist_dir/bdk-core.browser.mjs"
node "$script_dir/test-umd.mjs" "$dist_dir/bdk-core.umd.js"
node "$script_dir/test-umd.mjs" "$dist_dir/bdk-core.slim.umd.js"

# Size is part of the verifier's compatibility contract. Check each actual
# loader-plus-WASM payload using decimal kilobytes so compression or artifact
# splitting can never disguise a regression above the 300 KB ceiling. Report
# every oversized bundle and fail once at the end, so a single run surfaces the
# full picture instead of stopping at the first offender.
max_bundle_bytes=300000
oversized_bundles=()
for bundle_name in bdk-core bdk-core.browser bdk-core.umd bdk-core.slim.umd; do
  case "$bundle_name" in
    bdk-core.umd|bdk-core.slim.umd) loader_suffix=js ;;
    *) loader_suffix=mjs ;;
  esac
  bundle_bytes=$((
    $(wc -c < "$dist_dir/$bundle_name.$loader_suffix") +
    $(wc -c < "$dist_dir/$bundle_name.wasm")
  ))
  if (( bundle_bytes > max_bundle_bytes )); then
    echo "$bundle_name is $bundle_bytes bytes; maximum is $max_bundle_bytes" >&2
    oversized_bundles+=("$bundle_name")
  fi
done
if (( ${#oversized_bundles[@]} > 0 )); then
  echo "Oversized wasm bundles (max $max_bundle_bytes bytes): ${oversized_bundles[*]}" >&2
  exit 1
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

echo "Built and validated:"
ls -lh \
  "$dist_dir/bdk-core.mjs" "$dist_dir/bdk-core.wasm" \
  "$dist_dir/bdk-core.browser.mjs" "$dist_dir/bdk-core.browser.wasm" \
  "$dist_dir/bdk-core.umd.js" "$dist_dir/bdk-core.umd.wasm" \
  "$dist_dir/bdk-core.slim.umd.js" "$dist_dir/bdk-core.slim.umd.wasm"
