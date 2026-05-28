# php-parallax

A PHP extension that brings **Go `go func` / Rust `move`** semantics to PHP 8.4+: every spawned task runs on its own OS thread inside an isolated PHP runtime, captures are deep-copied at spawn time, and one failing task never affects its siblings.

```php
$wg = new WaitGroup(__DIR__ . '/bootstrap.php');
$wg->go(Report::userStats(...), [42]);
$wg->go(Report::userStats(...), [99]);
$wg->go(strlen(...), ['hello']);

foreach ($wg->wait() as $i => $slot) {
    echo "slot {$i}: ", $slot->ok ? json_encode($slot->value) : "ERR {$slot->error->message}", "\n";
}
```

## Status

**Pre-release (v0.1.0-dev)**. The implementation works on PHP 8.4 ZTS (musl + glibc); the surface is intentionally tiny in v0.1.0 — `WaitGroup`, `ParallaxResult`, `ParallaxWorkerError`, `CaptureError`, `SpawnError`. Closure capture (`fn () use (...)`) is the v0.2.0 milestone; v0.1.0 supports **named functions and static methods** only.

## Why does this exist?

PHP has no first-class story for **CPU-parallel** in-process concurrency that keeps the Rust/Go discipline of "isolated runtime + snapshot capture, share nothing":

- **Fibers / amphp / ReactPHP** are cooperative on a single OS thread. They give you async I/O, not real parallelism.
- **Swoole** has true concurrency but its runtime hooks silently corrupt PDO, autoload, and Xdebug state under load.
- **pcntl_fork / Symfony Process / Spatie async** give you isolation but pay ~50ms per task to fork+IPC.
- **krakjoe/parallel** gets the model right but has unfixed correctness bugs and zero distro packaging.

php-parallax fills the gap: real OS-thread parallelism, isolated PHP request lifecycle per worker, snapshot-copy semantics enforced at the language boundary, and a one-command install on any official `php:*-zts-*` Docker image.

## When to use php-parallax

Use it when you need:

