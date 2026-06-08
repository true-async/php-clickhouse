--TEST--
query() native {name:Type} parameter binding is typed and injection-safe
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

$res = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    return [
        $client->query("SELECT {n:UInt32} AS n", ['n' => 42])->fetchAll(),
        $client->query("SELECT {s:String} AS s", ['s' => "x' OR 1=1"])->fetchAll(),
        $client->query(
            "SELECT number FROM numbers(10) WHERE number > {min:UInt8} ORDER BY number",
            ['min' => 7]
        )->fetchAll(),
    ];
}));

var_dump($res[0][0]['n']);
var_dump($res[1][0]['s']);
var_dump(array_column($res[2], 'number'));
?>
--EXPECT--
int(42)
string(9) "x' OR 1=1"
array(2) {
  [0]=>
  int(8)
  [1]=>
  int(9)
}
