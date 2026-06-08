--TEST--
query() native params: PHP null binds as SQL NULL; true/false bind as Bool
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

    // A PHP null param becomes a NULL value (std::nullopt), not the string
    // "null"; PHP true/false become the Bool literals true/false.
    return $client->query(
        "SELECT {a:Nullable(Int32)} AS a, isNull({a:Nullable(Int32)}) AS a_is_null, " .
        "{b:Bool} AS b, {c:Bool} AS c",
        ["a" => null, "b" => true, "c" => false]
    )->fetchAll()[0];
}));

var_dump($row);
?>
--EXPECT--
array(4) {
  ["a"]=>
  NULL
  ["a_is_null"]=>
  int(1)
  ["b"]=>
  bool(true)
  ["c"]=>
  bool(false)
}
