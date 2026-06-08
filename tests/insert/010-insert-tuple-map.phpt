--TEST--
insert() handles Tuple and Map columns
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
    $client->query("DROP TABLE IF EXISTS test_async_tm");
    $client->query(
        "CREATE TABLE test_async_tm (" .
        "t Tuple(UInt32, String), " .
        "m Map(String, UInt32)" .
        ") ENGINE = Memory"
    );

    $client->insert("test_async_tm", ["t", "m"], [
        [[42, "hi"], ["a" => 1, "b" => 2]],
    ]);

    $result = $client->query("SELECT t, m FROM test_async_tm")->fetchAll();
    $client->query("DROP TABLE test_async_tm");

    return $result;
}));

var_dump($rows[0]);
?>
--EXPECT--
array(2) {
  ["t"]=>
  array(2) {
    [0]=>
    int(42)
    [1]=>
    string(2) "hi"
  }
  ["m"]=>
  array(2) {
    ["a"]=>
    int(1)
    ["b"]=>
    int(2)
  }
}
