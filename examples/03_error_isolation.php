<?php
/* One panicking worker yields ok=false in its slot. Sibling slots are
 * unaffected — there is no shared interpreter state to corrupt. */

require __DIR__ . "/bootstrap.php";

$wg = new WaitGroup(__DIR__ . "/bootstrap.php");
$wg->go(Report::userStats(...), [1]);
$wg->go(boom(...));
$wg->go(strlen(...), ["xyz"]);

foreach ($wg->wait() as $i => $slot) {
    if ($slot->ok) {
        echo "slot {$i} ok: ", json_encode($slot->value), "\n";
    } else {
        echo "slot {$i} [{$slot->error->class}]: {$slot->error->message}\n";
    }
}
