--TEST--
use() captures are a snapshot — mutating the original after go() does not change the worker's value
--EXTENSIONS--
parallax
--FILE--
<?php
$userId = 42;
$tag    = "orders";

$wg = new WaitGroup();
$wg->go(function () use ($userId, $tag) {
    return [$tag => $userId];
});

$userId = 99;     // worker should still see 42
$tag    = "junk"; // worker should still see "orders"

$r = $wg->wait();
echo json_encode($r[0]->value), "\n";
?>
--EXPECT--
{"orders":42}
