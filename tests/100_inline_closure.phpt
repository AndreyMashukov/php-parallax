--TEST--
Inline closure with use() captures runs in the worker
--EXTENSIONS--
parallax
--FILE--
<?php
$x = 10;
$y = 20;

$wg = new WaitGroup();
$wg->go(function () use ($x, $y) {
    return $x + $y;
});
$wg->go(function () use ($x, $y) {
    return $x * $y;
});

$r = $wg->wait();
printf("slot0 ok=%s value=%d\n", $r[0]->ok ? "y" : "n", $r[0]->value);
printf("slot1 ok=%s value=%d\n", $r[1]->ok ? "y" : "n", $r[1]->value);
?>
--EXPECT--
slot0 ok=y value=30
slot1 ok=y value=200
