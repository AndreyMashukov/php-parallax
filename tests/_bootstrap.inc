<?php
class Math {
    public static function square(int $n): int { return $n * $n; }
}

class DBConnect {
    public static function count(string $dsn, string $user, int $shard): int {
        return strlen($dsn) + strlen($user) + $shard;
    }
}

function ok_one(): int { return 1; }
function ok_two(): int { return 2; }
function boom(): void { throw new RuntimeException("kapow"); }
