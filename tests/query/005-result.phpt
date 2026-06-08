--TEST--
Result: fetch / fetchOne / fetchAll, empty result for DDL, summary and affectedRows
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
use TrueAsync\ClickHouse\Summary;
use function Async\spawn;
use function Async\await;

$cfg = clickhouse_test_config();

$out = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);

    // fetchOne: first column of the first row.
    $one = $client->query("SELECT 42 AS n, 'x' AS s")->fetchOne();

    // fetch: one row at a time.
    $r = $client->query("SELECT number AS n FROM numbers(3)");
    $r1 = $r->fetch();
    $r2 = $r->fetch();

    // DDL yields an empty result (and executes even though we read nothing).
    $client->query("DROP TABLE IF EXISTS test_async_result");
    $ddlRows = $client->query("CREATE TABLE test_async_result (n UInt32) ENGINE = Memory")->fetchAll();

    // affectedRows from INSERT … SELECT (no rows to read; summary is ready).
    $written = $client->query("INSERT INTO test_async_result SELECT number FROM numbers(50)")->affectedRows();

    // summary on a SELECT is final once the result is drained.
    $sel = $client->query("SELECT n FROM test_async_result");
    $sel->fetchAll();
    $sum = $sel->summary();

    $client->query("DROP TABLE test_async_result");

    return [
        'one'      => $one,
        'r1'       => $r1,
        'r2'       => $r2,
        'ddlCount' => count($ddlRows),
        'written'  => $written,
        'isSummary'=> $sum instanceof Summary,
        'readRows' => $sum->readRows,
        'selWritten' => $sum->writtenRows,
        'elapsedOk'=> $sum->elapsed >= 0.0,
    ];
}));

echo "fetchOne: {$out['one']}\n";
var_dump($out['r1']);
var_dump($out['r2']);
echo "ddl rows: {$out['ddlCount']}\n";
echo "affectedRows >= 50: ", ($out['written'] >= 50) ? "yes" : "no", "\n";
echo "summary is Summary: ", $out['isSummary'] ? "yes" : "no", "\n";
echo "select readRows > 0: ", ($out['readRows'] > 0) ? "yes" : "no", "\n";
echo "select writtenRows: {$out['selWritten']}\n";
echo "elapsed >= 0: ", $out['elapsedOk'] ? "yes" : "no", "\n";
?>
--EXPECT--
fetchOne: 42
array(1) {
  ["n"]=>
  int(0)
}
array(1) {
  ["n"]=>
  int(1)
}
ddl rows: 0
affectedRows >= 50: yes
summary is Summary: yes
select readRows > 0: yes
select writtenRows: 0
elapsed >= 0: yes
