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

> **These versions are authoritative because CI builds and tests against them.** The single source
> of truth is the GitHub Actions workflow files under `.github/workflows/`. **Update this table
> whenever those workflow files change** (see the
> [version cross-check](#keeping-this-page-in-sync) below).

| Dependency  | Version                                            | Where pinned (source of truth)                                                            |
|-------------|----------------------------------------------------|-------------------------------------------------------------------------------------------|
| Go          | **1.24.9**                                          | `build_bdk.yaml:53-58` (ubuntu-22.04 already ships it); `module/gobdk/go.mod` declares `go 1.24.0` |
| Boost       | **1.85.0**                                          | `prebuild_dependancies.yaml:18`; consumed at `build_bdk.yaml:98,107,114`                  |
| OpenSSL     | **3.4.0**                                           | `prebuild_dependancies.yaml:19`; consumed at `build_bdk.yaml:99,108,115`                  |
| CMake       | **minimum 3.16** — *not pinned by CI* (runner default; newer is fine) | `cmake_minimum_required(VERSION 3.16)` in `CMakeLists.txt:31`                              |
| bitcoin-sv  | commit **`879fc8b42168dd0e608dafd51b39c6dabad37d4d`** | `build_bdk.yaml:22` (`DEFAULT_BITCOIN_SV_COMMIT`); cloned from `https://github.com/bitcoin-sv/bitcoin-sv.git` (`build_bdk.yaml:79`) |
| Python      | **3.x**                                             | `build_bdk.yaml:51`                                                                       |
| C++ standard| **C++20**                                           | required by core                                                                          |
| OS / arch   | **ubuntu-22.04**, **ubuntu-22.04-arm**, **macos-15** | `build_bdk.yaml:29-31` — **there is no Windows entry in the CI matrix**                    |

CMake is intentionally **not** pinned by CI: the workflows use whatever CMake ships on the runner.
Only the `3.16` floor in `CMakeLists.txt` is binding; any newer CMake works.

## Prerequisites

### General

- **Python 3.x** (`build_bdk.yaml:51`). The following Python packages are needed to build the
  documentation and run the tests — note `requests` is required by CI:
  ```console
  python -m pip install pytest junitparser mkdocs pymdown-extensions plantuml_markdown requests
  ```
  (matches the CI install line at `build_bdk.yaml:90`.)
- A **C++20**-compatible compiler. CI uses the default toolchains of `ubuntu-22.04` and `macos-15`
  rather than pinning a specific compiler version.
- **CMake** ≥ 3.16 (CI uses the runner default; newer is fine).
- **Boost 1.85.0**. On *macOS*, Boost 1.86 breaks the clang build — `boost::uuids::uuid::data_type`
  changed, making the `reinterpret_cast` at `src/serialize.h` fail — so stay on 1.85.0.
- **OpenSSL 3.4.0**. (Older 3.0.x releases such as 3.0.14 are known to compile, but 3.4.0 is the
  version CI builds against and the one you should use.)
- **Go 1.24.9** — required to build and test the GoBDK language binding. The `gobdk` module declares
  `go 1.24.0` in `module/gobdk/go.mod`; CI pins `1.24.9`.
- A **`bitcoin-sv` checkout** pinned to commit `879fc8b42168dd0e608dafd51b39c6dabad37d4d`
  (see [How BDK finds the bitcoin-sv source](#how-bdk-finds-the-bitcoin-sv-source)).

Static libraries (Boost and OpenSSL) must be compiled with `fPIC` on.

### Documentation toolchain (optional)

Building the documentation site requires the Python packages `mkdocs`, `pymdown-extensions`,
`plantuml_markdown` (included in the `pip install` line above). If those packages or `mkdocs` are
missing, the CMake `core_doc` target is **silently skipped** with only a warning
(`documentation/CMakeLists.txt:19-34`).

The site's diagrams are authored in **Mermaid**, which renders client-side in the browser and needs
no extra tooling at build time. The `plantuml_markdown` extension is still enabled and the CMake
`core_doc` target stages `plantuml.jar` (via the launcher in `cmake/doc_tools/`), so a **Java
runtime is needed only if you add PlantUML (`@startuml`) diagrams** — the current pages do not, so
Java is **not** required to build the docs as they stand.

See [Building the documentation](#building-the-documentation).

### Environment variables

- Make sure the CMake `bin` directory is on `PATH`.
- Make sure the Python 3 directory is on `PATH`.
- Set `OPENSSL_ROOT_DIR` to where OpenSSL is installed.
- Set `BOOST_ROOT` to where Boost is installed.
- Optionally set `BSV_ROOT` to your `bitcoin-sv` checkout (recommended — see below).

## Building BDK (Linux / macOS)

These commands mirror what CI runs (`build_bdk.yaml:121-126`). Run them from the **root of the
`bdk` checkout**.

### Recommended (out-of-tree, matches CI)

```console
# 1. Clean any previous build artifacts and the Go vendor tree (recommended clean-build step)
rm -fR ./build/* && rm -fR test/golang/vendor/*

# 2. Configure (out-of-tree). The INSOURCE flag installs the standalone GoBDK static lib
#    back into module/gobdk so `go get` consumers can use it.
cmake -B build -S "." -DBUILD_MODULE_GOLANG_INSTALL_INSOURCE=ON

# 3. Build
cmake --build build --parallel 4

# 4. Test
ctest --output-on-failure --test-dir build
```

The vendor/build wipe in step 1 matches both CI (`build_bdk.yaml:122`) and the canonical local
build; run it whenever you want a clean build.

For a debug build, add `-DCMAKE_BUILD_TYPE=Debug` to the configure step. On Unix generators, if
`CMAKE_BUILD_TYPE` is left unset BDK **forces it to `Release`** with a warning
(`cmake/BDKInit.cmake:29-31`).

### Alternative (in-tree `make`)

The older in-tree form still works but does not match CI and is not recommended:

```console
# from a separate build directory alongside the bdk checkout
cmake ../bdk && make -j8
ctest
```

### Packaging

```console
cpack -G TGZ        # Linux/macOS tarball
```

## CMake build options

The root `CMakeLists.txt` (lines 34-44) and the `cmake/` helpers expose the following options.
Defaults are shown in parentheses.

| Option | Default | Effect |
|--------|---------|--------|
| `BDK_LOG_BSV_FILES` | `ON` | Log the bitcoin-sv files used to build core. |
| `BDK_BUILD_LEVELDB` | `OFF` | Build the bundled leveldb. |
| `BDK_BUILD_UNIVALUE` | `ON` | Build the bundled univalue. |
| `BDK_BUILD_CORE_ONLY` | `OFF` | Build only `bdk_core`; skip modules, tests and docs. |
| `BDK_BUILD_MODULES` | `ON` | Build the language-binding modules. |
| `BDK_BUILD_CORE_TESTS` | `ON` | Build the C++ core tests. |
| `BUILD_MODULE_GOLANG` | `ON` | Build and test the Golang (cgo) module. |
| `BUILD_MODULE_GOLANG_INSTALL_INSOURCE` | `ON` | Install the standalone GoBDK static lib into `module/gobdk` (used by CI). |

Additional build-facing variables live in the `cmake/` helpers and the GoBDK module:

- **`BSV_ROOT`** — path to the bitcoin-sv checkout. This is the **real** flag
  (`cmake/modules/FindBSVSourceHelper.cmake:42-54`); the resolver stores the result in the internal
  cache variable `BDK_BSV_ROOT_DIR`.
  > ⚠️ **Known bug:** the comment at `CMakeLists.txt:54` advertises `-DBDK_BSV_SRC_ROOT=...`, but
  > that name is **never read** by the resolver. Use `BSV_ROOT` (or the `BSV_ROOT` environment
  > variable) instead.
- **`CUSTOM_BOOST_ROOT`** — overrides `BOOST_ROOT` (`cmake/modules/FindBoostHelper.cmake:138-139`).
- **`CUSTOM_GOBDK_OS_ARCH`** — overrides the detected OS/arch suffix used to pick/name
  `libGoBDK_<os>_<arch>.a` (`module/gobdk/bdkcgo/CMakeLists.txt:129-137`). For example, set
  `-DCUSTOM_GOBDK_OS_ARCH=darwin_x86_64` to cross-target Intel macOS from a `macos-15` arm runner.
- **`CMAKE_BUILD_TYPE`** — on Unix (single-config) generators, defaults to `Release` (forced, with a
  warning) when unset (`cmake/BDKInit.cmake:29-31`). Use `-DCMAKE_BUILD_TYPE=Debug` for a debug build.
- **`CUSTOM_SYSTEM_OS_NAME`** — used for packaging to embed a precise OS name in the installer file
  name (`CMakeLists.txt:22-23,28`), e.g. `-DCUSTOM_SYSTEM_OS_NAME=Ubuntu`.

## How BDK finds the bitcoin-sv source

`HelpFindBSVSource` (`cmake/modules/FindBSVSourceHelper.cmake:42-83`) resolves the BSV checkout in
this priority order:

1. `-DBSV_ROOT=<path>` on the CMake command line.
2. The `BSV_ROOT` environment variable.
3. `<bdk>/bitcoin-sv` (a checkout inside the BDK tree).
4. `<bdk>/../bitcoin-sv` (a sibling checkout — the recommended local layout).
5. **Last resort:** a `git clone` of the BSV repository.

> ⚠️ The last-resort auto-clone uses **SSH** (`git@github.com:bitcoin-sv/bitcoin-sv.git`) and checks
> out **whatever `master` HEAD happens to be** — there is **no commit pin**
> (`FindBSVSourceHelper.cmake:69-75`). This silently diverges from CI, which clones over **HTTPS**
> and checks out the pinned commit `879fc8b42168dd0e608dafd51b39c6dabad37d4d`
> (`build_bdk.yaml:79-80`).
>
> **Recommendation:** always supply your own `bitcoin-sv` checkout pinned to that commit and point
> `BSV_ROOT` at it, e.g.:
> ```console
> git -C ../bitcoin-sv fetch
> git -C ../bitcoin-sv checkout 879fc8b42168dd0e608dafd51b39c6dabad37d4d
> cmake -B build -S "." -DBSV_ROOT=../bitcoin-sv -DBUILD_MODULE_GOLANG_INSTALL_INSOURCE=ON
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
cd module/gobdk && go build -o test_woc ./cmd/woc && ldd test_woc && cd ../..
```

If you ship a Go binary in a `scratch` container, copy over those shared libraries.

When linking GoBDK from an installed BDK package, point cgo at the install tree
(`BDK_INSTALL_ROOT`):

On Linux:
```console
export CGO_CFLAGS="-I${BDK_INSTALL_ROOT}/include ${CGO_CFLAGS}"
export CGO_LDFLAGS="-L${BDK_INSTALL_ROOT}/lib -L${BDK_INSTALL_ROOT}/bin ${CGO_LDFLAGS}"
export LD_LIBRARY_PATH="${BDK_INSTALL_ROOT}/bin:${LD_LIBRARY_PATH}"
```

On macOS:
```console
export CGO_CFLAGS="-I${BDK_INSTALL_ROOT}/include ${CGO_CFLAGS}"
export CGO_LDFLAGS="-L${BDK_INSTALL_ROOT}/lib -L${BDK_INSTALL_ROOT}/bin -Wl,-rpath,${BDK_INSTALL_ROOT}/bin ${CGO_LDFLAGS}"
```

## Building the documentation

The documentation is a real, buildable mkdocs artifact. From the `documentation/` directory:

```console
python -m pip install mkdocs pymdown-extensions plantuml_markdown
mkdocs build --clean --strict   # --strict turns broken nav/links into build failures
mkdocs serve                    # optional: visual check at http://localhost:8000
```

`--strict` is the key gate. The current pages use Mermaid (client-side) for diagrams, so no Java or
`plantuml.jar` is needed for this build. If you add PlantUML (`@startuml`) diagrams, also put the
plantuml launcher (which wraps `plantuml.jar` and requires Java) on `PATH`:
`export PATH="$PWD/../cmake/doc_tools:$PATH"`.

The equivalent in the CMake build is the `core_doc` target
(`cmake --build build --target core_doc`), which runs `mkdocs build -c`
(`documentation/CMakeLists.txt:47-66`) and requires Python + `mkdocs` + `pymdown-extensions` +
`plantuml_markdown` (plus Java only if PlantUML diagrams are present); if any are missing the target
is silently skipped with a warning.

## Keeping this page in sync

Before publishing any doc change, cross-check every version stated here against **all three**
workflow files — `.github/workflows/build_bdk.yaml`,
`.github/workflows/prebuild_dependancies.yaml`, and `.github/workflows/test_whatever.yaml` (the
last is a diagnostic-only `workflow_dispatch` job that builds nothing, but it still pins Go `1.24.9`
and the OS matrix, so it is checked, not skipped) — plus `module/gobdk/go.mod` and `CMakeLists.txt`.
Any mismatch is a documentation bug.

## Windows (experimental / unsupported)

> **Windows is not a supported build target.** It is **dropped from CI** (there is no Windows entry
> in the build matrix, `build_bdk.yaml:29-31`), the Windows-only workflow steps are guarded by
> `runner.os == 'Windows'` and therefore never execute, and **no Windows GoBDK static library is
> shipped**.

A CMake build of the C++ core is nevertheless still possible for users who build it themselves, for
example:

```console
cmake -G"Visual Studio 17 2022" -A x64 -B build -S "." -DBUILD_MODULE_GOLANG=OFF ^
      -DBOOST_ROOT="path\to\boost_1.85.0" -DOPENSSL_ROOT_DIR="path\to\openssl"
cmake --build build --config Release
ctest --output-on-failure --test-dir build -C Release
```

This path is untested by CI and is provided as-is. The Golang binding on Windows is not supported.
