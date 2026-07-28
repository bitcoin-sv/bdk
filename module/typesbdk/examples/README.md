## Build and validate the WASM module

`build.sh` is the reproducible WASM entry point. Use Emscripten 4.0.23, then
run the pinned build script from the BDK root:

```bash
source /path/to/emsdk/emsdk_env.sh
module/typesbdk/wasm/build.sh

# Optional: build against the prebuilt minimal Boost package instead of
# letting the script self-provision it:
curl --fail --location -o /tmp/dependancies_wasm.tar.gz \
  "https://github.com/bitcoin-sv/bdk/releases/download/depcy/dependancies_wasm.tar.gz"
mkdir -p build-wasm-deps && tar -xzf /tmp/dependancies_wasm.tar.gz -C build-wasm-deps
BOOST_ROOT="$PWD/build-wasm-deps/dependancies_wasm/boost_1.85.0" module/typesbdk/wasm/build.sh
```

The script installs the pinned minimal Boost 1.85.0 header set (when no
`BOOST_ROOT` is supplied), checks out the same `bitcoin-sv` commit used by BDK
CI, performs a clean standalone module build (`-DBDK_BUILD_CORE=OFF
-DBDK_BUILD_TYPES=ON`) that assembles the module's own `bdk_core_wasm` variant
from the shared core recipe, runs libsecp256k1's verified, non-verified, and
exhaustive WASM test binaries, and
runs real positive and negative transaction vectors. The verifier-only WASM
build does not require or link OpenSSL: it uses header-only multiprecision and a
minimal memory-cleanse implementation. Dependencies and build output default to
`build-wasm-deps/` and `build-wasm/`. Set `BDK_WASM_DEPS_DIR`,
`BDK_WASM_BUILD_DIR`, or `BDK_WASM_JOBS` to override those locations. Existing
`BSV_ROOT` and `BOOST_ROOT` installations are honored after their pinned
versions are verified. In particular, an existing `BSV_ROOT` must be a git
checkout at the exact commit used by the reproducible build.

The production artifact uses libsecp256k1's 32-bit-limb arithmetic backend
(`int64` in libsecp terminology), whole-program LTO for the hot verifier, and a
converged Binaryen `-O4` pass. It retains the full W15 verification precompute
window without shipping its serialized megabyte-scale tables: the table is
reconstructed lazily on the first verification using one field inversion and
the curve endomorphism for the second lane. On wasm32 the 32-bit backend avoids
Clang's much slower lowering of native `__int128` arithmetic. Set
`BDK_WASM_RUN_SECP_TESTS=0` only for local iteration when the standalone curve
suite has already passed.

Successful output includes:

```text
ok - bdk-core.mjs mainnet-p2pkh-block-620940: domain=0 code=0
ok - bdk-core.mjs mainnet-p2pkh-corrupt-signature: domain=1 code=39
```

The validated Node (`bdk-core.mjs`, `bdk-core.wasm`) and browser/worker
(`bdk-core.browser.mjs`, `bdk-core.browser.wasm`) artifacts are installed beside
the build script, together with a classic-script/UMD loader
(`bdk-core.umd.js`, `bdk-core.umd.wasm`). The split keeps Node loader imports
out of browser bundler graphs while preserving an identical verification ABI.

Verifier calls return a structured `{ domain, code }` result: domain `0` is
success, domain `1` is a script failure, domain `2` is a transaction-validation
or DoS-class failure, and domain `3` indicates a caught exception. Callers must
not treat an unrecognized domain as success.

`VerifyScriptArray` is the preferred single-transaction ABI: it bulk-copies
typed arrays into WASM memory. `VerifyScriptArrayNetwork` additionally accepts
an explicit network (`0` mainnet, `1` testnet, `2` STN, `3` regtest,
`4` TeraTestNet/`teratestnet`, and `5` Tera Scaling Test Network/`tstn`;
SDKs may also expose `ttn` and `terratestnet` as aliases for network `4`).
`VerifyScriptBatchArray` accepts concatenated EF/height buffers plus offset
tables and returns a flat `Int32Array` of domain/code pairs in one JS/WASM call.
`VerifySpendArray` and `VerifySpendBatchArray` validate one input from ordinary
transaction bytes with its source script and satoshis supplied separately.
The batch APIs are intended for callers that already retain serialized bytes;
chunk very large workloads rather than constructing an unbounded packed buffer.
`VerifyScript` remains available through the compatibility vector API for
existing callers.

The generic signing and public-key creation helpers initialize lazily and
randomize their libsecp256k1 signing context from
`globalThis.crypto.getRandomValues`; they fail closed if a secure host RNG is
unavailable. Full builds can export verification-table snapshots for worker
instances. Imports authenticate the complete snapshot before installing it, so
wrong-build or corrupted tables are rejected even when their length is valid.

## Native and direct WASM benchmark controls

After a WASM build, benchmark the exact exported verifier without SDK
serialization overhead:

```bash
node module/typesbdk/wasm/benchmark.mjs 5000 11
```

The native control is `bench_verifyscript` in `module/example/`, built by the
regular native build (no dedicated configure or flag needed):

```bash
cmake --build /path/to/build --target bench_verifyscript
/path/to/build/x64/release/bench_verifyscript 5000 11
```

Note: `bench_verifyscript` links the canonical `bdk_core` with logging compiled
in - the code path real native consumers (gobdk, rustbdk) run - whereas the
WASM verifier is compiled with `DISABLE_LOGGING`. For an occasional
logging-free native measurement, use a throwaway build tree configured with
`-DCMAKE_CXX_FLAGS=-DDISABLE_LOGGING`; that is a one-off, not a committed
build mode.

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
