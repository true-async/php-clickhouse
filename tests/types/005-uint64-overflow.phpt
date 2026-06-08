--TEST--
Type mapping: UInt64 past PHP_INT_MAX overflows to float (PHP int-overflow rule)
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
    return $client->query(
        "SELECT toUInt64(18446744073709551615) AS max64," // 2^64 - 1
        . " toUInt64(9223372036854775808) AS over,"        // PHP_INT_MAX + 1
        . " toUInt64(9223372036854775807) AS atmax,"       // PHP_INT_MAX
        . " toUInt64(42) AS small"
    )->fetchAll()[0];
}));

// Values above PHP_INT_MAX become float (and keep the right magnitude); values
// at or below it stay int.
var_dump($row['max64'] === 18446744073709551615.0);
var_dump($row['over'] === 9223372036854775808.0);
var_dump($row['atmax'] === PHP_INT_MAX);
var_dump($row['small']);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
int(42)
