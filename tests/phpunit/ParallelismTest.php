<?php

declare(strict_types=1);

namespace Amashukov\PhpParallax\Tests;

use Amashukov\PhpParallax\Tests\Fixtures\Worker;
use Amashukov\PhpParallax\Tests\Helpers\PathHelper;
use PHPUnit\Framework\Attributes\Test;
use PHPUnit\Framework\TestCase;
use WaitGroup;

/**
 * These tests assert *real* OS-thread parallelism, not cooperative interleaving.
 * The expectation: N tasks that each sleep T microseconds complete in roughly
 * T (not N*T) wall-clock seconds. We allow generous slack to keep CI stable
 * on slow shared runners — the qualitative property is what matters.
 */
final class ParallelismTest extends TestCase
{
    #[Test]
    public function fourSleepersRunInParallel(): void
    {
        $sleepMicro = 250_000; // 250 ms per task
        $tasks = 4;

        $wg = new WaitGroup(PathHelper::bootstrap());
        $start = hrtime(true);
        for ($i = 0; $i < $tasks; $i++) {
            $wg->go(Worker::sleepThenReturn(...), [$sleepMicro, $i]);
        }
        $results = $wg->wait();
        $elapsedMs = (hrtime(true) - $start) / 1_000_000.0;

        self::assertCount($tasks, $results);
        foreach ($results as $slot) {
            self::assertTrue($slot->ok);
        }

        $sequentialMs = $tasks * ($sleepMicro / 1000);
        $parallelMs   = $sleepMicro / 1000;

        // We expect closer to parallelMs than sequentialMs.
        // Tolerance: result must be < halfway between the two bounds.
        $threshold = $parallelMs + ($sequentialMs - $parallelMs) / 2;
        self::assertLessThan(
            $threshold,
            $elapsedMs,
            sprintf(
                '%d sleepers of %d ms each completed in %.1f ms — that looks serial (sequential would be %.0f ms, parallel ≈ %.0f ms)',
                $tasks,
                $sleepMicro / 1000,
                $elapsedMs,
                $sequentialMs,
                $parallelMs,
            ),
        );
    }

    #[Test]
    public function cpuBoundWorkScalesAcrossCores(): void
    {
        // Skip when the runner does not expose multiple cores.
        if (PHP_INT_SIZE < 8 || (int) shell_exec('nproc 2>/dev/null') < 2) {
            self::markTestSkipped('multi-core box required');
        }

        $chunkSize = 1_000_000;
        $chunks = 4;

        // Baseline: serial run inside the main thread.
        $serialStart = hrtime(true);
        for ($i = 0; $i < $chunks; $i++) {
            Worker::cpuHeavy($i * $chunkSize, ($i + 1) * $chunkSize);
        }
        $serialMs = (hrtime(true) - $serialStart) / 1_000_000.0;

        // Parallel: same total work across worker threads.
        $wg = new WaitGroup(PathHelper::bootstrap());
        $parallelStart = hrtime(true);
        for ($i = 0; $i < $chunks; $i++) {
            $wg->go(Worker::cpuHeavy(...), [$i * $chunkSize, ($i + 1) * $chunkSize]);
        }
        $r = $wg->wait();
        $parallelMs = (hrtime(true) - $parallelStart) / 1_000_000.0;

        self::assertCount($chunks, $r);
        foreach ($r as $slot) {
            self::assertTrue($slot->ok);
        }

        // We do not assert a hard speed-up factor — CI runners have variable
        // core counts and noise. Assert only that parallel is not slower
        // than serial by more than 25% (i.e. there is at least some win
        // or we are not catastrophically backfiring).
        self::assertLessThan(
            $serialMs * 1.25,
            $parallelMs,
            sprintf(
                'parallel=%0.1fms vs serial=%0.1fms — parallel branch should not be slower than serial',
                $parallelMs,
                $serialMs,
            ),
        );
    }

    #[Test]
    public function fanOutThousandSpawns(): void
    {
        $wg = new WaitGroup();
        for ($i = 0; $i < 1000; $i++) {
            $wg->go('intval', [(string) $i]);
        }
        $r = $wg->wait();

        self::assertCount(1000, $r);
        foreach ($r as $i => $slot) {
            self::assertTrue($slot->ok);
            self::assertSame($i, $slot->value);
        }
    }
}
