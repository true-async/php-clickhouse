--TEST--
insert() accepts int timestamps and DateTimeInterface for Date/DateTime
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
    $client->query("DROP TABLE IF EXISTS test_async_dt");
    $client->query("CREATE TABLE test_async_dt (d Date, dt DateTime('UTC')) ENGINE = Memory");
    $client->insert("test_async_dt", ["d", "dt"], [
        [strtotime("2023-06-15 UTC"), new DateTimeImmutable("2023-06-15 12:30:45", new DateTimeZone("UTC"))],
    ]);
    $result = $client->query("SELECT toString(d) AS d, toString(dt) AS dt FROM test_async_dt")->fetchAll();
    $client->query("DROP TABLE test_async_dt");
    return $result;
}));

var_dump($rows);
?>
--EXPECT--
array(1) {
  [0]=>
  array(2) {
    ["d"]=>
    string(10) "2023-06-15"
    ["dt"]=>
    string(19) "2023-06-15 12:30:45"
  }
}
