--TEST--
A batch flush whose connection dies surfaces ConnectionException; pool recovers
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
use function Async\await_all;

$cfg = clickhouse_test_config();
$upstream_addr = "tcp://{$cfg['host']}:{$cfg['port']}";

// A direct (un-proxied) client owns the table lifecycle (queries run in a
// coroutine: the handshake IO suspends).
$direct = new Client($cfg);
await(spawn(function () use ($direct) {
    $direct->query("DROP TABLE IF EXISTS test_async_batch_chaos");
    $direct->query("CREATE TABLE test_async_batch_chaos (id UInt32) ENGINE = Memory");
}));

$server = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
$name = stream_socket_get_name($server, false);
$port = (int) substr($name, strrpos($name, ':') + 1);

// Evil peer: cut the FIRST connection mid-insert (client's 2nd message, the
// BeginInsert); serve the SECOND connection transparently.
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
            break;
        }
    }
});

$worker = spawn(function () use ($port, $cfg) {
    $c = new Client([
        'host' => '127.0.0.1',
        'port' => $port,
        'user' => $cfg['user'],
        'password' => $cfg['password'],
        'compression' => Compression::None,
    ]);

    $out = [];

    $batch = $c->insertBatch("test_async_batch_chaos", ["id"]);
    $batch->append([1]);

    try {
        $batch->flush();
        $out[] = "no exception";
    } catch (ConnectionException $e) {
        $out[] = "ConnectionException";
    }

    unset($batch); // drop the broken connection back to the pool

    // The pool must drop the dead connection and hand out a fresh one.
    $out[] = "recovered=" . $c->query("SELECT 1 AS n")->fetchAll()[0]['n'];

    unset($c);
    return implode(" | ", $out);
});

[$results] = await_all([$worker, $proxy]);
echo $results[0], "\n";

await(spawn(function () use ($direct) {
    $direct->query("DROP TABLE test_async_batch_chaos");
}));
fclose($server);
?>
--EXPECT--
ConnectionException | recovered=1
