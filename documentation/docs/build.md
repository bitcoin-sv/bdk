# Development Build

This page describes how to build the Bitcoin Development Kit (BDK) and its language-binding
modules from source.

## Getting the source code

Building BDK requires the source for **both** the BDK repository and the `bitcoin-sv` (BSV)
repository. BDK does **not** vendor the BSV sources; it compiles a curated subset of them at build
time (see [Architecture overview](architecture.md) and
[Directory Structure](directories.md)).

The recommended layout places the two checkouts side by side:

```
<workspace>
   |-- bdk
   |-- bitcoin-sv      # pinned to the commit used by CI (see table below)
   |-- build           # out-of-tree build directory (created by you)
```

BDK locates the BSV checkout automatically (see
[How BDK finds the bitcoin-sv source](#how-bdk-finds-the-bitcoin-sv-source)).

## Dependencies & pinned versions

These are the versions and environments selected by the repository's workflows,
not universal minimum supported versions. CMake declares a 3.16 minimum; CI uses
the runner's CMake. Record the actual compiler and CMake versions when reproducing
a build. A newer version is not automatically a tested version.

| Input | Repository selection | Evidence |
|-------|----------------------|----------|
| bitcoin-sv | `879fc8b42168dd0e608dafd51b39c6dabad37d4d` | `build_bdk.yaml`, `DEFAULT_BITCOIN_SV_COMMIT` |
| Boost | 1.85.0 | `prebuild_dependancies.yaml`, `BOOST_VERSION` |
| OpenSSL, native only | 3.4.0 | `prebuild_dependancies.yaml`, `OPENSSL_VERSION` |
| Go | CI requests 1.24.9 except on ubuntu-22.04, where it assumes the runner supplies it; `go.mod` declares 1.24.0 | `build_bdk.yaml`, `Set up Go 1.24.9`; `module/gobdk/go.mod` |
| Rust | 1.98.1, with clippy and rustfmt | `module/rustbdk/rust-toolchain.toml`; identical copy in `test/rust` |
| C++ | C++20 | `cmake/BDKBuildSetting.cmake`, `CMAKE_CXX_STANDARD` |
| CMake | Minimum 3.16; CI does not pin a version | root `CMakeLists.txt` |
| Python | 3.x; Python package versions are not pinned by these workflows | `build_bdk.yaml`, Python setup and dependency installation |
| Emscripten, WASM only | 4.0.23 | `build_wasm.yaml`, `Install Emscripten 4.0.23` |
| Node.js, WASM tests | Must be on PATH when configuring; the workflow does not pin a version | `test/types/CMakeLists.txt`, `BDK_NODE_EXECUTABLE` |
| Native CI hosts | ubuntu-22.04, ubuntu-22.04-arm, macos-15 | `build_bdk.yaml`, build matrix |

The Rust gating workflow uses the pinned toolchain. The scheduled Rust toolchain
canary deliberately tests floating `stable`; the archive-refresh workflow also
installs stable. Do not substitute either for the gating pin.

### Prerequisites

Install a C++20 compiler, CMake, a build tool, Python 3, Go with cgo support, and
Rust. Rust is experimental but enabled by default; disable it explicitly with
`-DBUILD_MODULE_RUST=OFF` if it is not needed. With Rust enabled, missing `cargo`
or `rustc` is a configuration error.

Use Boost 1.85.0 and native OpenSSL 3.4.0 for CI-matching builds. The dependency
workflow builds static Boost with position-independent code and builds OpenSSL
with `no-shared no-tests no-docs no-dso no-engine`. The native Boost package includes
`chrono;filesystem;program_options;system;test;thread;circular_buffer;multi_index;property_tree;signals2;uuid;variant;multiprecision`.
WASM uses the smaller header set described below and does not link OpenSSL.
secp256k1 and univalue are built from the selected bitcoin-sv checkout; LevelDB is
optional and disabled by default.

Use one Python environment for the documentation packages:

```bash
python3 -m venv ../bdk-doc-venv
../bdk-doc-venv/bin/python -m pip install pytest junitparser mkdocs pymdown-extensions plantuml_markdown requests
export BDK_DOC_PYTHON="$(cd ../bdk-doc-venv/bin && pwd)/python"
```

These are the package names installed by CI. Versions are not locked by the CI
install command. Record installed versions when reporting a verification result.
Set `BOOST_ROOT` to the Boost installation and `OPENSSL_ROOT_DIR` to the OpenSSL
installation. Set `BSV_ROOT` to the pinned bitcoin-sv checkout. Put the compiler,
CMake, Go, Cargo and Python on PATH. The recipes below are run from the BDK root.

For Rust, install the checked-in toolchain from its package directory:

```bash
(cd module/rustbdk && rustup toolchain install)
cmp module/rustbdk/rust-toolchain.toml test/rust/rust-toolchain.toml
```

CMake's `core_doc` target is omitted with a warning if no single Python interpreter
can import all the documentation extensions. Set
`-DBDK_DOC_PYTHON_EXECUTABLE="$BDK_DOC_PYTHON"` to select the environment explicitly.
Mermaid diagrams render in the browser. Java is needed only for PlantUML diagrams;
the current pages contain no PlantUML diagrams.

## Building BDK (Linux / macOS)

Use a sibling `../build` directory for local builds. CI uses `build` inside its
checkout, which is still a separate CMake binary directory; these commands adapt
that recipe to the recommended local layout. Run them from the BDK root after
setting the prerequisites above. Use a fresh build directory for a clean build.

### Recommended sibling build

```bash
cmake -S . -B ../build -DCMAKE_BUILD_TYPE=Release \
  -DBSV_ROOT="$BSV_ROOT" -DBOOST_ROOT="$BOOST_ROOT" \
  -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR" \
  -DBDK_DOC_PYTHON_EXECUTABLE="$BDK_DOC_PYTHON" \
  -DBUILD_MODULE_GOLANG_INSTALL_INSOURCE=ON \
  -DBUILD_MODULE_RUST_INSTALL_INSOURCE=ON
cmake --build ../build --parallel 4
(cd ../build && ctest --output-on-failure)
```

Both install-insource switches write merged archives into their module source
directories. They default to ON and are appropriate when testing consumers of the
freshly built archives. Use a disposable checkout to verify this recipe without
replacing local artifacts. The Go CTest setup also regenerates `test/golang/vendor`
and runs `go mod tidy`; an out-of-tree CMake build does not make these steps read-only.

For a C++-only debugging session, use the separate recipe in
[Debugging transaction validation](debug_transaction.md#build-the-native-debug-executable).
An unset build type defaults to Release for single-configuration generators.

### Make-based equivalent

With the same prerequisites, a fresh sibling directory and a Makefile generator:

```bash
mkdir ../build-make
(cd ../build-make && cmake -G "Unix Makefiles" ../bdk -DCMAKE_BUILD_TYPE=Release && make -j4 && ctest --output-on-failure)
```

This is also an out-of-tree build. It assumes the checkout directory is named
`bdk`; use the explicit `-S`/`-B` recipe above otherwise.

### Packaging

After the Release build and tests complete:

```bash
(cd ../build && cpack -G TGZ)
```

Inspect the generated archive before distributing it. Packaging success alone
does not prove that all installed headers or optional module components are usable.

## CMake build options

The root `CMakeLists.txt` and the `cmake/` helpers expose the following options.
Defaults appear in the table's second column.

| Option | Default | Effect |
|--------|---------|--------|
| `BDK_LOG_BSV_FILES` | `ON` | Log the bitcoin-sv files used to build core. |
| `BDK_BUILD_LEVELDB` | `OFF` | Build the bundled leveldb. |
| `BDK_BUILD_UNIVALUE` | `ON` | Build the bundled univalue. |
| `BDK_BUILD_CORE_ONLY` | `OFF` | Build only `bdk_core`; skip modules, tests and docs. |
| `BDK_BUILD_MODULES` | `ON` | Build the language-binding modules. |
| `BDK_BUILD_CORE_TESTS` | `ON` | Build the C++ core tests. |
| `BDK_BUILD_CORE` | `ON` | Build the canonical native `bdk_core`. `OFF` skips core and force-disables everything that links or installs it (modules, tests, install gates). |
| `BDK_BUILD_WASM` | `OFF` | Build the typesbdk WASM module as a standalone build. Also adds `test/types`, which registers the module's CTest entries. Requires Emscripten and `BDK_BUILD_CORE=OFF` (see below). |
| `BUILD_MODULE_GOLANG` | `ON` | Build and test the Golang (cgo) module. |
| `BUILD_MODULE_GOLANG_INSTALL_INSOURCE` | `ON` | Install the standalone GoBDK static lib into `module/gobdk` (used by CI). |
| `BUILD_MODULE_RUST` | `ON` | Build the experimental Rust C ABI archive and register Rust tests; requires Cargo and rustc. |
| `BUILD_MODULE_RUST_INSTALL_INSOURCE` | `ON` | Copy the merged archive to `module/rustbdk/bdk-sys/lib`. |
| `BDK_INSTALL_CORE_ARCHIVE` | `ON` | Install the canonical core archive. |
| `BDK_INSTALL_BSV_HEADERS` | `ON` | Install BSV headers and bundled support files. |

Disabling a parent option forces dependent options OFF. In particular, `BDK_BUILD_CORE=OFF` disables native modules, native tests and native install gates; `BDK_BUILD_CORE_ONLY=ON` also omits documentation. See the flag-hierarchy blocks in the root `CMakeLists.txt`.

> `BDK_BUILD_WASM` now activates the **standalone** WASM build described below; in the
> pre-refactor tree the same name selected an in-tree overlay build that mutated the shared
> core, and its semantics changed with the standalone architecture (`-DBDK_BUILD_WASM=ON`
> without Emscripten is now a configure error). The former
> `BDK_BUILD_NATIVE_VERIFY_BENCHMARK` option **no longer exists** and nothing reads it; the
> VerifyScript benchmark is the ordinary `bench_verifyscript` executable in `module/example/`,
> built by the regular native build.

Additional build-facing variables live in the `cmake/` helpers and the GoBDK module:

- **`BSV_ROOT`** — path to the bitcoin-sv checkout, accepted as a CMake variable or environment variable. The resolved path is cached as `BDK_BSV_ROOT_DIR`. The root source comment advertising `BDK_BSV_SRC_ROOT` is stale; the resolver does not read that name.
- **`CUSTOM_BOOST_ROOT`** — overrides `BOOST_ROOT` (`cmake/modules/FindBoostHelper.cmake`).
- **`CUSTOM_GOBDK_OS_ARCH`** — overrides the suffix of the merged Go archive. It does not select a compiler architecture or provide cross-compilation. Only use a suffix matching the actual compiled target.
- **`CMAKE_BUILD_TYPE`** — on Unix (single-config) generators, defaults to `Release` (forced, with a
  warning) when unset (`cmake/BDKInit.cmake`). Use `-DCMAKE_BUILD_TYPE=Debug` for a debug build.
- **`CUSTOM_SYSTEM_OS_NAME`** — used for packaging to embed a precise OS name in the installer file
  name (`CMakeLists.txt`), e.g. `-DCUSTOM_SYSTEM_OS_NAME=Ubuntu`.
- **`BDK_DOC_PYTHON_EXECUTABLE`** — Python interpreter used by CMake to find MkDocs and all enabled Markdown extensions.

## Building the Rust binding (experimental)

The Rust C ABI is independent of GoBDK. This separate build keeps its archive in
the build tree and passes its location explicitly to Cargo:

```bash
cmake -S . -B ../build-rust -DCMAKE_BUILD_TYPE=Release \
  -DBSV_ROOT="$BSV_ROOT" -DBOOST_ROOT="$BOOST_ROOT" \
  -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR" \
  -DBUILD_MODULE_GOLANG=OFF -DBUILD_MODULE_GOLANG_INSTALL_INSOURCE=OFF \
  -DBUILD_MODULE_RUST=ON -DBUILD_MODULE_RUST_INSTALL_INSOURCE=OFF
cmake --build ../build-rust --target MergeBDKFFI --parallel 4
export BDK_LIB_DIR="$(cd ../build-rust/x64/release && pwd)"
(cd module/rustbdk && cargo build --locked --workspace --all-targets)
(cd module/rustbdk && cargo test --locked --workspace)
(cd module/rustbdk && cargo clippy --locked --workspace --all-targets -- -D warnings)
(cd module/rustbdk && cargo fmt --all -- --check)
```

The `x64` output directory denotes the 64-bit build layout, including supported
ARM64 hosts; the archive name still records its OS/architecture. `BDK_LIB_DIR`
must contain the matching `libbdkffi_<os_arch>.a`. Setting it explicitly prevents
an older archive in `bdk-sys/lib` from taking precedence. Run Cargo from inside
`module/rustbdk` so rustup selects its checked-in toolchain.

CI's archive-refresh workflow can publish archives under the `rustbdk-prebuilt`
release tag. The Cargo build script does not download them. These instructions
use a local source build. See [Rust usage](index.md#rust-experimental).

## The standalone WASM build

The WASM verifier needs aggressive size optimization (no OpenSSL, excluded upstream sources and module-local bigint and memory-cleanse replacements, runtime-reconstructed secp256k1 tables) that
must not deform the regular build: the canonical native `bdk_core` is never modified by any
module. Instead, `core/bdk-core-recipe.cmake` exposes the reusable core recipe — the curated
source lists plus the `bdk_add_core_library()` factory — and the WASM module uses it to build
its own `bdk_core_wasm` variant inside `module/typesbdk/wasm/`. There is exactly one canonical
native core; a specialized consumer builds its own variant in its own directory.

The reproducible entry point is `module/typesbdk/wasm/build.sh` (Emscripten 4.0.23). It is a
facility script that only automates the clean configure and build commands — it runs no test and
publishes nothing; the environment must be prepared before it runs, exactly like the native
build. Provide:

- Emscripten 4.0.23 (activate its `emsdk_env.sh`);
- `BOOST_ROOT` pointing at a Boost 1.85.0 install — the prebuilt `depcy` wasm package or a local
  install. CMake accepts either a directory that contains `boost/` directly or one that contains
  `include/boost/`, and enforces `BOOST_VERSION 108500`;
- a bitcoin-sv checkout — the sibling `../bitcoin-sv`, or `BSV_ROOT` (see *How BDK finds the
  bitcoin-sv source* below).

Emscripten needs a writable cache even when the SDK is already installed. If the
SDK is read-only, source its environment script first, then copy its existing cache
to a **new** writable sibling directory before configuring:

```bash
source /path/to/emsdk/emsdk_env.sh
cp -a "$EMSDK/upstream/emscripten/cache" ../emscripten-cache
export EM_CACHE="$(cd ../emscripten-cache && pwd)"
```

Keep `EM_CACHE` set in each build/test shell. This reuses the installed SDK's cache;
it does not install or update the toolchain. A cache lock-file permission error can
surface as CMake's `Unable to find Threads library`; inspect the underlying compiler
diagnostic before treating it as a missing dependency.

```bash
# First activate the installed Emscripten 4.0.23 SDK:
source /path/to/emsdk/emsdk_env.sh
command -v node
curl --fail --location -o ../dependancies_wasm.tar.gz \
  https://github.com/bitcoin-sv/bdk/releases/download/depcy/dependancies_wasm.tar.gz
mkdir -p ../build-wasm-deps
tar -xzf ../dependancies_wasm.tar.gz -C ../build-wasm-deps
export BOOST_ROOT="$(cd ../build-wasm-deps/dependancies_wasm/boost_1.85.0 && pwd)"
export BDK_WASM_BUILD_DIR="$(cd .. && pwd)/build-wasm"
export BDK_WASM_CLEAN=0
module/typesbdk/wasm/build.sh
(cd "$BDK_WASM_BUILD_DIR" && ctest -N && ctest --output-on-failure)
```

Replace `/path/to/emsdk` with the installed SDK path. The CI setup clones the `4.0.23` emsdk tag, installs and activates `4.0.23`, then sources `emsdk_env.sh`. Use a fresh `build-wasm` directory for a clean build. The script defaults `BDK_WASM_CLEAN` to `1`, which deletes its selected build directory; this example uses `0` to preserve an existing tree. Never point it at a source directory. Before testing, confirm that `ctest -N` lists all twelve names below; a green partial suite is insufficient.

```text
wasm_build_optimized
wasm_size_gate
wasm_functional_node
wasm_functional_browser
wasm_functional_umd
wasm_functional_slim_umd
wasm_secp256k1_build
wasm_secp256k1_tests
wasm_secp256k1_noverify_tests
wasm_secp256k1_first_call_parity
wasm_secp256k1_verify_corpus
wasm_secp256k1_verify_parity_node
```

Three commands, one responsibility each:

- **`build.sh` builds.** Everything stays inside the selected build tree (`../build-wasm/` in this example; the script default is `<repo>/build-wasm`); it never writes
  the eight tracked artifacts and never runs a test. Override the build tree with
  `BDK_WASM_BUILD_DIR` or the job count with `BDK_WASM_JOBS`. The preflight covers build tools
  only (`cmake emcmake emcc em++ emar emranlib make`).
- **`ctest` validates.** The twelve entries are registered by `test/types/CMakeLists.txt`; see
  [`test/types/README.md`](https://github.com/bitcoin-sv/bdk/blob/master/test/types/README.md)
  for what each covers. `node` must be on `PATH` at **configure** time or ten of the twelve are
  not registered, with CMake warnings — `build.sh` does not preflight it.
- **`cmake --build ../build-wasm --target bdk_wasm_install_insource` publishes.** This is the only
  thing that writes the eight committed artifacts under `module/typesbdk/wasm/`, and only the CI
  commit path invokes it (dispatch `build_bdk.yaml` with `commit-wasm-artifacts` ticked
  and the default bitcoin-sv pin selected). The
  tracked artifacts are a refreshed-on-demand convenience, exactly like `module/gobdk`'s
  `libGoBDK_*.a`: they may lag the sources between refreshes, and no pull-request gate compares
  them against a fresh build.

The former `BDK_WASM_RUN_TESTS` and `BDK_WASM_UPDATE_COMMITTED_ARTIFACTS` environment variables no
longer exist; anything still setting them has no effect. A direct configure without `build.sh` is
also supported:

```console
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -S . -B ../build-wasm-direct -DCMAKE_BUILD_TYPE=Release \
  -DBDK_BUILD_CORE=OFF -DBDK_BUILD_WASM=ON \
  -DBSV_ROOT="$BSV_ROOT" -DBOOST_ROOT="$BOOST_ROOT" \
  -DSECP256K1_ASM=OFF -DSECP256K1_ECMULT_WINDOW_SIZE=15 -DSECP256K1_ECMULT_GEN_KB=2 \
  -DSECP256K1_TEST_OVERRIDE_WIDE_MULTIPLY=int64 -DSECP256K1_BUILD_BENCHMARK=OFF
cmake --build ../build-wasm-direct --target bdk_wasm --parallel 4
```

The wasm Boost package pins this minimal `BOOST_INCLUDE_LIBRARIES` component set:

```text
multiprecision;chrono;uuid;variant;thread;filesystem;signals2;multi_index
```

The `multiprecision` component is required by the WASM bigint backend. This list is
defined once, in `prebuild_dependancies.yaml` (`WASM_BOOST_INCLUDE_LIBRARIES`) — the workflow
that builds the package. To change it, re-run the add-and-prune derivation (install a candidate
list, build, add components on missing-header errors, then remove each component one at a time
and keep only the necessary ones) and pin the converged result — never hand-edit the list.

### Boost multiprecision and the big-int parity suite

`test_big_int_boost` tests whether the wasm Boost bigint backend matches the OpenSSL-backed native
implementation of the consensus-critical `bsv::bint` arithmetic. The Boost **multiprecision**
headers are a **required native dependency** (header-only; the official prebuilt dependency
packages carry them). With native core tests enabled, a Boost install without them fails configuration with an
actionable error — use the refreshed `depcy` packages, or add `multiprecision` to your Boost
install's `BOOST_INCLUDE_LIBRARIES`.

## How BDK finds the bitcoin-sv source

`HelpFindBSVSource` calls `bdkFindBSVDir` in `cmake/modules/FindBSVSourceHelper.cmake`
to resolve the BSV checkout in
this priority order:

1. `-DBSV_ROOT=<path>` on the CMake command line.
2. The `BSV_ROOT` environment variable.
3. `<bdk>/bitcoin-sv` (a checkout inside the BDK tree).
4. `<bdk>/../bitcoin-sv` (a sibling checkout — the recommended local layout).
5. **Last resort:** a `git clone` of the BSV repository.

> ⚠️ The last-resort auto-clone uses **SSH** (`git@github.com:bitcoin-sv/bitcoin-sv.git`) and checks out the remote default branch at clone time — there is **no commit pin**
> (`FindBSVSourceHelper.cmake:69-75`). This silently diverges from CI, which clones over **HTTPS**
> and checks out the pinned commit `879fc8b42168dd0e608dafd51b39c6dabad37d4d`
> (`build_bdk.yaml`, `Check out bitcoin-sv`).
>
> **Recommendation:** always supply your own `bitcoin-sv` checkout pinned to that commit and point
> `BSV_ROOT` at it, e.g.:
>
> For a new checkout, use the commands below. If `../bitcoin-sv` already exists, verify its commit with `git -C ../bitcoin-sv rev-parse HEAD`; reuse it only if it is the required pin and has no local source changes. Otherwise create a separate pinned checkout and set `BSV_ROOT` to it.
>
> ```bash
> git clone https://github.com/bitcoin-sv/bitcoin-sv.git ../bitcoin-sv
> git -C ../bitcoin-sv checkout --detach 879fc8b42168dd0e608dafd51b39c6dabad37d4d
> export BSV_ROOT="$(cd ../bitcoin-sv && pwd)"
> ```
> On a correct configuration CMake prints `Found Bitcoin SV source code at BDK_BSV_ROOT_DIR=[…]`
> for the pinned path (rather than a clone message).

## Consuming the GoBDK module (cgo)

The Golang binding is built with cgo and links against a prebuilt static library
(`libGoBDK_<os>_<arch>.a`) committed under `module/gobdk/bdkcgo/` for every supported
platform/arch — Linux (`x86_64`, `aarch64`) and macOS (`arm64`, `x86_64`). Because those archives
are checked into git, downstream consumers can simply `go get` the module without building C++
themselves. See [Architecture overview](architecture.md#gobdk-prebuilt-static-libraries) for how
those archives are regenerated.

The static libraries depend on a few system runtime libraries (mainly the C/C++ runtime). A `go`
build may link successfully but fail to *run* if those are missing. Inspect them with `ldd` (Linux)
or `otool -L` (macOS):

```console
mkdir -p ../build-tools
(cd module/gobdk && go build -buildvcs=false -mod=mod -o ../../../build-tools/woc ./cmd/woc)
# Linux:
ldd ../build-tools/woc
# macOS: use this instead of ldd:
# otool -L ../build-tools/woc
../build-tools/woc tx --help
```

`-buildvcs=false` disables VCS stamping so the tool also builds from an exported
source tree without usable Git metadata.

If you ship a Go binary in a `scratch` container, copy over those shared libraries.

Consuming GoBDK solely from an installed BDK package is not a supported recipe in the current repository integration. Go consumers still need the Go module and its module-relative cgo headers/archive layout. The cgo platform files explicitly add `module/gobdk/bdkcgo` to the library search path and link `libGoBDK_<os_arch>.a`, whereas the native install rule places the merged archive under `lib/bdkcgo`. There is no install-prefix selection option in those platform files. Adding another `-L` directory does not by itself verify which archive was selected; this is a search-path directive, not an absolute archive filename or proof that linker overrides are impossible. The command above uses the module's archive; when verifying a fresh native build, use a disposable checkout with `BUILD_MODULE_GOLANG_INSTALL_INSOURCE=ON`. Inspect the resulting binary's shared-library dependencies before deployment.

## Building the documentation

From the repository root, using the Python environment prepared above:

```bash
"$BDK_DOC_PYTHON" -m mkdocs build --config-file documentation/mkdocs.yml \
  --clean --strict --site-dir "$(cd .. && pwd)/build/generated/core_doc"
```

Strict mode turns warnings into errors; it does not establish factual correctness
or guarantee that every link fragment and browser-rendered diagram works. Inspect
the generated pages and local links as well.

The CMake integration is:

```bash
cmake --build ../build --target core_doc
```

It runs a clean MkDocs build, without `--strict`. Run the explicit strict command
as the documentation acceptance gate. If the target is absent, check the configure
log for `Deactivate building documentation`, install all extensions into one
Python environment, and reconfigure with `BDK_DOC_PYTHON_EXECUTABLE` set to it.
`BDK_BUILD_CORE_ONLY=ON` and `BDK_BUILD_CORE=OFF` also omit this target.

To view the result:

```bash
"$BDK_DOC_PYTHON" -m http.server --directory ../build/generated/core_doc 8000
```

Open `http://localhost:8000`, inspect the navigation and diagrams, then stop the
server with Ctrl-C. Mermaid is rendered client-side. Java is required only if
PlantUML diagrams are added; in that case put `cmake/doc_tools` on PATH.

## Keeping this page in sync

Cross-check these workflow roles when changing the build instructions:

| Workflow | Role |
|----------|------|
| `build_bdk.yaml` | Native build/test matrix; coordinated Go/WASM artifact publication gate |
| `build_wasm.yaml` | Emscripten build, complete WASM CTest suite, direct-configure smoke |
| `prebuild_dependancies.yaml` | Boost/OpenSSL versions and dependency package contents |
| `rust_bdk.yaml` | Pinned Rust gating toolchain, independent archive build and Cargo checks |
| `rust_toolchain_canary.yaml` | Scheduled/manual floating-stable lint check |
| `rustbdk_archive_refresh.yaml` | Rust archive build and optional release publication |
| `test_whatever.yaml` | Diagnostic tool/version reporting; not a BDK build |

Also check the root CMake options, `module/gobdk/go.mod`, both Rust toolchain
files, and the module CMake files. Distinguish a version requested by a workflow
from a version assumed to exist on a runner. Run build/test commands locally in
a disposable checkout; do not dispatch publication workflows for documentation checks.

## Windows (experimental / unsupported)

Windows is absent from the native CI matrix, and no Windows GoBDK archive is
shipped. The following C++ configuration is experimental and is not a supported
Rust or Go binding recipe. Run it from a Visual Studio 2022 developer command
prompt with C++20 support and matching Boost/OpenSSL installations:

```bat
cmake -G "Visual Studio 17 2022" -A x64 -S . -B ..\build-windows ^
  -DBUILD_MODULE_GOLANG=OFF -DBUILD_MODULE_GOLANG_INSTALL_INSOURCE=OFF ^
  -DBUILD_MODULE_RUST=OFF -DBUILD_MODULE_RUST_INSTALL_INSOURCE=OFF ^
  -DBSV_ROOT="C:\path\to\bitcoin-sv" ^
  -DBOOST_ROOT="C:\path\to\boost_1.85.0" ^
  -DOPENSSL_ROOT_DIR="C:\path\to\openssl_3.4.0"
cmake --build ..\build-windows --config Release --parallel 4
cd ..\build-windows
ctest --output-on-failure -C Release
```

Replace the dependency paths with actual installations and pin bitcoin-sv as above.
This recipe requires a Windows validation run before it can be described as tested.
