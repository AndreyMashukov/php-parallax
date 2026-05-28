<?php

declare(strict_types=1);
/* Static methods executed across three OS threads with isolated PHP runtimes.
 * Each worker opens its own resources inside its callable. */

require __DIR__ . "/bootstrap.php";

$wg = new WaitGroup(__DIR__ . "/bootstrap.php");
$wg->go(Report::userStats(...), [42]);
$wg->go(Report::userStats(...), [99]);
$wg->go(strlen(...), ["hello"]);

foreach ($wg->wait() as $i => $slot) {
    if ($slot->ok) {
        echo "slot {$i}: ", json_encode($slot->value), "\n";
    } elseif ($slot->error !== null) {
        echo "slot {$i} FAILED: {$slot->error->message}\n";
    }
}
