## Build and validate the WASM module

See also [`module/typesbdk/wasm/README.md`](../wasm/README.md) for what that directory
contains (build inputs and the eight committed artifacts) and
[`test/types/README.md`](../../../test/types/README.md) for the twelve CTest entries.

`build.sh` is the reproducible WASM entry point. It is a facility script that
automates the clean configure and build commands; it runs no test and publishes
nothing. The environment must be prepared before it runs. Use Emscripten 4.0.23, set
`BOOST_ROOT` to a Boost 1.85.0 install, and provide a `bitcoin-sv` checkout (the
sibling `../bitcoin-sv`, or `BSV_ROOT`), then run it from the BDK root:

```bash
source /path/to/emsdk/emsdk_env.sh
# Prepare Boost 1.85.0 (here from the prebuilt minimal depcy package):
curl --fail --location -o /tmp/dependancies_wasm.tar.gz \
  "https://github.com/bitcoin-sv/bdk/releases/download/depcy/dependancies_wasm.tar.gz"
mkdir -p build-wasm-deps && tar -xzf /tmp/dependancies_wasm.tar.gz -C build-wasm-deps
export BOOST_ROOT="$PWD/build-wasm-deps/dependancies_wasm/boost_1.85.0"
module/typesbdk/wasm/build.sh
( cd build-wasm && ctest --output-on-failure )
```

`build.sh` performs a clean standalone module build (`-DBDK_BUILD_CORE=OFF
-DBDK_BUILD_WASM=ON`) that assembles the module's own `bdk_core_wasm` variant
from the shared core recipe. Validation is the separate `ctest` run above: it
executes libsecp256k1's verified and non-verified unit suites against the
substituted runtime tables (the *exhaustive* suite is not registered), the real
positive and negative transaction vectors, the verification-corpus parity leg and
the bundle size gate — twelve entries in total, registered by
`test/types/CMakeLists.txt`. `node` must be on `PATH` at **configure** time or ten of
them are silently not registered.
The verifier-only WASM build does not require or link OpenSSL: it uses
header-only multiprecision and a minimal memory-cleanse implementation. CMake
discovers bitcoin-sv (`ENV{BSV_ROOT}` or the sibling `../bitcoin-sv`) and Boost
(`ENV{BOOST_ROOT}` — either a directory containing `boost/` directly or one
containing `include/boost/`), and enforces the pinned Boost version 1.85.0. The
build output defaults to `build-wasm/`; set `BDK_WASM_BUILD_DIR` or
`BDK_WASM_JOBS` to override the location or job count.

The production artifact uses libsecp256k1's 32-bit-limb arithmetic backend
(`int64` in libsecp terminology), whole-program LTO for the hot verifier, and a
converged Binaryen `-O4` pass. It retains the full W15 verification precompute
window without shipping its serialized megabyte-scale tables: the table is
reconstructed lazily on the first verification using one field inversion and
the curve endomorphism for the second lane. On wasm32 the 32-bit backend avoids
Clang's much slower lowering of native `__int128` arithmetic.

Successful `ctest` output includes:

```text
ok - bdk-core.mjs mainnet-p2pkh-block-620940: domain=0 code=0
ok - bdk-core.mjs mainnet-p2pkh-corrupt-signature: domain=1 code=39
```

The validated Node (`bdk-core.mjs`, `bdk-core.wasm`) and browser/worker
(`bdk-core.browser.mjs`, `bdk-core.browser.wasm`) artifacts are produced in the
build tree, together with a classic-script/UMD loader (`bdk-core.umd.js`,
`bdk-core.umd.wasm`). No build ever modifies the eight tracked artifacts committed
beside the build script: writing them is a CMake target of its own,
`cmake --build build-wasm --target bdk_wasm_install_insource`, invoked only by the CI
commit path. To refresh the committed copies, dispatch
`.github/workflows/build_bdk.yaml` with the **`commit-wasm-artifacts`** box ticked; the
bot commits them with a `[WasmBDKUpdate]` marker. They are a refreshed-on-demand
convenience, so they may lag the sources — there is no reproducibility gate on pull
requests. The loader split keeps Node imports out of browser bundler graphs while
preserving an identical verification ABI.

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

Benchmark the exact exported verifier without SDK serialization overhead. The
benchmark imports the **committed** `module/typesbdk/wasm/bdk-core.mjs`, so it needs no
build tree and no Emscripten:

```bash
node test/types/benchmark.mjs 5000 11
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
