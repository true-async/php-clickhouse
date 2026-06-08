--TEST--
A TLS handshake against a non-TLS endpoint fails as ConnectionException
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
use TrueAsync\ClickHouse\ConnectionException;
use function Async\spawn;
use function Async\await;

$cfg = clickhouse_test_config();

// Forcing TLS against the plaintext native port makes the ssl:// stream fail to
// connect; the transport surfaces that as a ConnectionException (no crash, no
// leak of the half-built stream).
$out = await(spawn(function () use ($cfg) {
    $client = new Client([
        'host'        => $cfg['host'],
        'port'        => $cfg['port'],
        'tls'         => true,
        'tls_verify'  => false,
        'user'        => $cfg['user'],
        'password'    => $cfg['password'],
        'compression' => Compression::None,
    ]);

    try {
        // The failed ssl:// handshake emits OpenSSL-version-dependent stream
        // warnings; we assert on the surfaced exception, so silence the noise.
        @$client->query("SELECT 1");
        return "no exception";
    } catch (ConnectionException $e) {
        return "ConnectionException";
    }
}));

echo "{$out}\n";
?>
--EXPECT--
ConnectionException
