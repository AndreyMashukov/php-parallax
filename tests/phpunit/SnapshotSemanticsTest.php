<?php

declare(strict_types=1);

namespace Amashukov\PhpParallax\Tests;

use PHPUnit\Framework\Attributes\Test;
use PHPUnit\Framework\TestCase;
use WaitGroup;
use stdClass;

final class SnapshotSemanticsTest extends TestCase
{
    #[Test]
    public function captureSnapshotsValueAtSpawnTime(): void
    {
        $userId = 42;
        $wg = new WaitGroup();
        $wg->go(static function () use ($userId) {
            return $userId;
        });

        // Mutate AFTER go() — worker must keep 42.
        $userId = 99;

        $r = $wg->wait();
        self::assertSame(42, $r[0]->value);
    }

    #[Test]
    public function arrayArgumentDeepCloned(): void
    {
        $payload = ['count' => 1, 'items' => ['a', 'b']];
        $wg = new WaitGroup();
        $wg->go(static function (array $p) {
            return ['received' => $p, 'sum' => count($p['items'])];
        }, [$payload]);

        // Mutate after spawn.
        $payload['count'] = 999;
        $payload['items'][] = 'c';

        $r = $wg->wait();
        self::assertSame(1, $r[0]->value['received']['count']);
        self::assertSame(2, $r[0]->value['sum']);
    }

    #[Test]
    public function objectCaptureMaterialisesStdClassByValue(): void
    {
        $dto = new stdClass();
        $dto->id = 7;
        $dto->name = 'Andrei';

        $wg = new WaitGroup();
        $wg->go(static function (stdClass $o) {
            return "{$o->name} #{$o->id}";
        }, [$dto]);

        $dto->name = 'mutated';

        $r = $wg->wait();
        self::assertSame('Andrei #7', $r[0]->value);
    }

    #[Test]
    public function workerCannotMutateParentScope(): void
    {
        $counter = 0;
        $wg = new WaitGroup();
        for ($i = 0; $i < 5; $i++) {
            $wg->go(static function () use ($counter) {
                return $counter + 1;
            });
        }
        $r = $wg->wait();

        foreach ($r as $slot) {
            self::assertSame(1, $slot->value);
        }
        /* $counter is local to this method; workers operate on snapshots and
         * cannot reach into the parent scope (no side-effect to verify). */
    }
}
