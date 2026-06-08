--TEST--
query() runs a SELECT and maps scalar types
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

$rows = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    return $client->query("SELECT toInt32(-5) AS i, toUInt64(42) AS u, toFloat64(2.5) AS f, 'x' AS s")->fetchAll();
}));

var_dump($rows);
?>
--EXPECT--
array(1) {
  [0]=>
  array(4) {
    ["i"]=>
    int(-5)
    ["u"]=>
    int(42)
    ["f"]=>
    float(2.5)
    ["s"]=>
    string(1) "x"
  }
}
