<?php

declare(strict_types=1);

namespace Amashukov\PhpParallax\Tests;

use PHPUnit\Framework\Attributes\Test;
use PHPUnit\Framework\TestCase;

final class CaptureRejectionTest extends TestCase
{
    #[Test]
    public function resourceArgumentRejected(): void
    {
        $wg = new \WaitGroup();
        $tmp = tmpfile();

        try {
            $wg->go('fwrite', [$tmp, 'x']);
            self::fail('expected CaptureError for resource argument');
        } catch (\CaptureError $e) {
            self::assertStringContainsString('resource', $e->getMessage());
        } finally {
            fclose($tmp);
        }
    }

    #[Test]
    public function byReferenceArgumentRejected(): void
    {
        $wg = new \WaitGroup();
        $x = 1;
        $args = [];
        $args[0] = &$x;

        $this->expectException(\CaptureError::class);
        $wg->go('strval', $args);
    }

    #[Test]
    public function byReferenceCaptureRejected(): void
    {
        $wg = new \WaitGroup();
        $counter = 0;

        $this->expectException(\CaptureError::class);
        $wg->go(static function () use (&$counter) {
            $counter++;
        });
    }

    #[Test]
    public function nestedClosureCaptureRejected(): void
    {
        $wg = new \WaitGroup();
        $callback = static fn (int $n): int => $n * 2;

        $this->expectException(\CaptureError::class);
        $wg->go(static function () use ($callback) {
            return $callback(5);
        });
    }

    #[Test]
    public function evalDefinedClosureRejected(): void
    {
        $wg = new \WaitGroup();
        $fn = eval('return function () { return 1; };');

        $this->expectException(\CaptureError::class);
        $wg->go($fn);
    }

    #[Test]
    public function pdoLikeObjectRejected(): void
    {
        $wg = new \WaitGroup();
        $pdo = new \PDO('sqlite::memory:');

        $this->expectException(\CaptureError::class);
        $wg->go('strlen', [$pdo]);
    }
}
