--TEST--
insert() handles Array columns, including Nullable elements and nested arrays
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
    $client->query("DROP TABLE IF EXISTS test_async_arr");
    $client->query(
        "CREATE TABLE test_async_arr (" .
        "a Array(UInt32), " .
        "b Array(String), " .
        "c Array(Nullable(Int32)), " .
        "d Array(Array(Int32))" .
        ") ENGINE = Memory"
    );

    $client->insert("test_async_arr", ["a", "b", "c", "d"], [
        [[1, 2, 3], ["x", "y"], [1, null, 3], [[1, 2], [3]]],
    ]);

    $result = $client->query("SELECT a, b, c, d FROM test_async_arr")->fetchAll();
    $client->query("DROP TABLE test_async_arr");

    return $result;
}));

var_dump($rows[0]);
?>
--EXPECT--
array(4) {
  ["a"]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(2)
    [2]=>
    int(3)
  }
  ["b"]=>
  array(2) {
    [0]=>
    string(1) "x"
    [1]=>
    string(1) "y"
  }
  ["c"]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    NULL
    [2]=>
    int(3)
  }
  ["d"]=>
  array(2) {
    [0]=>
    array(2) {
      [0]=>
      int(1)
      [1]=>
      int(2)
    }
    [1]=>
    array(1) {
      [0]=>
      int(3)
    }
  }
}
