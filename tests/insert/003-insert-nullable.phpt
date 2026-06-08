--TEST--
insert() handles Nullable columns (null and non-null)
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
    $client->query("DROP TABLE IF EXISTS test_async_null");
    $client->query("CREATE TABLE test_async_null (id UInt32, name Nullable(String), n Nullable(Int32)) ENGINE = Memory");
    $client->insert("test_async_null", ["id", "name", "n"], [
        [1, "a", 42],
        [2, null, null],
    ]);
    $result = $client->query("SELECT * FROM test_async_null ORDER BY id")->fetchAll();
    $client->query("DROP TABLE test_async_null");
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
    ["n"]=>
    int(42)
  }
  [1]=>
  array(3) {
    ["id"]=>
    int(2)
    ["name"]=>
    NULL
    ["n"]=>
    NULL
  }
}
