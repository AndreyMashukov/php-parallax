# Changelog

All notable changes to php-parallax are documented in this file.
The project follows [Semantic Versioning](https://semver.org/).

## [0.1.0] — 2026-05-28

Initial public release. Tagged `v0.1.0`.

### Added

- `WaitGroup`, `ParallaxResult`, `ParallaxWorkerError`, `CaptureError`, `SpawnError`.
- Named-function and static-method callables (string `"fname"`, array `["Class", "method"]`, and the first-class callable form `Cls::method(...)`).
- Optional bootstrap file loaded per-worker via the `WaitGroup` constructor — required to make user-defined functions and classes visible to the spawned thread.
- Capture-time rejection of resources, by-ref arguments, nested Closures, and objects bound to native handlers.
- Per-slot error semantics — panicking workers never affect their siblings.
- ASAN-clean C core; the 10 000-spawn leak gate keeps RSS drift under 5 MB.
- Verified end-to-end on `php:8.4-zts-alpine` (musl) and `php:8.4-zts-bookworm` (glibc); pthread stack forced to 8 MB to bypass musl's 80 kB default that would SIGSEGV the interpreter.

### Planned for v0.2.0

- Closure capture (`fn () use (...)`) via source-text extraction and per-worker `zend_compile_string` re-compilation. Cache key is SHA-256 of source plus capture signature — never opcode (regression-tested against the failure mode of `krakjoe/parallel` issue #309).
- Optional `Pool` of warm worker threads for fine-grained tasks.
