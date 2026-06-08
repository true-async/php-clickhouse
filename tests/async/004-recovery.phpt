--TEST--
The pool recovers after a mid-query disconnect (next query gets a fresh conn)
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

// Evil peer: cut the FIRST connection mid-query; serve the SECOND transparently.
$proxy = spawn(function () use ($server, $upstream_addr) {
    $conn_no = 0;

    while (($client = @stream_socket_accept($server, 5)) !== false) {
        $conn_no++;
        $cut = ($conn_no === 1);
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
                    if ($cut && ++$client_chunks >= 2) {
                        break 2;
                    }
                    @fwrite($upstream, $data);
                } else {
                    @fwrite($client, $data);
                }
            }
        }

        @fclose($client);
        @fclose($upstream);

        if (!$cut) {
            break; // the transparent connection is done; stop accepting
        }
    }
});

$client = spawn(function () use ($port, $cfg) {
    $c = new Client([
        'host' => '127.0.0.1',
        'port' => $port,
        'user' => $cfg['user'],
        'password' => $cfg['password'],
        'compression' => Compression::None,
    ]);

    $out = [];
    try {
        $c->query("SELECT 1");
        $out[] = "no exception";
    } catch (ConnectionException $e) {
        $out[] = "ConnectionException";
    }

    // The pool must drop the dead connection and hand out a fresh one.
    $out[] = "recovered=" . $c->query("SELECT 1 AS n")->fetchAll()[0]['n'];

    unset($c); // close the pool so the proxy's second connection ends
    return implode(" | ", $out);
});

[$results] = await_all([$client, $proxy]);
echo $results[0], "\n";
fclose($server);
?>
--EXPECT--
ConnectionException | recovered=1
