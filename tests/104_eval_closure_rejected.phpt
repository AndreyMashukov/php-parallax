--TEST--
Closure defined in eval()-d source is rejected (no resolvable file)
--EXTENSIONS--
parallax
--FILE--
<?php
$fn = eval('return function () { return 1; };');
$wg = new WaitGroup();
try {
    $wg->go($fn);
    echo "FAIL: should have thrown\n";
} catch (CaptureError $e) {
    echo "OK\n";
}
?>
--EXPECT--
OK
