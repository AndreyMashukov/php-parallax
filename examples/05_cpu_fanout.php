<?php
/* CPU fan-out: four cores chew through a 100-million-iteration loop in
 * parallel. Fibers / amphp cannot do this — they share one OS thread. */

require __DIR__ . "/bootstrap.php";

$wg = new WaitGroup(__DIR__ . "/bootstrap.php");
$chunk = 25_000_000;
for ($c = 0; $c < 4; $c++) {
    $wg->go(heavy(...), [$c * $chunk, ($c + 1) * $chunk]);
}

$sum = array_sum(array_map(
    fn ($slot) => $slot->ok ? $slot->value : 0,
    $wg->wait(),
));
echo "sum = {$sum}\n";
