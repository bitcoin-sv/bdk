## Build and validate the WASM module

Use Emscripten 4.0.23, then run the pinned build script from the BDK root:

```bash
source /path/to/emsdk/emsdk_env.sh
module/typesbdk/wasm/build.sh
```

The script downloads and verifies Boost 1.85.0 and OpenSSL 3.4.0, checks out the
same `bitcoin-sv` commit used by BDK CI, builds OpenSSL for WASM, performs a clean
core-only BDK build, and runs real positive and negative transaction vectors.
Dependencies and build output default to `build-wasm-deps/` and `build-wasm/`.
Set `BDK_WASM_DEPS_DIR`, `BDK_WASM_BUILD_DIR`, or `BDK_WASM_JOBS` to override
those locations. Existing `BSV_ROOT`, `BOOST_ROOT`, and `OPENSSL_ROOT_DIR`
installations are honored.

Successful output includes:

```text
ok - mainnet-p2pkh-block-620940: domain=0 code=0
ok - mainnet-p2pkh-corrupt-signature: domain=1 code=39
```

The validated `bdk-core.mjs` and `bdk-core.wasm` are installed beside the build
script. `VerifyScript` returns a structured `{ domain, code }` result; domain `0`
is success, domain `1` is a script failure, and domain `3` indicates an exception.

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
