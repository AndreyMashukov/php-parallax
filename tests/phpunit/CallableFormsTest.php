<?php

declare(strict_types=1);

namespace Amashukov\PhpParallax\Tests;

use Amashukov\PhpParallax\Tests\Fixtures\Worker;
use Amashukov\PhpParallax\Tests\Helpers\PathHelper;
use PHPUnit\Framework\Attributes\Test;
use PHPUnit\Framework\TestCase;
use WaitGroup;

final class CallableFormsTest extends TestCase
{
    #[Test]
    public function internalFunctionByName(): void
    {
        $wg = new WaitGroup();
        $wg->go('strlen', ['hello']);
        $wg->go('strtoupper', ['abc']);
        $r = $wg->wait();

        self::assertTrue($r[0]->ok);
        self::assertSame(5, $r[0]->value);
        self::assertTrue($r[1]->ok);
        self::assertSame('ABC', $r[1]->value);
    }

    #[Test]
    public function userFunctionRequiresBootstrap(): void
    {
        $wg = new WaitGroup(PathHelper::bootstrap());
        $wg->go(Worker::identity(...), [42]);
        $r = $wg->wait();

        self::assertTrue($r[0]->ok);
        self::assertSame(42, $r[0]->value);
    }

    #[Test]
    public function staticMethodAsString(): void
    {
        $wg = new WaitGroup(PathHelper::bootstrap());
        $wg->go(Worker::class . '::square', [7]);
        $r = $wg->wait();

        self::assertSame(49, $r[0]->value);
    }

    #[Test]
    public function staticMethodAsArray(): void
    {
        $wg = new WaitGroup(PathHelper::bootstrap());
        $wg->go([Worker::class, 'square'], [8]);
        $r = $wg->wait();

        self::assertSame(64, $r[0]->value);
    }

    #[Test]
    public function staticMethodAsFirstClassCallable(): void
    {
        $wg = new WaitGroup(PathHelper::bootstrap());
        $wg->go(Worker::square(...), [9]);
        $r = $wg->wait();

        self::assertSame(81, $r[0]->value);
    }

    #[Test]
    public function inlineClosureWithUseCapture(): void
    {
        $a = 10;
        $b = 20;

        $wg = new WaitGroup();
        $wg->go(static function () use ($a, $b) {
            return $a + $b;
        });
        $r = $wg->wait();

        self::assertSame(30, $r[0]->value);
    }

    #[Test]
    public function inlineClosureWithoutCaptures(): void
    {
        $wg = new WaitGroup();
        $wg->go(static function () {
            return 'parameterless';
        });
        $r = $wg->wait();

        self::assertSame('parameterless', $r[0]->value);
    }

    #[Test]
    public function inlineClosureWithParameters(): void
    {
        $factor = 3;
        $wg = new WaitGroup();
        $wg->go(static function (int $x, int $y) use ($factor) {
            return ($x + $y) * $factor;
        }, [4, 5]);
        $r = $wg->wait();

        self::assertSame(27, $r[0]->value);
    }

    #[Test]
    public function staticInlineClosure(): void
    {
        $offset = 100;
        $wg = new WaitGroup();
        $wg->go(static function (int $n) use ($offset) {
            return $n + $offset;
        }, [7]);
        $r = $wg->wait();

        self::assertSame(107, $r[0]->value);
    }
}
