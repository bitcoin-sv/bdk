## Build and validate the WASM module

Use Emscripten 4.0.23, then run the pinned build script from the BDK root:

```bash
source /path/to/emsdk/emsdk_env.sh
module/typesbdk/wasm/build.sh
```

The script downloads and verifies Boost 1.85.0 and OpenSSL 3.4.0, checks out the
same `bitcoin-sv` commit used by BDK CI, builds OpenSSL for WASM, performs a clean
core-only BDK build, runs libsecp256k1's verified, non-verified, and exhaustive
WASM test binaries, and runs real positive and negative transaction vectors.
Dependencies and build output default to `build-wasm-deps/` and `build-wasm/`.
Set `BDK_WASM_DEPS_DIR`, `BDK_WASM_BUILD_DIR`, or `BDK_WASM_JOBS` to override
those locations. Existing `BSV_ROOT`, `BOOST_ROOT`, and `OPENSSL_ROOT_DIR`
installations are honored.

The production artifact uses libsecp256k1's 32-bit-limb arithmetic backend
(`int64` in libsecp terminology), the maximum bundled verification precompute
window, and a converged Binaryen `-O4` pass. On wasm32 this avoids Clang's much
slower lowering of native `__int128` arithmetic. Set
`BDK_WASM_RUN_SECP_TESTS=0` only for local iteration when the standalone curve
suite has already passed.

Successful output includes:

```text
ok - mainnet-p2pkh-block-620940: domain=0 code=0
ok - mainnet-p2pkh-corrupt-signature: domain=1 code=39
```

The validated `bdk-core.mjs` and `bdk-core.wasm` are installed beside the build
script. `VerifyScript` returns a structured `{ domain, code }` result; domain `0`
is success, domain `1` is a script failure, and domain `3` indicates an exception.
`VerifyScriptArray` is the preferred ABI: it bulk-copies normal JavaScript
number arrays into WASM memory. `VerifyScript` remains available for existing
callers that use Embind vectors.

## Native and direct WASM benchmark controls

After a WASM build, benchmark the exact exported verifier without SDK
serialization overhead:

```bash
node module/typesbdk/wasm/benchmark.mjs 5000 11
```

Build the matching native control with a native Boost and OpenSSL installation:

```bash
cmake -S . -B build-native-benchmark \
  -DCMAKE_BUILD_TYPE=Release \
  -DBDK_BUILD_CORE_ONLY=ON \
  -DBDK_BUILD_MODULES=OFF \
  -DBDK_BUILD_CORE_TESTS=OFF \
  -DBUILD_MODULE_GOLANG=OFF \
  -DBUILD_MODULE_GOLANG_INSTALL_INSOURCE=OFF \
  -DBUILD_MODULE_RUST=OFF \
  -DBUILD_MODULE_RUST_INSTALL_INSOURCE=OFF \
  -DBDK_BUILD_NATIVE_VERIFY_BENCHMARK=ON \
  -DBSV_ROOT=/path/to/bitcoin-sv \
  -DCUSTOM_BOOST_ROOT=/path/to/boost \
  -DOPENSSL_ROOT_DIR=/path/to/openssl
cmake --build build-native-benchmark --target bdk_verify_benchmark
build-native-benchmark/x64/release/bdk_verify_benchmark 5000 11
```

### To run example backend

```
rm -rf /path/to/bdk/module/typesbdk/examples/backend/node_modules && cd /path/to/bdk/module/typesbdk/examples/backend
node /path/to/bdk/module/typesbdk/examples/backend/index.mjs
```

### To run example frontend

```
rm -rf /path/to/bdk/module/typesbdk/examples/frontend/node_modules && cd /path/to/bdk/module/typesbdk/examples/frontend
yarn install && yarn run start
# Then open the browser and click on "verify"
```
