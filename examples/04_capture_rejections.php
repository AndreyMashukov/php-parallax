<?php

declare(strict_types=1);
/* CaptureError is raised at go() time when an argument would smuggle a
 * non-portable construct across the thread boundary. */

$wg = new WaitGroup();

$tmp = tmpfile();
try {
    $wg->go("fwrite", [$tmp, "x"]);
} catch (CaptureError $e) {
    echo "rejected: {$e->getMessage()}\n";
}
fclose($tmp);

$ref = 1;
$args = [];
$args[0] = &$ref;
try {
    $wg->go("strval", $args);
} catch (CaptureError $e) {
    echo "rejected: {$e->getMessage()}\n";
}
