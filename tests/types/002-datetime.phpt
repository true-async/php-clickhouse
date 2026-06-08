--TEST--
Type mapping: Date, DateTime, DateTime64 -> DateTimeImmutable
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
    $sql = "SELECT"
        . " toDate('2023-06-15') AS d,"
        . " toDateTime('2023-06-15 12:30:45', 'UTC') AS dt,"
        . " toDateTime64('2023-06-15 12:30:45.123', 3, 'UTC') AS dt64";
    return $client->query($sql)->fetchAll()[0];
}));

foreach (['d', 'dt', 'dt64'] as $key) {
    $value = $row[$key];
    echo $key, ' ', get_class($value), ' ', $value->format('Y-m-d H:i:s.u P'), "\n";
}
?>
--EXPECT--
d DateTimeImmutable 2023-06-15 00:00:00.000000 +00:00
dt DateTimeImmutable 2023-06-15 12:30:45.000000 +00:00
dt64 DateTimeImmutable 2023-06-15 12:30:45.123000 +00:00
