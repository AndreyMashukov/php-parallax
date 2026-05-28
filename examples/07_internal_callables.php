<?php
/* Built-in PHP functions work without a bootstrap file — they are always
 * present in the worker's function table. */

$wg = new WaitGroup();
$wg->go(strlen(...),     ["hello"]);
$wg->go(strtoupper(...), ["world"]);
$wg->go(array_sum(...),  [[1, 2, 3, 4, 5]]);
$wg->go(json_encode(...), [["a" => 1, "b" => 2]]);

foreach ($wg->wait() as $i => $slot) {
    echo "slot {$i}: ", var_export($slot->value, true), "\n";
}
