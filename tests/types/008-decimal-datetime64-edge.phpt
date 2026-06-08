--TEST--
Type mapping: Decimal scale-0 and sub-1 scaling, DateTime64 from float/int, uppercase UUID, Map(Int) keys
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

$res = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);

    // Decimal(9,0) renders without a point; Decimal(9,3) of a sub-1 value pads
    // leading zeros. An uppercase UUID parses and reads back lowercase. A
    // Map(Int32, String) round-trips with integer keys.
    $client->query("DROP TABLE IF EXISTS test_async_edge");
    $client->query("CREATE TABLE test_async_edge (dec0 Decimal(9,0), dec3 Decimal(9,3), id UUID, m Map(Int32, String)) ENGINE = Memory");
    $client->insert("test_async_edge", ["dec0", "dec3", "id", "m"], [
        ["42", "0.5", "550E8400-E29B-41D4-A716-446655440000", [1 => "a", 2 => "b"]],
    ]);
    $full = $client->query("SELECT dec0, dec3, toString(id) AS id, m FROM test_async_edge")->fetchAll()[0];

    // DateTime64(3) ticks come from a float (sub-second) and from a whole-second int.
    $client->query("DROP TABLE IF EXISTS test_async_dt64");
    $client->query("CREATE TABLE test_async_dt64 (dt DateTime64(3)) ENGINE = Memory");
    $client->insert("test_async_dt64", ["dt"], [[1700000000.5], [1700000001]]);
    $ticks = array_column($client->query("SELECT toUnixTimestamp64Milli(dt) AS t FROM test_async_dt64 ORDER BY dt")->fetchAll(), "t");

    $client->query("DROP TABLE test_async_edge");
    $client->query("DROP TABLE test_async_dt64");
    return ["full" => $full, "ticks" => $ticks];
}));

var_dump($res);
?>
--EXPECT--
array(2) {
  ["full"]=>
  array(4) {
    ["dec0"]=>
    string(2) "42"
    ["dec3"]=>
    string(5) "0.500"
    ["id"]=>
    string(36) "550e8400-e29b-41d4-a716-446655440000"
    ["m"]=>
    array(2) {
      [1]=>
      string(1) "a"
      [2]=>
      string(1) "b"
    }
  }
  ["ticks"]=>
  array(2) {
    [0]=>
    int(1700000000500)
    [1]=>
    int(1700000001000)
  }
}
