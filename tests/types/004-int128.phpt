--TEST--
Type mapping: Int128 / UInt128 -> string
--EXTENSIONS--
true_async_clickhouse
--SKIPIF--
<?php
require __DIR__ . '/../inc/clickhouse.inc';
clickhouse_skip_if_no_server();
?>
--FILE--
<?php
require __DIR__ . '/../inc/clickhouse.inc';

use TrueAsync\ClickHouse\Client;
use function Async\spawn;
use function Async\await;

$cfg = clickhouse_test_config();

$row = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    return $client->query(
        "SELECT toInt128('-170141183460469231731687303715884105728') AS i,"
        . " toUInt128('340282366920938463463374607431768211455') AS u"
    )->fetchAll()[0];
}));

var_dump($row['i']);
var_dump($row['u']);
?>
--EXPECT--
string(40) "-170141183460469231731687303715884105728"
string(39) "340282366920938463463374607431768211455"
