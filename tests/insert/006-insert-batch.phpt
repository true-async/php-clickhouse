--TEST--
insertBatch() streams rows: append/count/flush, reuse, and a large batch
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
use function Async\spawn;
use function Async\await;

$cfg = clickhouse_test_config();

$out = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    $client->query("DROP TABLE IF EXISTS test_async_batch");
    $client->query("CREATE TABLE test_async_batch (id UInt32, name String) ENGINE = Memory");

    $batch = $client->insertBatch("test_async_batch", ["id", "name"]);

    // append buffers locally; count() reports pending rows.
    $batch->append([1, "a"]);
    $batch->append([2, "b"]);
    $pending = $batch->count();

    // flush sends the buffered rows and clears the buffer.
    $batch->flush();
    $afterFlush = $batch->count();

    // the same batch is reusable: append + flush again.
    $batch->append([3, "c"]);
    $batch->flush();

    // flushing with nothing buffered is a no-op.
    $batch->flush();

    // a larger batch to exercise the block write path.
    for ($i = 100; $i < 1100; $i++) {
        $batch->append([$i, "row{$i}"]);
    }
    $bigPending = $batch->count();
    $batch->flush();

    $total = $client->query("SELECT count() AS c FROM test_async_batch")->fetchAll()[0]['c'];
    $first = $client->query("SELECT id, name FROM test_async_batch ORDER BY id LIMIT 3")->fetchAll();

    $client->query("DROP TABLE test_async_batch");

    return compact('pending', 'afterFlush', 'bigPending', 'total', 'first');
}));

echo "pending after 2 appends: {$out['pending']}\n";
echo "pending after flush: {$out['afterFlush']}\n";
echo "pending in big batch: {$out['bigPending']}\n";
echo "total rows in table: {$out['total']}\n";
var_dump($out['first']);
?>
--EXPECT--
pending after 2 appends: 2
pending after flush: 0
pending in big batch: 1000
total rows in table: 1003
array(3) {
  [0]=>
  array(2) {
    ["id"]=>
    int(1)
    ["name"]=>
    string(1) "a"
  }
  [1]=>
  array(2) {
    ["id"]=>
    int(2)
    ["name"]=>
    string(1) "b"
  }
  [2]=>
  array(2) {
    ["id"]=>
    int(3)
    ["name"]=>
    string(1) "c"
  }
}
