--TEST--
TLS connection (php_stream ssl://) runs a query
--EXTENSIONS--
true_async_clickhouse
--SKIPIF--
<?php
require __DIR__ . '/../inc/clickhouse.inc';
clickhouse_skip_if_no_server();
$cfg = clickhouse_test_config();
$tls_port = (int) (getenv('CLICKHOUSE_TLS_PORT') ?: 9440);
$fp = @fsockopen($cfg['host'], $tls_port, $errno, $errstr, 1.0);
if ($fp === false) {
    die("skip ClickHouse TLS port {$tls_port} not reachable");
}
fclose($fp);
?>
--FILE--
<?php
require __DIR__ . '/../inc/clickhouse.inc';

use TrueAsync\ClickHouse\Client;
use TrueAsync\ClickHouse\Compression;
use function Async\spawn;
use function Async\await;

$cfg = clickhouse_test_config();
$tls_port = (int) (getenv('CLICKHOUSE_TLS_PORT') ?: 9440);

$n = await(spawn(function () use ($cfg, $tls_port) {
    $client = new Client([
        'host'        => $cfg['host'],
        'port'        => $tls_port,
        'tls'         => true,
        'tls_verify'  => false, // self-signed test cert
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
