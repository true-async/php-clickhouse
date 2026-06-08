--TEST--
insert() handles DateTime64 (sub-second), IPv4 and IPv6 columns
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
    $client->query("DROP TABLE IF EXISTS test_async_dt64");
    $client->query(
        "CREATE TABLE test_async_dt64 (ts DateTime64(3), v4 IPv4, v6 IPv6) ENGINE = Memory"
    );

    $when = new DateTimeImmutable('2021-06-07 12:00:00.500000', new DateTimeZone('UTC'));

    $client->insert("test_async_dt64", ["ts", "v4", "v6"], [
        [$when, "192.168.1.10", "2001:db8::1"],
    ]);

    $result = $client->query("SELECT ts, v4, v6 FROM test_async_dt64")->fetchAll();
    $client->query("DROP TABLE test_async_dt64");

    return $result;
}));

$row = $rows[0];
echo $row['ts']->format('Y-m-d H:i:s.u') . "\n";
echo $row['v4'] . "\n";
echo $row['v6'] . "\n";
?>
--EXPECT--
2021-06-07 12:00:00.500000
192.168.1.10
2001:db8::1
