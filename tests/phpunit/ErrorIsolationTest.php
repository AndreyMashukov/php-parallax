<?php

declare(strict_types=1);

namespace Amashukov\PhpParallax\Tests;

use Amashukov\PhpParallax\Tests\Fixtures\Worker;
use Amashukov\PhpParallax\Tests\Helpers\PathHelper;
use PHPUnit\Framework\Attributes\Test;
use PHPUnit\Framework\TestCase;
use DomainException;
use LogicException;
use ParallaxWorkerError;
use RuntimeException;
use WaitGroup;

final class ErrorIsolationTest extends TestCase
{
    #[Test]
    public function panickingWorkerDoesNotAffectSiblings(): void
    {
        $wg = new WaitGroup(PathHelper::bootstrap());
        $wg->go(Worker::identity(...), [1]);
        $wg->go(Worker::panic(...));
        $wg->go(Worker::identity(...), [2]);

        $r = $wg->wait();

        self::assertTrue($r[0]->ok);
        self::assertSame(1, $r[0]->value);

        self::assertFalse($r[1]->ok);
        self::assertInstanceOf(ParallaxWorkerError::class, $r[1]->error);
        self::assertSame(RuntimeException::class, $r[1]->error->class);
        self::assertSame('kapow', $r[1]->error->message);

        self::assertTrue($r[2]->ok);
        self::assertSame(2, $r[2]->value);
    }

    #[Test]
    public function multiplePanicsAllCapturedIndividually(): void
    {
        $wg = new WaitGroup(PathHelper::bootstrap());
        $wg->go(Worker::panic(...), ['first']);
        $wg->go(Worker::panicLogic(...), ['second']);
        $wg->go(Worker::panic(...), ['third']);

        $r = $wg->wait();

        self::assertFalse($r[0]->ok);
        self::assertFalse($r[1]->ok);
        self::assertFalse($r[2]->ok);

        self::assertInstanceOf(ParallaxWorkerError::class, $r[0]->error);
        self::assertSame(RuntimeException::class, $r[0]->error->class);
        self::assertSame('first', $r[0]->error->message);

        self::assertInstanceOf(ParallaxWorkerError::class, $r[1]->error);
        self::assertSame(LogicException::class, $r[1]->error->class);
        self::assertSame('second', $r[1]->error->message);

        self::assertInstanceOf(ParallaxWorkerError::class, $r[2]->error);
        self::assertSame(RuntimeException::class, $r[2]->error->class);
        self::assertSame('third', $r[2]->error->message);
    }

    #[Test]
    public function panicInsideInlineClosureCaptured(): void
    {
        $wg = new WaitGroup();
        $code = 7;
        $wg->go(static function () use ($code) {
            throw new DomainException("inline panic", $code);
        });

        $r = $wg->wait();

        self::assertFalse($r[0]->ok);
        self::assertInstanceOf(ParallaxWorkerError::class, $r[0]->error);
        self::assertSame(DomainException::class, $r[0]->error->class);
        self::assertSame('inline panic', $r[0]->error->message);
        self::assertSame(7, $r[0]->error->code);
    }
}
