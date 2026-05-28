--TEST--
WaitGroup::count() reports the number of spawned tasks
--EXTENSIONS--
parallax
--FILE--
<?php
$wg = new WaitGroup();
var_dump($wg->count());
for ($i = 0; $i < 5; $i++) {
    $wg->go("strlen", [str_repeat("x", $i + 1)]);
}
var_dump($wg->count());
$wg->wait();
var_dump($wg->count());
?>
--EXPECT--
int(0)
int(5)
int(5)
