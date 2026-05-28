# Changelog

All notable changes to php-parallax are documented in this file.
The project follows [Semantic Versioning](https://semver.org/).

## [0.2.1] — 2026-05-28

Bug-fix and release-infrastructure pass.

### Fixed

- `bridge.c::object_to_value` now walks `obj->properties` in addition to `ce->properties_info`, so `stdClass` instances and any object with dynamic properties cross the thread boundary with their fields intact. Before this, a `stdClass` DTO would arrive in the worker stripped to an empty object.
- `release.yml` lowercases the GHCR namespace before tagging — `github.repository_owner` returns the verbatim `AndreyMashukov` mixed-case which the Docker registry rejects. The `docker-images` job also now declares `verify-version` in its `needs:` so the resolved version is actually available in the tag template.
- PECL package name renamed `php-parallax` → `parallax`. The PECL channel validator rejects `-` in package names and requires `<name>` to match `<providesextension>`. Same convention as `redis` (PECL) → `php-redis` (Debian).

### Added

- PHPUnit harness under `tests/phpunit/` with 33 tests and 2 087 assertions covering: WaitGroup lifecycle, every callable form (string / array / FCC / inline closure ± captures / static / with parameters), every capture rejection, panicking-worker isolation across multiple slots and inside inline closures, snapshot semantics for scalars / arrays / `stdClass` / nested structures, real OS-thread parallelism asserted by timing, 1 000-spawn fan-out, 2 000-row array returns, 250 kB string round-trip, 32-level nested array capture.
- `composer.json` registers the `Amashukov\PhpParallax\Tests` PSR-4 namespace and pulls `phpunit/phpunit ^11`, `friendsofphp/php-cs-fixer ^3.50`, `phpstan/phpstan ^2.0`, `phpstan/phpstan-phpunit ^2.0`.
- `phpstan-stubs/Parallax.php` declares the five C-defined classes for static analysis via `scanFiles`. Level 8 is now clean over `tests/phpunit/` and `examples/`.
- `.php-cs-fixer.dist.php` matches the `@PER-CS` + `@PHP84Migration` rule set used in `amashukov/rector-php-rules`, with import-symbols and import-classes enabled.
- `.github/workflows/cs.yml` and `.github/workflows/stan.yml` run the lint and static-analysis suites on every push/PR.
- The CI workflow now runs the PHPUnit suite on `php:8.4-zts-alpine` and `php:8.4-zts-bookworm` after the C ASAN gate.
- `dist/alpine/APKBUILD` ships a turnkey `php84-parallax` recipe providing the unversioned `php-parallax` alias.
- `.github/workflows/alpine-repo.yml` builds and signs the `.apk` on tag push and publishes the project's own apk repository to GitHub Pages, so users can `apk add php-parallax` from `https://andreymashukov.github.io/php-parallax/alpine/v3.20/community` without waiting on the aports community queue.

## [0.2.0] — 2026-05-28

Closure capture lands.

### Added

- Inline `function (...) use (...) { ... }` as a valid `WaitGroup::go()` callable.
- `use ($var)` captures are deep-cloned by value at spawn time — mutating the original variable in the parent after `go()` has no effect on the worker (Rust `move` semantics, verified by `101_use_by_value_snapshot.phpt`).
- Closure source is extracted from its origin file via a state-machine scanner that correctly skips single/double-quoted strings, heredoc/nowdoc bodies, and line/block comments before brace-balancing the body.
- Per-task wrapper source is rebuilt from scratch every time and carries the captures inline, so two structurally identical closures with different captures cannot mismap — regression test `105_closure_cache_isolation.phpt` is the explicit guard for the failure mode of [krakjoe/parallel#309](https://github.com/krakjoe/parallel/issues/309).

### Rejected at `go()` with `CaptureError`

- `use (&$x)` by-reference captures.
- Nested `Closure` instances inside captures.
- Eval-defined / REPL / stream-wrapper-sourced closures (no resolvable file).
- Closures bound to `$this` — declare `static` to detach.

### Still planned (later)

- Arrow functions `fn (...) => expr` — the extractor currently looks for the `function` keyword and a `{` body; arrow-function bodies extend only to the next expression-terminator and need a separate scanner path.
- `Pool` of warm worker threads for fine-grained tasks.
- Shared-arena closure cache keyed by SHA-256 of source plus capture signature (the wrapper today is rebuilt per task, which is correct but a touch wasteful for hot fan-out loops).

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
