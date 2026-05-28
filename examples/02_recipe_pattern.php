<?php
/* "Recipe, not handle": pass DSN + credentials as data; the worker
 * instantiates its own PDO inside the callable. Live connections cannot
 * cross the thread boundary — that is the source of most multi-threaded
 * PHP bugs in other extensions. */

require __DIR__ . "/bootstrap.php";

$dsn  = "mysql:host=localhost;dbname=app";
$cred = ["u", "p"];

$wg = new WaitGroup(__DIR__ . "/bootstrap.php");
for ($shard = 0; $shard < 4; $shard++) {
    $wg->go(Shard::countEvents(...), [$dsn, $cred, $shard]);
}

$total = array_sum(array_map(
    fn ($slot) => $slot->ok ? $slot->value : 0,
    $wg->wait(),
));
echo "total = {$total}\n";
