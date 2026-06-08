# Type mapping

How ClickHouse column types map to PHP values when **reading** (`query()`
results) and what each accepts when **writing** (`insert`/`insertBatch`).

## Reading: ClickHouse → PHP

| ClickHouse | PHP | Notes |
|------------|-----|-------|
| `Int8`…`Int64`, `UInt8`…`UInt32` | `int` | |
| `UInt64` | `int` / `float` | values above `PHP_INT_MAX` overflow to `float` |
| `Int128`, `UInt128` | `string` | decimal string (lossless) |
| `Float32`, `Float64` | `float` | |
| `String`, `FixedString` | `string` | |
| `Bool` | `bool` | |
| `Enum8`, `Enum16` | `string` | the label |
| `UUID` | `string` | canonical `8-4-4-4-12` |
| `IPv4`, `IPv6` | `string` | textual address |
| `Decimal`, `Decimal32/64/128` | `string` | lossless |
| `Date`, `Date32`, `DateTime`, `DateTime64` | `DateTimeImmutable` | UTC; `DateTime64` keeps sub-second precision |
| `Array(T)` | `list` | recursive |
| `Nullable(T)` | `null` \| `T` | |
| `Tuple(...)` | `list` | positional |
| `Map(K, V)` | `array` | associative |
| `LowCardinality(String)` | `string` | incl. `LowCardinality(Nullable(String))` → `null`\|`string` |

> `LowCardinality` over non-string types is not supported by the underlying
> clickhouse-cpp on the read path and is not produced.

## Writing: PHP → ClickHouse

Column types are taken from the server's INSERT sample block; the PHP value is
encoded to that type.

| ClickHouse | Accepts (PHP) |
|------------|---------------|
| `Int*`, `UInt*` | `int` |
| `Float32/64` | `float` or `int` |
| `String`, `FixedString` | `string` |
| `Bool` | `bool` |
| `Date`, `Date32`, `DateTime` | `int` (unix seconds) or `DateTimeInterface` |
| `DateTime64` | `int`/`float` seconds or `DateTimeInterface` (sub-second preserved) |
| `UUID` | `string` |
| `IPv4`, `IPv6` | `string` (textual address) |
| `Enum8`, `Enum16` | `string` label or `int` value |
| `Decimal`, `Decimal32/64/128` | `string` (or numeric; parsed at the column scale) |
| `Nullable(T)` | `null` or a `T` value |
| `Array(T)` | `list` (nested arrays and `Array(Nullable(T))` supported) |
| `Tuple(...)` | `list`, positional |
| `Map(K, V)` | associative `array` |

> `LowCardinality` columns cannot be written by this client yet (clickhouse-cpp
> limitation). Insert into the inner type or use `INSERT … SELECT`.

### Date / DateTime examples

```php
// All three of these write the same DateTime column:
$client->insert("t", ["ts"], [
    [1717761600],                                              // unix seconds
    [new DateTimeImmutable('2026-06-07 12:00:00', new DateTimeZone('UTC'))],
]);

// DateTime64(3) keeps milliseconds:
$client->insert("t64", ["ts"], [
    [new DateTimeImmutable('2026-06-07 12:00:00.500', new DateTimeZone('UTC'))],
]);
```

### Nested examples

```php
$client->insert("nested", ["arr", "tup", "m"], [
    [
        [[1, 2], [3]],            // Array(Array(Int32))
        [42, "hi"],               // Tuple(UInt32, String)
        ["a" => 1, "b" => 2],     // Map(String, UInt32)
    ],
]);
```
