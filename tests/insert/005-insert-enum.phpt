--TEST--
insert() handles Enum columns (by label)
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

$values = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    $client->query("DROP TABLE IF EXISTS test_async_enum");
    $client->query("CREATE TABLE test_async_enum (e Enum8('a' = 1, 'b' = 2, 'c' = 3)) ENGINE = Memory");
    $client->insert("test_async_enum", ["e"], [["a"], ["c"]]);
    $rows = $client->query("SELECT e FROM test_async_enum ORDER BY e")->fetchAll();
    $client->query("DROP TABLE test_async_enum");
    return array_column($rows, 'e');
}));

var_dump($values);
?>
--EXPECT--
array(2) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "c"
}
