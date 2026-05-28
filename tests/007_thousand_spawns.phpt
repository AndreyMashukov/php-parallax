--TEST--
1000 sequential spawns complete cleanly with no slot loss
--EXTENSIONS--
parallax
--FILE--
<?php
$wg = new WaitGroup();
for ($i = 0; $i < 1000; $i++) {
    $wg->go("intval", [(string)$i]);
}
$results = $wg->wait();
echo "count=", $wg->count(), "\n";

$failures = 0;
foreach ($results as $i => $slot) {
    if (!$slot->ok || $slot->value !== $i) {
        $failures++;
    }
}
echo "failures=", $failures, "\n";
?>
--EXPECT--
count=1000
failures=0
