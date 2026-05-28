--TEST--
use(&$x) by-reference capture is rejected with CaptureError
--EXTENSIONS--
parallax
--FILE--
<?php
$counter = 0;
$wg = new WaitGroup();

try {
    $wg->go(function () use (&$counter) {
        $counter++;
        return $counter;
    });
    echo "FAIL: should have thrown\n";
} catch (CaptureError $e) {
    echo "OK: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
OK: by-reference `use(&$var)` cannot be captured into a parallax task
