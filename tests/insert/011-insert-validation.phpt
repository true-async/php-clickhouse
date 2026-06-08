--TEST--
insert() validation: non-array row, and Array/Tuple/Map cell shape errors raise \ValueError; the connection survives
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
    $r = [];
    $try = function ($label, $fn) use (&$r) {
        try {
            $fn();
            $r[] = "{$label}: no exception";
        } catch (\ValueError $e) {
            $r[] = "{$label}: ValueError";
        }
    };

    // A row that is not an array is a caller error.
    $client->query("DROP TABLE IF EXISTS test_async_verr");
    $client->query("CREATE TABLE test_async_verr (a UInt32, b String) ENGINE = Memory");
    $try("row_not_array", fn () => $client->insert("test_async_verr", ["a", "b"], [[1, "x"], 42]));

    // An Array column needs a PHP array cell.
    $client->query("DROP TABLE IF EXISTS test_async_arr");
    $client->query("CREATE TABLE test_async_arr (arr Array(Int32)) ENGINE = Memory");
    $try("array_not_array", fn () => $client->insert("test_async_arr", ["arr"], [[42]]));

    // A Tuple column needs a PHP array of the exact arity.
    $client->query("DROP TABLE IF EXISTS test_async_tup");
    $client->query("CREATE TABLE test_async_tup (t Tuple(Int32, String)) ENGINE = Memory");
    $try("tuple_not_array", fn () => $client->insert("test_async_tup", ["t"], [[42]]));
    $try("tuple_too_many", fn () => $client->insert("test_async_tup", ["t"], [[[1, "x", 999]]]));
    $try("tuple_too_few", fn () => $client->insert("test_async_tup", ["t"], [[[1]]]));

    // A Map column needs a PHP array cell.
    $client->query("DROP TABLE IF EXISTS test_async_map");
    $client->query("CREATE TABLE test_async_map (m Map(String, Int32)) ENGINE = Memory");
    $try("map_not_array", fn () => $client->insert("test_async_map", ["m"], [[42]]));

    // The connection is still usable after every caught error.
    $r[] = "alive: " . $client->query("SELECT 7 AS n")->fetchAll()[0]["n"];

    foreach (["test_async_verr", "test_async_arr", "test_async_tup", "test_async_map"] as $t) {
        $client->query("DROP TABLE {$t}");
    }

    return $r;
}));

foreach ($out as $line) {
    echo $line, "\n";
}
?>
--EXPECT--
row_not_array: ValueError
array_not_array: ValueError
tuple_not_array: ValueError
tuple_too_many: ValueError
tuple_too_few: ValueError
map_not_array: ValueError
alive: 7
