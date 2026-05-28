--TEST--
WaitGroup::go() rejects by-reference array arguments
--EXTENSIONS--
parallax
--FILE--
<?php
$wg = new WaitGroup();

$x = 1;
$ref = &$x;
$args = [];
$args[0] = &$ref;

try {
    $wg->go("strval", $args);
    echo "FAIL: should have thrown\n";
} catch (CaptureError $e) {
    echo "OK: caught\n";
}
?>
--EXPECT--
OK: caught
