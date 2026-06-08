--TEST--
A directly constructed Result has no data: every accessor is safe; an unbacked Batch errors
--EXTENSIONS--
true_async_clickhouse
--FILE--
<?php

use TrueAsync\ClickHouse\Result;
use TrueAsync\ClickHouse\Batch;
use TrueAsync\ClickHouse\Summary;

// A Result with no underlying stream (no Client::query backing it) must not
// crash: the accessors all return their empty/null forms.
$r = new Result();

var_dump($r->fetch());
var_dump($r->fetchAll());
var_dump($r->fetchOne());
var_dump($r->valid());
var_dump($r->key());
var_dump($r->current());

// summary() builds a zeroed Summary; rowsBeforeLimit is null when no LIMIT applied.
$s = $r->summary();
var_dump($s instanceof Summary, $s->readRows, $s->rowsBeforeLimit, $s->elapsed);

// foreach over an empty Result yields nothing.
$n = 0;
foreach ($r as $row) {
    $n++;
}
echo "iterated rows: {$n}\n";

// A Batch built directly (no insertBatch) has no connection; using it is an Error.
try {
    (new Batch())->count();
} catch (\Error $e) {
    echo "count: Error\n";
}
?>
--EXPECT--
NULL
array(0) {
}
NULL
bool(false)
int(-1)
NULL
bool(true)
int(0)
NULL
float(0)
iterated rows: 0
count: Error
