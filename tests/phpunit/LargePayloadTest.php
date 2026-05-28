<?php

declare(strict_types=1);

namespace Amashukov\PhpParallax\Tests;

use Amashukov\PhpParallax\Tests\Fixtures\Worker;
use Amashukov\PhpParallax\Tests\Helpers\PathHelper;
use PHPUnit\Framework\Attributes\Test;
use PHPUnit\Framework\TestCase;

final class LargePayloadTest extends TestCase
{
    #[Test]
    public function returnsLargeArrayPayload(): void
    {
        $wg = new \WaitGroup(PathHelper::bootstrap());
        $wg->go(Worker::bigPayload(...), [2000]);
        $r = $wg->wait();

        self::assertTrue($r[0]->ok);
        self::assertCount(2000, $r[0]->value);
        self::assertSame(['i' => 0, 'tag' => 'row-0'], $r[0]->value[0]);
        self::assertSame(['i' => 1999, 'tag' => 'row-1999'], $r[0]->value[1999]);
    }

    #[Test]
    public function largeStringArgumentRoundtrips(): void
    {
        $big = str_repeat('abcde', 50_000); // 250 kB string

        $wg = new \WaitGroup();
        $wg->go('strlen', [$big]);
        $wg->go('md5', [$big]);
        $r = $wg->wait();

        self::assertSame(strlen($big), $r[0]->value);
        self::assertSame(md5($big), $r[1]->value);
    }

    #[Test]
    public function deeplyNestedArrayCapture(): void
    {
        $deep = ['leaf' => 'x'];
        for ($i = 0; $i < 32; $i++) {
            $deep = ['next' => $deep];
        }

        $wg = new \WaitGroup();
        $wg->go(static function (array $tree): string {
            $cur = $tree;
            while (isset($cur['next'])) {
                $cur = $cur['next'];
            }

            return $cur['leaf'];
        }, [$deep]);

        $r = $wg->wait();
        self::assertSame('x', $r[0]->value);
    }
}
