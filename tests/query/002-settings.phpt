--TEST--
query() sends per-query settings via options
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

$mt = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    return $client->query(
        "SELECT getSetting('max_threads') AS mt",
        [],
        ['settings' => ['max_threads' => 3]]
    )->fetchAll()[0]['mt'];
}));

var_dump($mt);
?>
--EXPECT--
int(3)
