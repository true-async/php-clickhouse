--TEST--
A 'hosts' entry with no :port falls back to the client's default port
--EXTENSIONS--
true_async_clickhouse
--SKIPIF--
<?php
require __DIR__ . '/../inc/clickhouse.inc';
clickhouse_skip_if_no_server();
$cfg = clickhouse_test_config();
if ($cfg['port'] !== 9000) {
    die("skip default-port path needs the server on 9000 (got {$cfg['port']})");
}
?>
--FILE--
<?php
require __DIR__ . '/../inc/clickhouse.inc';

use TrueAsync\ClickHouse\Client;
use TrueAsync\ClickHouse\Compression;
use function Async\spawn;
use function Async\await;

$cfg = clickhouse_test_config();

// "127.0.0.1" with no ":port" parses to host-only; the port defaults to the
// client's port (9000 for plaintext).
$n = await(spawn(function () use ($cfg) {
    $client = new Client([
        'hosts'       => [$cfg['host']],
        'user'        => $cfg['user'],
        'password'    => $cfg['password'],
        'compression' => Compression::None,
    ]);

    return $client->query("SELECT 9 AS n")->fetchAll()[0]['n'];
}));

echo "n: {$n}\n";
?>
--EXPECT--
n: 9
