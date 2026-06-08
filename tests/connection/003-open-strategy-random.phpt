--TEST--
OpenStrategy::Random picks a starting host per pooled connection; a non-enum value is a TypeError
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
use TrueAsync\ClickHouse\Compression;
use TrueAsync\ClickHouse\OpenStrategy;
use function Async\spawn;
use function Async\await;
use function Async\await_all;

$cfg = clickhouse_test_config();
$hp = "{$cfg['host']}:{$cfg['port']}";

// 'open_strategy' must be an OpenStrategy enum, not an arbitrary value.
try {
    new Client(['open_strategy' => 'round_robin']);
} catch (\TypeError $e) {
    echo "type: TypeError\n";
}

// Random spreads pooled connections over the hosts. Both entries point at the
// one real server, so whichever host each connection randomly starts on, the
// query succeeds; we assert correctness rather than a particular split.
[$results, $errors] = await(spawn(function () use ($cfg, $hp) {
    $client = new Client([
        'hosts'         => [$hp, $hp],
        'user'          => $cfg['user'],
        'password'      => $cfg['password'],
        'compression'   => Compression::None,
        'open_strategy' => OpenStrategy::Random,
        'pool'          => ['max' => 4],
    ]);

    $coros = [];
    for ($i = 0; $i < 4; $i++) {
        $coros[] = spawn(fn () => $client->query("SELECT 1 AS n")->fetchAll()[0]['n']);
    }

    return await_all($coros);
}));

echo "queries: ", count($results), "\n";
echo "sum: ", array_sum($results), "\n";
echo "errors: ", count($errors), "\n";
?>
--EXPECT--
type: TypeError
queries: 4
sum: 4
errors: 0
