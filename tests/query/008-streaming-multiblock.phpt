--TEST--
query() streams a result that spans several server blocks; rows pull lazily across block boundaries
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

// numbers(200000) is larger than one native protocol block, so iterating it
// makes the result pull (and adopt) more than one block from the server.
$info = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);

    $n = 200000;
    $count = 0;
    $sum = 0;
    foreach ($client->query("SELECT number FROM numbers({$n})") as $row) {
        $count++;
        $sum += $row['number'];
    }

    return [
        'count'  => $count,
        'sum_ok' => $sum === ($n * ($n - 1) / 2),
    ];
}));

echo "count: {$info['count']}\n";
echo "sum_ok: ", ($info['sum_ok'] ? 'yes' : 'no'), "\n";
?>
--EXPECT--
count: 200000
sum_ok: yes
