--TEST--
Recipe pattern: pass DSN/credentials as data, not as a live resource
--EXTENSIONS--
parallax
--FILE--
<?php
require __DIR__ . "/_bootstrap.inc";

$dsn  = "mysql:host=localhost;dbname=app";
$user = "u";

$wg = new WaitGroup(__DIR__ . "/_bootstrap.inc");
for ($shard = 0; $shard < 4; $shard++) {
    $wg->go(DBConnect::count(...), [$dsn, $user, $shard]);
}
$results = $wg->wait();
$total = 0;
foreach ($results as $slot) {
    if ($slot->ok) {
        $total += $slot->value;
    }
}
echo "total=", $total, "\n";
?>
--EXPECT--
total=134
