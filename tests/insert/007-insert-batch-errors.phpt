--TEST--
insertBatch() error paths: row arity, direct construction, server error
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
use TrueAsync\ClickHouse\Batch;
use TrueAsync\ClickHouse\ServerException;
use function Async\spawn;
use function Async\await;

$cfg = clickhouse_test_config();

await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    $client->query("DROP TABLE IF EXISTS test_async_batch_err");
    $client->query("CREATE TABLE test_async_batch_err (id UInt32, name String) ENGINE = Memory");

    $batch = $client->insertBatch("test_async_batch_err", ["id", "name"]);

    // A row whose value count differs from the column count is a caller error.
    try {
        $batch->append([1]);
    } catch (\ValueError $e) {
        echo "arity: ValueError\n";
    }

    // The bad row was not buffered.
    echo "pending: {$batch->count()}\n";

    $client->query("DROP TABLE test_async_batch_err");
}));

// A directly constructed Batch has no connection; using it is an error.
try {
    (new Batch())->flush();
} catch (\Error $e) {
    echo "construct: Error\n";
}

// Flushing into a missing table surfaces the server error.
await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    $batch = $client->insertBatch("no_such_table_xyz", ["id"]);
    $batch->append([1]);

    try {
        $batch->flush();
    } catch (ServerException $e) {
        echo "flush: ServerException code>0: " . ($e->getCode() > 0 ? "yes" : "no") . "\n";
    }
}));
?>
--EXPECT--
arity: ValueError
pending: 0
construct: Error
flush: ServerException code>0: yes
