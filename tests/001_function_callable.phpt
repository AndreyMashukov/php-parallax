--TEST--
WaitGroup::go() with built-in function callable
--EXTENSIONS--
parallax
--FILE--
<?php
$wg = new WaitGroup();
$wg->go("strlen", ["hello"]);
$wg->go("strlen", ["WaitGroup!"]);
$wg->go("strtoupper", ["abc"]);

$results = $wg->wait();
foreach ($results as $i => $slot) {
    printf("%d ok=%s value=%s\n", $i, $slot->ok ? "1" : "0", var_export($slot->value, true));
}
?>
--EXPECT--
0 ok=1 value=5
1 ok=1 value=10
2 ok=1 value='ABC'
