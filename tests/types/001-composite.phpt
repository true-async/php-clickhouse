--TEST--
Type mapping: Array, Nullable, Tuple, Map (recursive)
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
    $sql = "SELECT"
        . " [1, 2, 3] AS arr,"
        . " CAST(NULL AS Nullable(Int32)) AS n0,"
        . " CAST(7 AS Nullable(Int32)) AS n1,"
        . " tuple(1, 'x', 2.5) AS tup,"
        . " map('a', 10, 'b', 20) AS m";
    return $client->query($sql)->fetchAll()[0];
}));

var_dump($row);
?>
--EXPECT--
array(5) {
  ["arr"]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(2)
    [2]=>
    int(3)
  }
  ["n0"]=>
  NULL
  ["n1"]=>
  int(7)
  ["tup"]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    string(1) "x"
    [2]=>
    float(2.5)
  }
  ["m"]=>
  array(2) {
    ["a"]=>
    int(10)
    ["b"]=>
    int(20)
  }
}
