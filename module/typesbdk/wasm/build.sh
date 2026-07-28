#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
deps_dir="${BDK_WASM_DEPS_DIR:-$repo_root/build-wasm-deps}"
build_dir="${BDK_WASM_BUILD_DIR:-$repo_root/build-wasm}"
jobs="${BDK_WASM_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)}"

boost_version=1.85.0
boost_archive="boost-$boost_version-cmake.tar.gz"
boost_url="https://github.com/boostorg/boost/releases/download/boost-$boost_version/$boost_archive"
boost_sha256=ab9c9c4797384b0949dd676cf86b4f99553f8c148d767485aaac412af25183e6

# Minimal Boost component set for the wasm verifier, converged empirically with
# the add-and-prune derivation procedure. Must stay identical to
# WASM_BOOST_INCLUDE_LIBRARIES in .github/workflows/prebuild_dependancies.yaml;
# to change it, re-run the derivation and pin its output, never hand-edit.
BDK_WASM_BOOST_LIBS="multiprecision;chrono;uuid;variant;thread;filesystem;signals2;multi_index"

bsv_commit=879fc8b42168dd0e608dafd51b39c6dabad37d4d

for command in cmake emcmake emcc em++ emar emranlib git curl make node; do
  command -v "$command" >/dev/null || {
    echo "Missing required command: $command" >&2
    echo "Activate Emscripten 4.0.23 with 'source /path/to/emsdk_env.sh' before running this script." >&2
    exit 1
  }
done

wasm_opt="${WASM_OPT:-}"
if [[ -z "$wasm_opt" ]]; then
  if command -v wasm-opt >/dev/null; then
    wasm_opt="$(command -v wasm-opt)"
  elif [[ -n "${EMSDK:-}" && -x "$EMSDK/upstream/bin/wasm-opt" ]]; then
    wasm_opt="$EMSDK/upstream/bin/wasm-opt"
  else
    echo "Missing wasm-opt; activate the pinned Emscripten SDK or set WASM_OPT." >&2
    exit 1
  fi
fi

sha256_file () {
  local expected="$1"
  local file="$2"
  local actual
  if command -v sha256sum >/dev/null; then
    actual="$(sha256sum "$file" | awk '{print $1}')"
  else
    actual="$(shasum -a 256 "$file" | awk '{print $1}')"
  fi
  if [[ "$actual" != "$expected" ]]; then
    echo "SHA-256 mismatch for $file: expected $expected, got $actual" >&2
    exit 1
  fi
}

download () {
  local url="$1"
  local destination="$2"
  local sha256="$3"
  if [[ ! -f "$destination" ]]; then
    curl --fail --location --retry 3 --output "$destination" "$url"
  fi
  sha256_file "$sha256" "$destination"
}

mkdir -p "$deps_dir"

if [[ -z "${BSV_ROOT:-}" ]]; then
  BSV_ROOT="$deps_dir/bitcoin-sv"
  if [[ ! -d "$BSV_ROOT/.git" ]]; then
    git init "$BSV_ROOT"
    git -C "$BSV_ROOT" remote add origin https://github.com/bitcoin-sv/bitcoin-sv.git
    git -C "$BSV_ROOT" fetch --depth 1 origin "$bsv_commit"
    git -C "$BSV_ROOT" checkout --detach FETCH_HEAD
  fi
fi

if [[ ! -d "$BSV_ROOT/.git" ]]; then
  echo "BSV_ROOT must be a git checkout pinned to $bsv_commit: $BSV_ROOT" >&2
  exit 1
fi
bsv_actual_commit="$(git -C "$BSV_ROOT" rev-parse HEAD)"
if [[ "$bsv_actual_commit" != "$bsv_commit" ]]; then
  echo "BSV_ROOT is at $bsv_actual_commit; the reproducible WASM build requires $bsv_commit" >&2
  exit 1
fi

