--TEST--
WaitGroup::go() with static method callable
--EXTENSIONS--
parallax
--FILE--
<?php
require __DIR__ . "/_bootstrap.inc";

$wg = new WaitGroup(__DIR__ . "/_bootstrap.inc");
$wg->go("Math::square", [4]);
$wg->go(["Math", "square"], [5]);
$wg->go(Math::square(...), [6]);

$results = $wg->wait();
foreach ($results as $i => $slot) {
    printf("%d ok=%s value=%d\n", $i, $slot->ok ? "1" : "0", $slot->value);
}
?>
--EXPECT--
0 ok=1 value=16
1 ok=1 value=25
2 ok=1 value=36
