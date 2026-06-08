--TEST--
A corrupted mid-stream response drops the connection; the pool recovers
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
use TrueAsync\ClickHouse\ClickHouseException;
use function Async\spawn;
use function Async\await_all;

$cfg = clickhouse_test_config();
$upstream = "tcp://{$cfg['host']}:{$cfg['port']}";

$server = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
$name = stream_socket_get_name($server, false);
$port = (int) substr($name, strrpos($name, ':') + 1);

// Evil peer. Connection #1: once the client has sent its query, flip a byte in
// the server's reply (the compressed result block), corrupting it mid-stream.
// Depending on where the flip lands this surfaces as a ProtocolException (block
// checksum) or a ConnectionException (a mangled length the read can't satisfy) —
// either way the connection is poisoned and must not be reused.
// Connection #2: forward transparently so the recovery query can succeed.
$proxy = spawn(function () use ($server, $upstream) {
    $conn_no = 0;

    while (($client = @stream_socket_accept($server, 5)) !== false) {
        $conn_no++;
        $corrupt = ($conn_no === 1);
        $up = stream_socket_client($upstream, $e, $s, 5);
        $client_chunks = 0;
        $poisoned = false;

        while (true) {
            $read = [$client, $up];
            $w = [];
            $x = [];

            if (@stream_select($read, $w, $x, 5) <= 0) {
                break;
            }

            foreach ($read as $r) {
                $data = @fread($r, 65536);
                if ($data === '' || $data === false) {
                    break 2;
                }

                if ($r === $client) {
                    $client_chunks++;
                    @fwrite($up, $data);
                } else {
                    // The first sizeable server reply after the query is the
                    // result block; corrupting any byte fails its checksum.
                    if ($corrupt && !$poisoned && $client_chunks >= 2 && strlen($data) > 40) {
                        $i = strlen($data) - 5;
                        $data[$i] = chr(ord($data[$i]) ^ 0xFF);
                        $poisoned = true;
                    }

                    @fwrite($client, $data);
                }
            }
        }

        @fclose($client);
        @fclose($up);

        if (!$corrupt) {
            break;
        }
    }
});

$client = spawn(function () use ($port, $cfg) {
    $c = new Client([
        'host'        => '127.0.0.1',
        'port'        => $port,
        'user'        => $cfg['user'],
        'password'    => $cfg['password'],
        'compression' => Compression::LZ4,
    ]);

    $out = [];
    try {
        $c->query("SELECT number FROM numbers(1000)")->fetchAll();
        $out[] = "no exception";
    } catch (ClickHouseException $e) {
        // Subclass (Protocol vs Connection) depends on where the corruption
        // lands; the guarantee under test is drop-and-recover, not the flavour.
        $out[] = "caught";
    }

    // The poisoned connection must have been dropped, so the next query gets a
    // fresh one and succeeds.
    $out[] = "recovered=" . $c->query("SELECT 7 AS n")->fetchAll()[0]['n'];

    unset($c); // close the pool so the proxy's second connection ends
    return implode(" | ", $out);
});

[$results] = await_all([$client, $proxy]);
echo $results[0], "\n";
fclose($server);
?>
--EXPECT--
caught | recovered=7
