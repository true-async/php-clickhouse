--TEST--
insert() columnar batch insert round-trips through the server
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
    $client->query("DROP TABLE IF EXISTS test_async_ins");
    $client->query("CREATE TABLE test_async_ins (id UInt32, name String, score Float64) ENGINE = Memory");
    $client->insert("test_async_ins", ["id", "name", "score"], [
        [1, "a", 1.5],
        [2, "b", 2.5],
    ]);
    $result = $client->query("SELECT * FROM test_async_ins ORDER BY id")->fetchAll();
    $client->query("DROP TABLE test_async_ins");
    return $result;
}));

var_dump($rows);
?>
--EXPECT--
array(2) {
  [0]=>
  array(3) {
    ["id"]=>
    int(1)
    ["name"]=>
    string(1) "a"
    ["score"]=>
    float(1.5)
  }
  [1]=>
  array(3) {
    ["id"]=>
    int(2)
    ["name"]=>
    string(1) "b"
    ["score"]=>
    float(2.5)
  }
}
