--TEST--
Type mapping: narrow integer widths, Float32, Bool, FixedString, Date32, Enum (insert + read)
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

$rows = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    $client->query("DROP TABLE IF EXISTS test_async_widths");
    $client->query(
        "CREATE TABLE test_async_widths (" .
        "i8 Int8, i16 Int16, i64 Int64, " .
        "u8 UInt8, u16 UInt16, u64 UInt64, " .
        "f32 Float32, b Bool, fs FixedString(3), d32 Date32, " .
        "e8 Enum8('x' = 1, 'y' = 2), e16 Enum16('p' = 1, 'q' = 2)" .
        ") ENGINE = Memory"
    );

    $cols = ["i8", "i16", "i64", "u8", "u16", "u64", "f32", "b", "fs", "d32", "e8", "e16"];
    $date = new DateTimeImmutable("2020-01-02 00:00:00", new DateTimeZone("UTC"));

    // Row 1 appends Enum8 by int (2 -> 'y') and Enum16 by label ('q').
    // Row 2 appends Enum16 by int (1 -> 'p').
    $client->insert("test_async_widths", $cols, [
        [-8, -1600, -64, 200, 40000, 64, 1.5, true, "abc", $date, 2, "q"],
        [-7, -1500, -63, 201, 40001, 65, 2.5, false, "def", $date, 1, 1],
    ]);

    // Select d32 as a native Date32 (not toString) so the Date32 read path maps
    // it to a DateTimeImmutable; render it back to a stable string for output.
    $out = $client->query(
        "SELECT i8, i16, i64, u8, u16, u64, f32, b, fs, d32, e8, e16 " .
        "FROM test_async_widths ORDER BY i8"
    )->fetchAll();

    foreach ($out as &$row) {
        $row['d32'] = $row['d32']->format('Y-m-d');
    }
    unset($row);

    $client->query("DROP TABLE test_async_widths");
    return $out;
}));

var_dump($rows);
?>
--EXPECT--
array(2) {
  [0]=>
  array(12) {
    ["i8"]=>
    int(-8)
    ["i16"]=>
    int(-1600)
    ["i64"]=>
    int(-64)
    ["u8"]=>
    int(200)
    ["u16"]=>
    int(40000)
    ["u64"]=>
    int(64)
    ["f32"]=>
    float(1.5)
    ["b"]=>
    bool(true)
    ["fs"]=>
    string(3) "abc"
    ["d32"]=>
    string(10) "2020-01-02"
    ["e8"]=>
    string(1) "y"
    ["e16"]=>
    string(1) "q"
  }
  [1]=>
  array(12) {
    ["i8"]=>
    int(-7)
    ["i16"]=>
    int(-1500)
    ["i64"]=>
    int(-63)
    ["u8"]=>
    int(201)
    ["u16"]=>
    int(40001)
    ["u64"]=>
    int(65)
    ["f32"]=>
    float(2.5)
    ["b"]=>
    bool(false)
    ["fs"]=>
    string(3) "def"
    ["d32"]=>
    string(10) "2020-01-02"
    ["e8"]=>
    string(1) "x"
    ["e16"]=>
    string(1) "p"
  }
}
