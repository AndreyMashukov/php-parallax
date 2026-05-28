--TEST--
A nested Closure inside a use() capture is rejected
--EXTENSIONS--
parallax
--FILE--
<?php
$callback = function ($n) { return $n * 2; };

$wg = new WaitGroup();
try {
    $wg->go(function () use ($callback) {
        return $callback(7);
    });
    echo "FAIL: should have thrown\n";
} catch (CaptureError $e) {
    echo "OK: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
OK: nested closure cannot be captured into a parallax task
