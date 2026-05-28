<?php

declare(strict_types=1);

namespace Amashukov\PhpParallax\Tests;

use Amashukov\PhpParallax\Tests\Fixtures\Worker;
use Amashukov\PhpParallax\Tests\Helpers\PathHelper;
use PHPUnit\Framework\Attributes\Test;
use PHPUnit\Framework\TestCase;

final class WaitGroupTest extends TestCase
{
    #[Test]
    public function constructorWithoutBootstrap(): void
    {
        $wg = new \WaitGroup();
        self::assertSame(0, $wg->count());
    }

    #[Test]
    public function constructorWithBootstrap(): void
    {
        $wg = new \WaitGroup(PathHelper::bootstrap());
        self::assertSame(0, $wg->count());
    }

    #[Test]
    public function countTracksSpawnedTasks(): void
    {
        $wg = new \WaitGroup(PathHelper::bootstrap());

        self::assertSame(0, $wg->count());

        for ($i = 1; $i <= 5; $i++) {
            $wg->go(Worker::identity(...), [$i]);
            self::assertSame($i, $wg->count());
        }

        $wg->wait();
        self::assertSame(5, $wg->count(), 'count() must remain stable after wait()');
    }

    #[Test]
    public function waitReturnsArrayIndexedBySpawnOrder(): void
    {
        $wg = new \WaitGroup(PathHelper::bootstrap());
        $wg->go(Worker::identity(...), [10]);
        $wg->go(Worker::identity(...), [20]);
        $wg->go(Worker::identity(...), [30]);

        $results = $wg->wait();

        self::assertIsArray($results);
        self::assertCount(3, $results);
        self::assertSame(10, $results[0]->value);
        self::assertSame(20, $results[1]->value);
        self::assertSame(30, $results[2]->value);
    }

    #[Test]
    public function emptyWaitGroupYieldsEmptyArray(): void
    {
        $wg = new \WaitGroup(PathHelper::bootstrap());
        self::assertSame([], $wg->wait());
    }
}
