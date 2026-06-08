--TEST--
Type mapping: LowCardinality(String / Nullable(String)) read
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
    $client->query("DROP TABLE IF EXISTS test_async_lc");
    $client->query(
        "CREATE TABLE test_async_lc (" .
        "id UInt32, " .
        "s LowCardinality(String), " .
        "n LowCardinality(Nullable(String))" .
        ") ENGINE = Memory"
    );
    $client->query("INSERT INTO test_async_lc SELECT 1, 'hello', 'world'");
    $client->query("INSERT INTO test_async_lc SELECT 2, 'foo', NULL");

    $result = $client->query("SELECT s, n FROM test_async_lc ORDER BY id")->fetchAll();
    $client->query("DROP TABLE test_async_lc");

    return $result;
}));

var_dump($rows);
?>
--EXPECT--
array(2) {
  [0]=>
  array(2) {
    ["s"]=>
    string(5) "hello"
    ["n"]=>
    string(5) "world"
  }
  [1]=>
  array(2) {
    ["s"]=>
    string(3) "foo"
    ["n"]=>
    NULL
  }
}
