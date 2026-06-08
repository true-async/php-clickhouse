--TEST--
insert() handles UUID and Decimal columns
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
    $client->query("DROP TABLE IF EXISTS test_async_ud");
    $client->query("CREATE TABLE test_async_ud (id UUID, price Decimal64(4)) ENGINE = Memory");
    $client->insert("test_async_ud", ["id", "price"], [
        ["61f0c404-5cb3-11e7-907b-a6006ad3dba0", "1234.5678"],
    ]);
    $result = $client->query("SELECT toString(id) AS id, toString(price) AS price FROM test_async_ud")->fetchAll();
    $client->query("DROP TABLE test_async_ud");
    return $result;
}));

var_dump($rows);
?>
--EXPECT--
array(1) {
  [0]=>
  array(2) {
    ["id"]=>
    string(36) "61f0c404-5cb3-11e7-907b-a6006ad3dba0"
    ["price"]=>
    string(9) "1234.5678"
  }
}
