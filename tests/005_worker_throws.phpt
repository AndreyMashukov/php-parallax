--TEST--
A panicking worker yields ok=false but does not affect its siblings
--EXTENSIONS--
parallax
--FILE--
<?php
require __DIR__ . "/_bootstrap.inc";

$wg = new WaitGroup(__DIR__ . "/_bootstrap.inc");
$wg->go("ok_one");
$wg->go("boom");
$wg->go("ok_two");

$results = $wg->wait();

printf("slot0 ok=%s value=%s\n", $results[0]->ok ? "yes" : "no", var_export($results[0]->value, true));
printf("slot1 ok=%s class=%s message=%s\n",
    $results[1]->ok ? "yes" : "no",
    $results[1]->error->class,
    $results[1]->error->message);
printf("slot2 ok=%s value=%s\n", $results[2]->ok ? "yes" : "no", var_export($results[2]->value, true));
?>
--EXPECT--
slot0 ok=yes value=1
slot1 ok=no class=RuntimeException message=kapow
slot2 ok=yes value=2
