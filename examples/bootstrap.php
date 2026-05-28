<?php

declare(strict_types=1);
/* Worker bootstrap loaded by examples/ scripts via the WaitGroup constructor.
 * Define classes, free functions and load your Composer autoloader here so
 * worker threads can resolve user-defined callables. */

final class Report
{
    /**
     * @return array{user_id: int, orders: int, revenue: float}
     */
    public static function userStats(int $userId): array
    {
        return [
            'user_id' => $userId,
            'orders'  => $userId * 3,
            'revenue' => $userId * 12.50,
        ];
    }
}

final class Shard
{
    /**
     * @param list<string> $cred
     */
    public static function countEvents(string $dsn, array $cred, int $shard): int
    {
        return strlen($dsn) + strlen($cred[0]) + $shard;
    }
}

function heavy(int $from, int $to): int
{
    $acc = 0;
    for ($i = $from; $i < $to; $i++) {
        $acc += (int) sqrt($i) % 7;
    }

    return $acc;
}

function boom(): void
{
    throw new RuntimeException("kapow");
}
