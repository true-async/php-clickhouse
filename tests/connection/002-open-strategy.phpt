--TEST--
OpenStrategy: round_robin spreads pooled connections across hosts; in_order does not
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
$upstream = "tcp://{$cfg['host']}:{$cfg['port']}";

// Count how many pooled connections each of two hosts receives under a strategy.
// Both hosts are local proxies forwarding to the one real server, so the only
// difference between them is which one the strategy picks first.
function run_strategy(string $upstream, array $cfg, OpenStrategy $strategy): array
{
    $counts = [0, 0];
    $srv = [stream_socket_server('tcp://127.0.0.1:0'), stream_socket_server('tcp://127.0.0.1:0')];
    $ports = [];
    foreach ($srv as $s) {
        $n = stream_socket_get_name($s, false);
        $ports[] = (int) substr($n, strrpos($n, ':') + 1);
    }

    $proxies = [];
    foreach ([0, 1] as $i) {
        $proxies[] = spawn(function () use ($srv, $i, $upstream, &$counts) {
            while (($client = @stream_socket_accept($srv[$i], 2)) !== false) {
                $counts[$i]++;
                $up = @stream_socket_client($upstream, $e, $s, 2);

                while (true) {
                    $read = [$client, $up];
                    $w = [];
                    $x = [];
                    if (@stream_select($read, $w, $x, 2) <= 0) {
                        break;
                    }

                    foreach ($read as $r) {
                        $data = @fread($r, 65536);
                        if ($data === '' || $data === false) {
                            break 2;
                        }
                        @fwrite($r === $client ? $up : $client, $data);
                    }
                }

                @fclose($client);
                @fclose($up);
            }
        });
    }

    $runner = spawn(function () use ($ports, $cfg, $strategy) {
        $client = new Client([
            'hosts'         => ["127.0.0.1:{$ports[0]}", "127.0.0.1:{$ports[1]}"],
            'user'          => $cfg['user'],
            'password'      => $cfg['password'],
            'compression'   => Compression::None,
            'open_strategy' => $strategy,
            'pool'          => ['max' => 6],
        ]);

        // Overlapping queries force the pool to open several connections at once;
        // each one runs the strategy to pick its starting host.
        $coros = [];
        for ($i = 0; $i < 6; $i++) {
            $coros[] = spawn(fn () => $client->query("SELECT sleep(0.2)")->fetchAll());
        }
        await_all($coros);

        unset($client); // close the pool so the proxies' forward loops end
    });

    await_all(array_merge([$runner], $proxies));
    foreach ($srv as $s) {
        fclose($s);
    }

    return $counts;
}

[$io, $rr] = await(spawn(function () use ($upstream, $cfg) {
    return [
        run_strategy($upstream, $cfg, OpenStrategy::InOrder),
        run_strategy($upstream, $cfg, OpenStrategy::RoundRobin),
    ];
}));

// in_order keeps every connection on the primary; the second host stays a pure
// failover backup. round_robin must touch both hosts.
echo "in_order uses second host: ", ($io[1] > 0 ? "yes" : "no"), "\n";
echo "in_order uses primary: ", ($io[0] > 0 ? "yes" : "no"), "\n";
echo "round_robin uses both hosts: ", ($rr[0] > 0 && $rr[1] > 0 ? "yes" : "no"), "\n";
?>
--EXPECT--
in_order uses second host: no
in_order uses primary: yes
round_robin uses both hosts: yes
