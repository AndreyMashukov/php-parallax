# Changelog

All notable changes to php-parallax are documented in this file.
The project follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Planned for v0.1.0

- `WaitGroup`, `ParallaxResult`, `ParallaxWorkerError`, `CaptureError`, `SpawnError`.
- Named-function and static-method callables (string, `[Class, 'method']`, first-class callable `Cls::method(...)`).
- Optional bootstrap file loaded per-worker via the `WaitGroup` constructor.
- Capture-time rejection of resources, by-ref arguments, nested Closures, and objects with native handlers.
- Per-slot error semantics — panicking workers never affect their siblings.
- ASAN-clean C core; 10 000-spawn leak gate.
- Works on `php:8.4-zts-alpine` and `php:8.4-zts-bookworm`.

### Planned for v0.2.0

- Closure capture (`fn () use (...)`) via source-text extraction and per-worker `zend_compile_string` re-compilation. Cache key is SHA-256 of source plus capture signature — never opcode (regression-tested against the failure mode of `krakjoe/parallel` issue #309).
- Optional `Pool` of warm worker threads for fine-grained tasks.
