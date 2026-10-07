# Bitcoin Development Kit Versioning

This page describes both the **versioning scheme as currently implemented** in the code and the
**semver policy** the project intends to follow.

## Current implementation

BDK carries several distinct version numbers:

- **Overall BDK version — `1.3.0`.** The root `CMakeLists.txt` sets
  `BDK_VERSION_MAJOR/MINOR/PATCH` to `1`, `3` and `0`, respectively.
  CPack uses these values for its package version.
- **Go-binding version — `1.3.0`.** A separate `BDK_GOLANG_VERSION_{MAJOR,MINOR,PATCH}`, exposed
  from Go via `module/gobdk/version.go` (`BDK_GOLANG_VERSION_STRING()` and friends). It is
  **derived** from the overall version in `module/gobdk/bdkcgo/CMakeLists.txt` with zero
  increments, so it equals the overall version. Releases tag it as `module/gobdk/v<version>`.
  Up to `module/gobdk/v1.2.4` the patch carried a `+2` offset, which is why that tag pairs with
  BDK `1.2.2`.
- **Rust C ABI version — `1.3.0`.** `module/rustbdk/capi/CMakeLists.txt` derives all three components from the overall BDK version with zero increments. This records the C ABI build version, not a promise that every Cargo package manifest uses the same number.
- **Captured bitcoin-sv (BSV) version / commit.** Version and Git metadata from the selected
  BSV checkout are captured when CMake configures the build: `core/BDKVersion.h` is the static declaration of the version symbols, and
  their concrete values are generated into `BDKVersion.cpp` from `core/BDKVersion.cpp.in`. They are
  re-exported in Go through `module/gobdk/version.go` — `BSV_VERSION_STRING()`,
  `BSV_GIT_COMMIT_HASH()`, `BSV_GIT_COMMIT_TAG_OR_BRANCH()`, `BSV_GIT_COMMIT_DATETIME()`, plus the
  BDK source's own `SOURCE_GIT_COMMIT_HASH()` / `SOURCE_GIT_COMMIT_DATETIME()` and
  `BDK_BUILD_DATETIME_UTC()`.

### Relationship to the bitcoin-sv version

BDK is built against a **pinned bitcoin-sv commit** — CI pins
`6504a3aff65ba97c0f6c80962b033e35ecbfed4b` (`build_bdk.yaml`, `DEFAULT_BITCOIN_SV_COMMIT`; see
[Dependencies & pinned versions](build.md#dependencies-pinned-versions)). That commit does **not**
mechanically determine the BDK version number. CMake writes the selected checkout's metadata
into generated `BDKVersion.cpp`. The Git hash is abbreviated and gains a `_dirty` suffix
for tracked changes; it does not identify those changes or untracked files. Without Git
metadata, the Git-derived values may be empty. Record the source revision separately when
building from an exported source tree. When the pinned BSV commit changes, update the table
in [build.md](build.md) accordingly.

## Versioning policy (semver)

The following [semantic-versioning](https://semver.org/) rules describe the project's policy. Only the maximum rule is enforced by the build: configuring fails if the Go or Rust version derived from the overall version would exceed it. The bump rules are applied by hand.

BDK consists of a "core" (code taken from SV plus common code shared by bindings) and the language
bindings. Each binding (module) and the core may be assigned a semver number, and the SDK as a whole
carries an overall version.

"Bumping" a version means incrementing the patch, minor, or major number and resetting the lesser
numbers to 0. The rules:

- If a language binding's version is bumped, the SDK's overall version is bumped in the same manner.
- If core's version is bumped, the language bindings are also bumped (to avoid detailed dependency
  analysis, a binding may be bumped even when a core change does not directly affect it).
- Only a single "most significant" bump is ever applied to the SDK.
- The SDK version is greater than or equal to every component version. CMake checks this at configure time.

Internal builds against development branches may append `-develop` or `-RC<n>` to the version, e.g.
`1.5.6-RC2` is the 2nd release candidate for `1.5.6`.

## When should I upgrade?

Review the release notes and commit history for changes affecting your APIs and linked BSV revision. The bump rules above are applied by hand, so unchanged version numbers alone are not sufficient evidence that an upgrade has no relevant changes.
