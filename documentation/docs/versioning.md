# Bitcoin Development Kit Versioning

This page describes both the **versioning scheme as currently implemented** in the code and the
**semver policy** the project follows.

## Current implementation

BDK carries several distinct version numbers:

- **Overall BDK version — `1.2.2`.** Hard-coded in the root `CMakeLists.txt:60-62` as
  `BDK_VERSION_MAJOR` (`1`), `BDK_VERSION_MINOR` (`2`), `BDK_VERSION_PATCH` (`2`). This is the
  package version used by CPack.
- **Go-binding version — `1.2.4`.** A separate `BDK_GOLANG_VERSION_{MAJOR,MINOR,PATCH}`, exposed
  from Go via `module/gobdk/version.go` (`BDK_GOLANG_VERSION_STRING()` and friends). It is
  **derived** from the overall version in `module/gobdk/bdkcgo/CMakeLists.txt:19-22`: major and
  minor are taken as-is (`1`, `2`) and the patch is the overall patch **plus 2**
  (`createIncrementVersion(BDK_GOLANG_VERSION_PATCH ${BDK_VERSION_PATCH} 2 …)`), i.e. `2 + 2 = 4` —
  giving `1.2.4` for the current overall `1.2.2`.
- **Captured bitcoin-sv (BSV) version / commit.** The exact BSV source BDK was built against is
  captured at build time: `core/BDKVersion.h` is the static declaration of the version symbols, and
  their concrete values are generated into `BDKVersion.cpp` from `core/BDKVersion.cpp.in`. They are
  re-exported in Go through `module/gobdk/version.go` — `BSV_VERSION_STRING()`,
  `BSV_GIT_COMMIT_HASH()`, `BSV_GIT_COMMIT_TAG_OR_BRANCH()`, `BSV_GIT_COMMIT_DATETIME()`, plus the
  BDK source's own `SOURCE_GIT_COMMIT_HASH()` / `SOURCE_GIT_COMMIT_DATETIME()` and
  `BDK_BUILD_DATETIME_UTC()`.

### Relationship to the bitcoin-sv version

BDK is built against a **pinned bitcoin-sv commit** — CI pins
`879fc8b42168dd0e608dafd51b39c6dabad37d4d` (`build_bdk.yaml:22`; see
[Dependencies & pinned versions](build.md#dependencies-pinned-versions)). That commit does **not**
mechanically determine the BDK version number, but it is **captured into the generated version
header** at build time, so any built artifact records exactly which BSV source produced it. When the
pinned BSV commit changes, update the table in [build.md](build.md) accordingly.

## Versioning policy (semver)

The project follows [semantic versioning](https://semver.org/) with the following intent. (This is
the *policy*; the *current* numbers are listed above.)

BDK consists of a "core" (code taken from SV plus common code shared by bindings) and the language
bindings. Each binding (module) and the core may be assigned a semver number, and the SDK as a whole
carries an overall version.

"Bumping" a version means incrementing the patch, minor, or major number and resetting the lesser
numbers to 0. The rules:

- If a language binding's version is bumped, the SDK's overall version is bumped in the same manner.
- If core's version is bumped, the language bindings are also bumped (to avoid detailed dependency
  analysis, a binding may be bumped even when a core change does not directly affect it).
- Only a single "most significant" bump is ever applied to the SDK.
- The SDK version is always greater than or equal to the versions of any of its components.

Internal builds against development branches may append `-develop` or `-RC<n>` to the version, e.g.
`1.5.6-RC2` is the 2nd release candidate for `1.5.6`.

## When should I upgrade?

If the versions of core and the language binding you use have not changed, you do not need to
upgrade. Consult the release notes / commit history to determine whether a change affects you.
