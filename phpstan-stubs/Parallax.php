<?php

declare(strict_types=1);

/**
 * Stub declarations for static analysis. The five classes here are registered
 * by the parallax C extension at PHP_MINIT and never appear in PHP-land source.
 * This file is referenced via phpstan.neon's `scanFiles` so PHPStan can resolve
 * the symbols without trying to autoload them at runtime.
 */

final class WaitGroup
{
    public function __construct(?string $bootstrap = null) {}

    /**
     * @param array<int|string, mixed> $args
     */
    public function go(callable $task, array $args = []): void {}

    /**
     * @return array<int, ParallaxResult>
     */
    public function wait(): array
    {
        return [];
    }

    public function count(): int
    {
        return 0;
    }
}

final class ParallaxResult
{
    public bool $ok = false;
    public mixed $value = null;
    public ?ParallaxWorkerError $error = null;
}

final class ParallaxWorkerError
{
    public string $class = '';
    public string $message = '';
    public int $code = 0;
    public string $trace = '';
}

class CaptureError extends \Error {}

class SpawnError extends \Error {}
