--TEST--
A mid-query disconnect (evil peer) surfaces as ConnectionException
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
use function Async\await_all;

$cfg = clickhouse_test_config();
$upstream_addr = "tcp://{$cfg['host']}:{$cfg['port']}";

$server = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
$name = stream_socket_get_name($server, false);
$port = (int) substr($name, strrpos($name, ':') + 1);

// Evil peer: a single-coroutine proxy that forwards to ClickHouse but cuts the
// connection on the client's second message (the query) — i.e. mid-query.
$proxy = spawn(function () use ($server, $upstream_addr) {
    $client = stream_socket_accept($server, 5);
    $upstream = stream_socket_client($upstream_addr, $e, $s, 5);
    $client_chunks = 0;

    while (true) {
        $read = [$client, $upstream];
        $write = [];
        $except = [];

        if (@stream_select($read, $write, $except, 5) <= 0) {
            break;
        }

        foreach ($read as $r) {
            $data = @fread($r, 65536);
            if ($data === '' || $data === false) {
                break 2;
            }

            if ($r === $client) {
                if (++$client_chunks >= 2) {
                    break 2; // the query — cut the connection here
                }
                @fwrite($upstream, $data);
            } else {
                @fwrite($client, $data);
            }
        }
    }

    @fclose($client);
    @fclose($upstream);
});

$client = spawn(function () use ($port, $cfg) {
    try {
        $c = new Client([
            'host' => '127.0.0.1',
            'port' => $port,
            'user' => $cfg['user'],
            'password' => $cfg['password'],
            'compression' => Compression::None,
        ]);
        $c->query("SELECT 1");
        return "no exception";
    } catch (ConnectionException $e) {
        return "ConnectionException";
    } catch (\Throwable $e) {
        return "other: " . get_class($e);
    }
});

[$results] = await_all([$client, $proxy]);
echo $results[0], "\n";
fclose($server);
?>
--EXPECT--
ConnectionException
