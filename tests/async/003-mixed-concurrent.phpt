--TEST--
Concurrent coroutines running different query shapes
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
    $client->query("DROP TABLE IF EXISTS test_async_mixed");
    $client->query("CREATE TABLE test_async_mixed (id UInt32) ENGINE = Memory");

    $coros = [
        'num'    => spawn(fn () => $client->query("SELECT 42 AS n")->fetchAll()[0]['n']),
        'str'    => spawn(fn () => $client->query("SELECT 'hello' AS s")->fetchAll()[0]['s']),
        'count'  => spawn(fn () => $client->query("SELECT count() AS c FROM numbers(100)")->fetchAll()[0]['c']),
        'param'  => spawn(fn () => $client->query("SELECT {x:UInt32} AS x", ['x' => 7])->fetchAll()[0]['x']),
        'insert' => spawn(fn () => $client->insert("test_async_mixed", ["id"], [[1], [2]])),
    ];

    $out = await_all($coros);
    $client->query("DROP TABLE test_async_mixed");
    return $out;
}));

ksort($results);
var_dump($results);
var_dump(count($errors));
?>
--EXPECT--
array(5) {
  ["count"]=>
  int(100)
  ["insert"]=>
  NULL
  ["num"]=>
  int(42)
  ["param"]=>
  int(7)
  ["str"]=>
  string(5) "hello"
}
int(0)