if [[ -z "${BOOST_ROOT:-}" ]]; then
  # Self-provision the pinned minimal header set from the once-downloaded
  # cached tarball, with the same cmake mechanism the prebuilt wasm dependency
  # package uses. Local builds therefore exercise exactly the component set CI
  # enforces, and a missing-header failure reproduces the CI failure.
  boost_source="$deps_dir/boost-$boost_version"
  boost_install="$deps_dir/boost-wasm"
  boost_stamp="$boost_install/.bdk-wasm-boost-libs"
  download "$boost_url" "$deps_dir/$boost_archive" "$boost_sha256"
  if [[ ! -d "$boost_source" ]]; then
    tar -xzf "$deps_dir/$boost_archive" -C "$deps_dir"
  fi
  if [[ ! -f "$boost_install/include/boost/version.hpp" ]] \
    || [[ "$(cat "$boost_stamp" 2>/dev/null)" != "$BDK_WASM_BOOST_LIBS" ]]; then
    cmake -E rm -rf "$boost_install"
    cmake -B "$boost_source/build" -S "$boost_source" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_INSTALL_PREFIX="$boost_install" \
      -DBOOST_INCLUDE_LIBRARIES="$BDK_WASM_BOOST_LIBS"
    cmake --build "$boost_source/build" --target install --parallel "$jobs"
    printf '%s' "$BDK_WASM_BOOST_LIBS" > "$boost_stamp"
  fi
  # Byte-reproducibility layout rule: BOOST_ROOT must be the directory that
  # contains boost/ DIRECTLY, so -ffile-prefix-map yields /boost/boost/... in
  # embedded __FILE__ strings exactly like the committed artifacts.
  BOOST_ROOT="$boost_install/include"
fi

if [[ -f "$BOOST_ROOT/boost/version.hpp" ]]; then
  boost_include_root="$BOOST_ROOT"
elif [[ -f "$BOOST_ROOT/include/boost/version.hpp" ]]; then
  boost_include_root="$BOOST_ROOT/include"
else
  echo "Required Boost header is missing below BOOST_ROOT: boost/version.hpp" >&2
  exit 1
fi
# Byte-repro layout rule: hand cmake the directory containing boost/ DIRECTLY,
# so -ffile-prefix-map=${BOOST_ROOT}=/boost always embeds /boost/boost/...
# regardless of whether the caller supplied a source layout or an install prefix.
BOOST_ROOT="$boost_include_root"

boost_actual_version="$(
  sed -n 's/^#define BOOST_VERSION \([0-9][0-9]*\).*/\1/p' \
    "$boost_include_root/boost/version.hpp"
)"
if [[ "$boost_actual_version" != 108500 ]]; then
  echo "BOOST_ROOT reports BOOST_VERSION=$boost_actual_version; the reproducible WASM build requires Boost $boost_version (108500)" >&2
  exit 1
fi

for path in \
  "$BSV_ROOT/src/script/interpreter.cpp" \
  "$boost_include_root/boost/version.hpp"; do
  [[ -e "$path" ]] || { echo "Required dependency path is missing: $path" >&2; exit 1; }
done

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
  -DBSV_ROOT="$BSV_ROOT" \
  -DBOOST_ROOT="$BOOST_ROOT" \
  -DSECP256K1_ASM=OFF \
  -DSECP256K1_BUILD_BENCHMARK=OFF \
  -DSECP256K1_ECMULT_WINDOW_SIZE=15 \
  -DSECP256K1_ECMULT_GEN_KB=2 \
  -DSECP256K1_TEST_OVERRIDE_WIDE_MULTIPLY=int64

# wasm-opt consumes the linker output in place. Force only the four cheap
# final links to rerun so an incremental build never optimizes an already
# optimized binary and drifts from a clean/CI artifact.
dist_dir="$build_dir/module/typesbdk/wasm/dist"
cmake -E rm -f \
  "$dist_dir/bdk-core.mjs" "$dist_dir/bdk-core.wasm" \
  "$dist_dir/bdk-core.browser.mjs" "$dist_dir/bdk-core.browser.wasm" \
  "$dist_dir/bdk-core.umd.js" "$dist_dir/bdk-core.umd.wasm" \
  "$dist_dir/bdk-core.slim.umd.js" "$dist_dir/bdk-core.slim.umd.wasm"
