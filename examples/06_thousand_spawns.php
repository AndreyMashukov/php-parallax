<?php
/* Quick sanity sweep: a thousand sequential spawns, all results collected,
 * zero slot loss. */

$wg = new WaitGroup();
for ($i = 0; $i < 1000; $i++) {
    $wg->go("strlen", [str_repeat("x", ($i % 32) + 1)]);
}

$results = $wg->wait();
$failed = 0;
foreach ($results as $slot) {
    if (!$slot->ok) {
        $failed++;
    }
}

echo "spawned = ", count($results), "  failed = {$failed}\n";
