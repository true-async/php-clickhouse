--TEST--
Type mapping: pre-epoch DateTime64 floors the sub-second toward negative infinity
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

$out = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    // 0.5s before the epoch: ticks = -500 (scale 1000). The reader must floor
    // toward -inf (sec = -1, usec = 500000), not truncate toward zero.
    $d = $client->query("SELECT toDateTime64('1969-12-31 23:59:59.5', 3, 'UTC') AS d")->fetchOne();
    return [get_class($d), $d->format('Y-m-d H:i:s.u P')];
}));

var_dump($out[0]);
echo $out[1], "\n";
?>
--EXPECT--
string(17) "DateTimeImmutable"
1969-12-31 23:59:59.500000 +00:00
