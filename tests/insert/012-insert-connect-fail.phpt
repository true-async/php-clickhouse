--TEST--
insert() and insertBatch() against a dead port surface ConnectionException (acquire failure)
--EXTENSIONS--
true_async_clickhouse
--FILE--
<?php
use TrueAsync\ClickHouse\Client;
use TrueAsync\ClickHouse\ConnectionException;
use function Async\spawn;
use function Async\await;

// Both write entry points acquire a pooled connection first; when that fails
// (dead port), the acquire failure must surface as a ConnectionException, not a
// crash or a null Batch.
$out = await(spawn(function () {
    $client = new Client(['host' => '127.0.0.1', 'port' => 1]);
    $r = [];

    try {
        $client->insert('t', ['a'], [[1]]);
        $r[] = 'insert: no exception';
    } catch (ConnectionException $e) {
        $r[] = 'insert: ConnectionException';
    }

    try {
        $client->insertBatch('t', ['a']);
        $r[] = 'batch: no exception';
    } catch (ConnectionException $e) {
        $r[] = 'batch: ConnectionException';
    }

    return $r;
}));

echo implode("\n", $out), "\n";
?>
--EXPECT--
insert: ConnectionException
batch: ConnectionException
