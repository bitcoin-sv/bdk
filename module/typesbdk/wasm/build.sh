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

openssl_version=3.4.0
openssl_archive="openssl-$openssl_version.tar.gz"
openssl_url="https://www.openssl.org/source/$openssl_archive"
openssl_sha256=e15dda82fe2fe8139dc2ac21a36d4ca01d5313c75f99f46c4e8a27709b7294bf

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

if [[ -z "${BOOST_ROOT:-}" ]]; then
  boost_source="$deps_dir/boost-$boost_version"
  download "$boost_url" "$deps_dir/$boost_archive" "$boost_sha256"
  if [[ ! -d "$boost_source" ]]; then
    tar -xzf "$deps_dir/$boost_archive" -C "$deps_dir"
  fi
  if [[ ! -f "$boost_source/boost/version.hpp" ]]; then
    (cd "$boost_source" && ./bootstrap.sh && ./b2 headers)
  fi
  BOOST_ROOT="$boost_source"
fi

if [[ -z "${OPENSSL_ROOT_DIR:-}" ]]; then
  openssl_source="$deps_dir/openssl-$openssl_version"
  OPENSSL_ROOT_DIR="$deps_dir/openssl-wasm-$openssl_version"
  download "$openssl_url" "$deps_dir/$openssl_archive" "$openssl_sha256"
  if [[ ! -d "$openssl_source" ]]; then
    tar -xzf "$deps_dir/$openssl_archive" -C "$deps_dir"
  fi
  if [[ ! -f "$OPENSSL_ROOT_DIR/lib/libcrypto.a" ]]; then
    make -C "$openssl_source" clean >/dev/null 2>&1 || true
    (
      cd "$openssl_source"
      CROSS_COMPILE= CC=emcc CXX=em++ AR=emar RANLIB=emranlib NM=emnm \
        ./Configure linux-generic32 no-asm no-shared no-threads no-dso no-tests no-docs no-engine no-ui-console \
        --prefix="$OPENSSL_ROOT_DIR" --openssldir="$OPENSSL_ROOT_DIR"
      make -j"$jobs"
      make install_sw
    )
  fi
fi

if [[ -f "$BOOST_ROOT/boost/version.hpp" ]]; then
  boost_include_root="$BOOST_ROOT"
elif [[ -f "$BOOST_ROOT/include/boost/version.hpp" ]]; then
  boost_include_root="$BOOST_ROOT/include"
else
  echo "Required Boost header is missing below BOOST_ROOT: boost/version.hpp" >&2
  exit 1
fi

for path in \
  "$BSV_ROOT/src/script/interpreter.cpp" \
  "$boost_include_root/boost/version.hpp" \
  "$OPENSSL_ROOT_DIR/include/openssl/ssl.h" \
  "$OPENSSL_ROOT_DIR/lib/libcrypto.a" \
  "$OPENSSL_ROOT_DIR/lib/libssl.a"; do
  [[ -e "$path" ]] || { echo "Required dependency path is missing: $path" >&2; exit 1; }
done

if [[ "${BDK_WASM_CLEAN:-1}" == 1 ]]; then
  cmake -E remove_directory "$build_dir"
fi

EMSDK_QUIET=1 emcmake cmake -S "$repo_root" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBDK_BUILD_CORE_ONLY=ON \
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
  -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR" \
  -DOPENSSL_INCLUDE_DIR="$OPENSSL_ROOT_DIR/include" \
  -DOPENSSL_CRYPTO_LIBRARY="$OPENSSL_ROOT_DIR/lib/libcrypto.a" \
  -DOPENSSL_SSL_LIBRARY="$OPENSSL_ROOT_DIR/lib/libssl.a" \
  -DOPENSSL_USE_STATIC_LIBS=TRUE \
  -DSECP256K1_ASM=OFF \
  -DSECP256K1_BUILD_BENCHMARK=OFF \
  -DSECP256K1_ECMULT_WINDOW_SIZE=15 \
  -DSECP256K1_TEST_OVERRIDE_WIDE_MULTIPLY=int64

cmake --build "$build_dir" --target bdk_wasm --parallel "$jobs"

dist_dir="$build_dir/module/typesbdk/wasm/dist"
# Emscripten's -O3 link performs one Binaryen optimization pass. A converged
# -O4 pass is measurably faster for this integer-heavy verifier while retaining
# the same WebAssembly feature set emitted by Emscripten.
"$wasm_opt" "$dist_dir/bdk-core.wasm" \
  -O4 --converge \
  --enable-bulk-memory \
  --enable-bulk-memory-opt \
  --enable-nontrapping-float-to-int \
  --enable-sign-ext \
  --enable-mutable-globals \
  -o "$dist_dir/bdk-core.optimized.wasm"
mv "$dist_dir/bdk-core.optimized.wasm" "$dist_dir/bdk-core.wasm"

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
    -DSECP256K1_ENABLE_MODULE_ECDH=ON \
    -DSECP256K1_ENABLE_MODULE_RECOVERY=ON \
    -DSECP256K1_TEST_OVERRIDE_WIDE_MULTIPLY=int64
  cmake --build "$secp_test_dir" \
    --target tests noverify_tests exhaustive_tests \
    --parallel "$jobs"
  # 35 is the smallest scaling count that exercises every test, including
  # test_ecmult_constants_2bit, without making the cross-compiled CI run
  # unnecessarily long.
  node "$secp_test_dir/src/tests.js" 35 35010203040506070809000102030405
  node "$secp_test_dir/src/noverify_tests.js" 35 45010203040506070809000102030405
  node "$secp_test_dir/src/exhaustive_tests.js" 2 20010203040506070809000102030405
fi

install -m 0644 "$dist_dir/bdk-core.mjs" "$script_dir/bdk-core.mjs"
install -m 0644 "$dist_dir/bdk-core.wasm" "$script_dir/bdk-core.wasm"
node "$script_dir/test.mjs"

echo "Built and validated:"
ls -lh "$script_dir/bdk-core.mjs" "$script_dir/bdk-core.wasm"
