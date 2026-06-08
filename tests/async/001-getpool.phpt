--TEST--
getPool() exposes the underlying Async\Pool with live stats
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
$cfg['pool'] = ['max' => 4];

$stats = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    // Consume the result so its connection returns to the pool (a discarded,
    // unread result is treated as abandoned and its connection is dropped).
    $client->query("SELECT 1")->fetchAll();
    $pool = $client->getPool();

    return [
        $pool instanceof \Async\Pool,
        $pool->count(),
        $pool->idleCount(),
        $pool->activeCount(),
    ];
}));

var_dump($stats);
?>
--EXPECT--
array(4) {
  [0]=>
  bool(true)
  [1]=>
  int(1)
  [2]=>
  int(1)
  [3]=>
  int(0)
}
