--TEST--
Concurrent coroutines each get their own pooled connection
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
use function Async\await_all;

$cfg = clickhouse_test_config();
$cfg['pool'] = ['max' => 8];

[$results, $errors] = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);

    $coros = [];
    for ($i = 1; $i <= 5; $i++) {
        // sleep() forces the queries to overlap; a single shared connection
        // would corrupt the protocol stream or serialize them.
        $coros[$i] = spawn(fn () =>
            $client->query("SELECT {id:UInt32} AS id, sleep(0.1) AS s", ['id' => $i])->fetchAll()[0]['id']);
    }

    return await_all($coros);
}));

ksort($results);
var_dump($results);
var_dump(count($errors));
?>
--EXPECT--
array(5) {
  [1]=>
  int(1)
  [2]=>
  int(2)
  [3]=>
  int(3)
  [4]=>
  int(4)
  [5]=>
  int(5)
}
int(0)
