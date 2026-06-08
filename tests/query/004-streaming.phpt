--TEST--
query() result streams rows lazily with foreach; supports parameters
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

$out = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);

    $rows = [];
    foreach ($client->query("SELECT number AS x FROM numbers(5)") as $k => $row) {
        $rows[$k] = $row['x'];
    }

    $filtered = [];
    foreach ($client->query("SELECT number AS x FROM numbers(10) WHERE number >= {min:UInt32}", ['min' => 8]) as $row) {
        $filtered[] = $row['x'];
    }

    return ['rows' => $rows, 'filtered' => $filtered];
}));

var_dump($out['rows']);
var_dump($out['filtered']);
?>
--EXPECT--
array(5) {
  [0]=>
  int(0)
  [1]=>
  int(1)
  [2]=>
  int(2)
  [3]=>
  int(3)
  [4]=>
  int(4)
}
array(2) {
  [0]=>
  int(8)
  [1]=>
  int(9)
}
