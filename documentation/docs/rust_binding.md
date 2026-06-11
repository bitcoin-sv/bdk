# Rust Binding

The experimental Rust binding lives under
[`module/rustbdk`](https://github.com/bitcoin-sv/bdk/tree/master/module/rustbdk).
Its package is named `rust-bdk` and Rust code imports it as `rust_bdk`. See the
[`module/rustbdk` README](https://github.com/bitcoin-sv/bdk/blob/master/module/rustbdk/README.md)
for source-build and Cargo usage details.

## Module independence rules

`module/rustbdk` is an independent BDK module. The binding follows these rules:

1. It depends on `bdk_core`, its public headers, and the shared third-party and
   bitcoin-sv link-closure inputs that `bdk_core` needs.
2. It must not include, link, copy, symlink, read at build time, or edit files
   from sibling modules such as `module/gobdk`, `module/typesbdk`, or
   `module/example`.
3. Its C ABI is defined inside `module/rustbdk` and uses the distinct
   `bdkffi_` symbol prefix.
4. Its static archive is `libbdkffi_<os_arch>.a`; it must never fall back to a
   GoBDK archive.
5. Consensus and policy logic remain in `bdk_core`; Rust wraps that logic rather
   than reimplementing it.
6. The crates stay layered: `bdk-sys` owns raw FFI, and `rust-bdk` exposes the
   safe public API.
7. Validation parity comes from calling the same `bdk_core`
   `ValidateTransaction(..., consensus)` implementation. `consensus=true` means
   block/consensus rules, while `consensus=false` means peer/mempool policy
   rules.
