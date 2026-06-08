--TEST--
LZ4 and ZSTD compression round-trip correctly
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
use function Async\spawn;
use function Async\await;

$base = clickhouse_test_config();

$ok = await(spawn(function () use ($base) {
    $out = [];
    foreach ([Compression::LZ4, Compression::ZSTD] as $method) {
        $cfg = $base;
        $cfg['compression'] = $method;
        $client = new Client($cfg);
        $rows = $client->query("SELECT number AS n FROM numbers(1000)")->fetchAll();
        $out[$method->value] = (count($rows) === 1000 && $rows[0]['n'] === 0 && $rows[999]['n'] === 999);
    }
    return $out;
}));

var_dump($ok['lz4']);
var_dump($ok['zstd']);
?>
--EXPECT--
bool(true)
bool(true)
