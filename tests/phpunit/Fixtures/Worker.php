<?php

declare(strict_types=1);

namespace Amashukov\PhpParallax\Tests\Fixtures;

use LogicException;
use RuntimeException;

/**
 * Worker payloads invoked by parallax inside worker threads. The class must
 * be visible to the worker via the bootstrap file at tests/phpunit/bootstrap.php.
 */
final class Worker
{
    public static function identity(int $n): int
    {
        return $n;
    }

    public static function add(int $a, int $b): int
    {
        return $a + $b;
    }

    public static function square(int $n): int
    {
        return $n * $n;
    }

    public static function reverse(string $s): string
    {
        return strrev($s);
    }

    public static function sleepThenReturn(int $microseconds, int $value): int
    {
        usleep($microseconds);

        return $value;
    }

    public static function panic(string $message = 'kapow'): never
    {
        throw new RuntimeException($message);
    }

    public static function panicLogic(string $message): never
    {
        throw new LogicException($message);
    }

    /**
     * @return list<array{i: int, tag: string}>
     */
    public static function bigPayload(int $n): array
    {
        $out = [];
        for ($i = 0; $i < $n; $i++) {
            $out[] = ['i' => $i, 'tag' => "row-{$i}"];
        }

        return $out;
    }

    public static function cpuHeavy(int $from, int $to): int
    {
        $acc = 0;
        for ($i = $from; $i < $to; $i++) {
            $acc += (int) sqrt($i) % 7;
        }

        return $acc;
    }
}
