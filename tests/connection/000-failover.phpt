--TEST--
A dead primary host fails over to the next host in 'hosts'
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

$cfg = clickhouse_test_config();

$n = await(spawn(function () use ($cfg) {
    $client = new Client([
        'hosts'       => ['127.0.0.1:1', "{$cfg['host']}:{$cfg['port']}"],
        'user'        => $cfg['user'],
        'password'    => $cfg['password'],
        'compression' => Compression::None,
    ]);
    return $client->query("SELECT 1 AS n")->fetchAll()[0]['n'];
}));

var_dump($n);
?>
--EXPECT--
int(1)