- **Sub-millisecond task latency** for fan-out work (process fork is ~50× slower).
- **True CPU parallelism** across cores (Fibers / amphp can't).
- **Rust-style isolation** — no shared globals, no aliasing surprises.
- **PHP ≥ 8.4 ZTS** is your floor.

Use something else when:

- You're inside a single HTTP request, doing I/O fan-out → **amphp + Fibers**, lighter and ships with zero install.
- Tasks are ≥100 ms each and infrequent → **Symfony\Process** or **pcntl_fork**.
- You're already on Swoole and have accepted its hook surface → stay there.

## Requirements

- PHP **8.4 or newer**, compiled with **ZTS** (`--enable-zts`). The build hard-errors on non-ZTS PHP. Official Docker images that work: `php:8.4-zts-alpine`, `php:8.4-zts-bookworm`, and the 8.5 / 8.6 ZTS variants when those land.
- POSIX threads (`-lpthread`). All Linux + macOS distros are fine.

## Install

### One-line installer (Linux Docker images)

```sh
curl -sSL https://github.com/AndreyMashukov/php-parallax/raw/main/install-php-parallax.sh | sh
```

Detects ZTS, falls back to building from source, and supports apt / apk / yum.

### Source build

```sh
git clone https://github.com/AndreyMashukov/php-parallax.git
cd php-parallax
phpize && ./configure --enable-parallax && make -j$(nproc)
sudo make install
echo extension=parallax.so | sudo tee /usr/local/etc/php/conf.d/parallax.ini
php -m | grep parallax
```

### Why does `php -m` say `parallax` but the package is `php-parallax`?

The PHP extension symbol name has to be a valid C identifier, so the `.so` artefact stays `parallax.so` and `php -m` lists `parallax`. The PECL package, the Debian/Ubuntu package, the Alpine package, the install script, and the canonical name everywhere user-facing is `php-parallax` — same convention as `php-redis` on Debian shipping `redis.so`.

## API

```php
final class WaitGroup {
    public function __construct(?string $bootstrap = null);
    public function go(callable $task, array $args = []): void;     // does not block
    public function wait(): array;                                   // blocks; ParallaxResult[]
    public function count(): int;                                    // number of spawned tasks
}

final class ParallaxResult {
    public bool                $ok;
    public mixed               $value;        // when ok
    public ?ParallaxWorkerError $error;       // when !ok
}

final class ParallaxWorkerError {
    public string $class;
    public string $message;
    public int    $code;
    public string $trace;        // string, not a live Throwable
}

class CaptureError extends \Error {}   // by-ref, resource, nested closure, ...
class SpawnError   extends \Error {}   // unresolvable callable
```

Error semantics: **per-slot, never throwing across the boundary**. One panicking worker yields `ok = false` in its slot; sibling slots are unaffected. Same shape as Go's `errgroup`, except partial results are never lost.

## The bootstrap file

Each worker thread runs in its own request lifecycle with an empty function/class table. To make your user-defined classes and functions visible to the worker, pass a bootstrap file to the `WaitGroup` constructor:

```php
$wg = new WaitGroup(__DIR__ . '/bootstrap.php');
$wg->go(MyClass::doWork(...), [$args]);
```

The bootstrap should:

- Register your autoloader (`require 'vendor/autoload.php';`).
- Warm any process-wide caches the worker will reuse.
- Define free functions you intend to pass as callables.

Bootstrap is loaded once per worker request via `zend_execute_scripts`.

## Capture rules (the "move" semantics)

When you call `go()`, every argument is deep-cloned into a thread-portable snapshot. The following throw `CaptureError` at the call site, never reaching the worker:

- **Resources** (`tmpfile()`, sockets, gd handles, ...).
- **By-reference arguments** (`$args[0] = &$ref`).
- **Nested `Closure` instances inside captures**.
- **Objects bound to non-portable native handlers** — PDO, GMP, Reflection, Generator, ... For these, pass the recipe (DSN, credentials, configuration data) and reconstruct inside the worker.

Plain `stdClass`, public-property DTOs, and value-style classes are fine.

## Examples

See [examples/](examples/) for the full set. A flavour:

### Fan-out with isolated DB connections (recipe pattern)

```php
$dsn  = 'mysql:host=localhost;dbname=app';
$cred = ['u', 'p'];

$wg = new WaitGroup(__DIR__ . '/bootstrap.php');
for ($shard = 0; $shard < 4; $shard++) {
    $wg->go(Shard::countEvents(...), [$dsn, $cred, $shard]);
}
$total = array_sum(array_map(
    fn ($slot) => $slot->ok ? $slot->value : 0,
    $wg->wait()
));
```

### Error isolation

```php
$wg = new WaitGroup(__DIR__ . '/bootstrap.php');
$wg->go(Report::userStats(...), [1]);   // OK
$wg->go(boom(...));                     // throws
$wg->go(strlen(...), ['xyz']);          // OK

foreach ($wg->wait() as $i => $slot) {
    if ($slot->ok) {
        echo "slot {$i}: ", json_encode($slot->value), "\n";
    } else {
        echo "slot {$i} [{$slot->error->class}]: {$slot->error->message}\n";
    }
}
```

## INI settings

| Name | Default | Notes |
|---|---|---|
| `parallax.max_workers` | `0` (unbounded) | Reserved for v0.2.0 task-pool. |
| `parallax.worker_stack_kb` | `8192` | Worker pthread stack size. The default 8 MB matches glibc; **musl's 80 kB default would SIGSEGV inside the PHP interpreter**, so the worker setup always forces 8 MB regardless of this value in v0.1.0. |

## Comparison with other PHP concurrency options

| | parallax | ext-parallel | Swoole | Fibers/amphp | pcntl_fork |
|---|---|---|---|---|---|
| OS-level parallelism | yes | yes | no (1 thread/process) | no | yes |
| Latency per task | µs | µs | µs | ns | ms |
| State isolation | full | full | shared | shared | full |
| ZTS required | yes | yes | no | no | no |
| Maintained 2026 | yes | yes (niche) | yes | yes (core) | n/a |
| Distro install | (planned) | source-only | yes | yes (core) | yes (core) |

See [docs/comparison.md](docs/comparison.md) for the long version with citations.

## Building from source (development)

```sh
make -f Makefile.dev value-test            # C-only unit tests under ASAN+UBSAN
make -f Makefile.dev core-test             # pthread WaitGroup tests under ASAN
make -f Makefile.dev stress-test           # 1k concurrent spawns
make -f Makefile.dev leak-test             # 10k-spawn RSS-drift gate
make -f Makefile.dev docker-test-alpine    # full pipeline on php:8.4-zts-alpine
make -f Makefile.dev docker-test-bookworm  # full pipeline on php:8.4-zts-bookworm
```

## License

MIT. See [LICENSE](LICENSE).

## Author

[Andrei Mashukov](https://github.com/AndreyMashukov) — `a.mashukoff@gmail.com`
