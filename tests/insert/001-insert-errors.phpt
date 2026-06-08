--TEST--
insert() rejects malformed rows with \ValueError (caller error)
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
    $client->query("DROP TABLE IF EXISTS test_async_ins_err");
    $client->query("CREATE TABLE test_async_ins_err (a UInt32, b String) ENGINE = Memory");

    $results = [];

    try {
        $client->insert("test_async_ins_err", ["a", "b"], [[1, "x", 999]]);
        $results[] = "no exception";
    } catch (\ValueError $e) {
        $results[] = "ValueError";
    }

    try {
        $client->insert("test_async_ins_err", ["a", "b"], [[1]]);
        $results[] = "no exception";
    } catch (\ValueError $e) {
        $results[] = "ValueError";
    }

    $client->query("DROP TABLE test_async_ins_err");
    return $results;
}));

var_dump($out);
?>
--EXPECT--
array(2) {
  [0]=>
  string(10) "ValueError"
  [1]=>
  string(10) "ValueError"
}