cmake --build "$build_dir" \
  --target bdk_wasm bdk_wasm_browser bdk_wasm_umd bdk_wasm_slim_umd \
  --parallel "$jobs"

# Emscripten's -O3 link performs one Binaryen optimization pass. A converged
# -O4 pass is measurably faster for this integer-heavy verifier while retaining
# the same WebAssembly feature set emitted by Emscripten.
for wasm_name in bdk-core bdk-core.browser bdk-core.umd bdk-core.slim.umd; do
  "$wasm_opt" "$dist_dir/$wasm_name.wasm" \
    -O4 --converge \
    --enable-bulk-memory \
    --enable-bulk-memory-opt \
    --enable-nontrapping-float-to-int \
    --enable-sign-ext \
    --enable-mutable-globals \
    -o "$dist_dir/$wasm_name.optimized.wasm"
  mv "$dist_dir/$wasm_name.optimized.wasm" "$dist_dir/$wasm_name.wasm"
done

if [[ "${BDK_WASM_RUN_SECP_TESTS:-1}" == 1 ]]; then
  secp_test_dir="$build_dir/secp256k1-tests"
  EMSDK_QUIET=1 emcmake cmake \
    -S "$BSV_ROOT/src/secp256k1" \
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

install -m 0644 "$dist_dir/bdk-core.mjs" "$script_dir/bdk-core.mjs"
install -m 0644 "$dist_dir/bdk-core.wasm" "$script_dir/bdk-core.wasm"
install -m 0644 "$dist_dir/bdk-core.browser.mjs" "$script_dir/bdk-core.browser.mjs"
install -m 0644 "$dist_dir/bdk-core.browser.wasm" "$script_dir/bdk-core.browser.wasm"
install -m 0644 "$dist_dir/bdk-core.umd.js" "$script_dir/bdk-core.umd.js"
install -m 0644 "$dist_dir/bdk-core.umd.wasm" "$script_dir/bdk-core.umd.wasm"
install -m 0644 "$dist_dir/bdk-core.slim.umd.js" "$script_dir/bdk-core.slim.umd.js"
install -m 0644 "$dist_dir/bdk-core.slim.umd.wasm" "$script_dir/bdk-core.slim.umd.wasm"

# Size is part of the verifier's compatibility contract. Check each actual
# loader-plus-WASM payload using decimal kilobytes so compression or artifact
# splitting can never disguise a regression above the 300 KB ceiling.
max_bundle_bytes=300000
for bundle_name in bdk-core bdk-core.browser bdk-core.umd; do
  loader_suffix=mjs
  [[ "$bundle_name" == bdk-core.umd ]] && loader_suffix=js
  bundle_bytes=$((
    $(wc -c < "$script_dir/$bundle_name.$loader_suffix") +
    $(wc -c < "$script_dir/$bundle_name.wasm")
  ))
  if (( bundle_bytes > max_bundle_bytes )); then
    echo "$bundle_name is $bundle_bytes bytes; maximum is $max_bundle_bytes" >&2
    exit 1
  fi
done
slim_bundle_bytes=$((
  $(wc -c < "$script_dir/bdk-core.slim.umd.js") +
  $(wc -c < "$script_dir/bdk-core.slim.umd.wasm")
))
if (( slim_bundle_bytes > max_bundle_bytes )); then
  echo "bdk-core.slim.umd is $slim_bundle_bytes bytes; maximum is $max_bundle_bytes" >&2
  exit 1
fi
node "$script_dir/test.mjs"
node "$script_dir/test.mjs" bdk-core.browser.mjs
node "$script_dir/test-umd.mjs"
node "$script_dir/test-umd.mjs" bdk-core.slim.umd.js

echo "Built and validated:"
ls -lh \
  "$script_dir/bdk-core.mjs" "$script_dir/bdk-core.wasm" \
  "$script_dir/bdk-core.browser.mjs" "$script_dir/bdk-core.browser.wasm" \
  "$script_dir/bdk-core.umd.js" "$script_dir/bdk-core.umd.wasm" \
  "$script_dir/bdk-core.slim.umd.js" "$script_dir/bdk-core.slim.umd.wasm"
