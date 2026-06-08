--TEST--
streaming a query() result surfaces server errors; pool recovers after error / early break
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

$out = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);

    $error = false;
    try {
        foreach ($client->query("SELECT * FROM no_such_table_zzz") as $row) {
        }
    } catch (ServerException $e) {
        $error = true;
    }

    $after_error = $client->query("SELECT 1 AS n")->fetchAll()[0]['n'];

    $seen = 0;
    foreach ($client->query("SELECT number FROM numbers(1000)") as $row) {
        if (++$seen >= 3) {
            break;
        }
    }

    $after_break = $client->query("SELECT 2 AS n")->fetchAll()[0]['n'];

    return [$error, $after_error, $seen, $after_break];
}));

var_dump($out[0]);
var_dump($out[1]);
var_dump($out[2]);
var_dump($out[3]);
?>
--EXPECT--
bool(true)
int(1)
int(3)
int(2)
