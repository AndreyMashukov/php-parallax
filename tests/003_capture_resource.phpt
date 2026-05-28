--TEST--
WaitGroup::go() rejects resource arguments with CaptureError
--EXTENSIONS--
parallax
--FILE--
<?php
$wg = new WaitGroup();
$tmp = tmpfile();

try {
    $wg->go("fwrite", [$tmp, "x"]);
    echo "FAIL: should have thrown\n";
} catch (CaptureError $e) {
    echo "OK: ", $e->getMessage(), "\n";
}

fclose($tmp);
?>
--EXPECT--
OK: resource cannot be captured into a parallax task
