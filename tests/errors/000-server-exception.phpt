--TEST--
A query the server rejects raises ServerException; the connection survives
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
use TrueAsync\ClickHouse\ServerException;
use function Async\spawn;
use function Async\await;

$cfg = clickhouse_test_config();

$result = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);

    $out = ['class' => 'none', 'code' => 0];
    try {
        $client->query("SELECT * FROM no_such_table_xyz");
    } catch (ServerException $e) {
        $out['class'] = get_class($e);
        $out['code'] = $e->getCode();
    }

    // A server-side error must not poison the connection.
    $out['recovered'] = $client->query("SELECT 1 AS n")->fetchAll()[0]['n'];
    return $out;
}));

var_dump($result['class'] === ServerException::class);
var_dump($result['code'] > 0);
var_dump($result['recovered']);
?>
--EXPECT--
bool(true)
bool(true)
int(1)
