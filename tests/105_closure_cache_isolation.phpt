--TEST--
Two structurally identical closures with different captures do not clobber each other (regression for krakjoe/parallel #309)
--EXTENSIONS--
parallax
--FILE--
<?php
$wg = new WaitGroup();
for ($i = 1; $i <= 4; $i++) {
    $tag = "task-{$i}";
    $wg->go(function () use ($i, $tag) {
        return ["i" => $i, "tag" => $tag];
    });
}
$r = $wg->wait();
foreach ($r as $idx => $slot) {
    echo json_encode($slot->value), "\n";
}
?>
--EXPECT--
{"i":1,"tag":"task-1"}
{"i":2,"tag":"task-2"}
{"i":3,"tag":"task-3"}
{"i":4,"tag":"task-4"}
