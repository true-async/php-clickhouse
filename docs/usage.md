# Usage

All examples use the `TrueAsync\ClickHouse\Client` class. Every network
operation transparently yields the current coroutine, so **all client calls must
run inside a coroutine** (`Async\spawn`). Calling `query()`/`insert()` at the top
level (outside a coroutine) fails, because the handshake and reads need a
scheduler to suspend on.

```php
use TrueAsync\ClickHouse\Client;
use function Async\spawn;
use function Async\await_all;

$client = new Client(['host' => '127.0.0.1', 'user' => 'default']);

// Each query runs in its own coroutine, so the two execute concurrently —
// one Client, two connections from the hidden pool.
[$results] = await_all([
    spawn(fn() => $client->query("SELECT 1 AS n")->fetchOne()),
    spawn(fn() => $client->query("SELECT 2 AS n")->fetchOne()),
]);

var_dump($results); // [1, 2]
```

A single `Client` owns a hidden, per-coroutine [connection pool](pool.md):
concurrent coroutines each get their own physical connection, so one `Client`
can serve many simultaneous queries.

---

## Querying

### `query(string $sql, array $params = [], array $options = []): Result`

Runs a statement and returns a [`Result`](#the-result-object). Also used for
DDL (`CREATE`, `DROP`, …) and `INSERT … SELECT` (which give an empty result).
The statement has executed by the time `query()` returns.

```php
$result = $client->query("SELECT number, number * 2 AS doubled FROM numbers(3)");

$rows = $result->fetchAll();
// [
//   ['number' => 0, 'doubled' => 0],
//   ['number' => 1, 'doubled' => 2],
//   ['number' => 2, 'doubled' => 4],
// ]
```

### Parameter binding

Use ClickHouse's native `{name:Type}` placeholders. Values are bound
server-side and typed; they are never string-interpolated, so this is
injection-safe.

```php
$rows = $client->query(
    "SELECT * FROM events WHERE user_id = {uid:UInt64} AND day >= {since:Date}",
    ['uid' => 42, 'since' => '2026-01-01']
)->fetchAll();
```

### Per-query settings

Pass ClickHouse settings for a single query via `options['settings']`. They
override the connection's settings for that query only.

```php
$rows = $client->query(
    "SELECT * FROM big_table",
    [],
    ['settings' => ['max_threads' => '8', 'max_execution_time' => '30']]
)->fetchAll();
```

---

## The Result object

`query()` returns a `Result`: a single-pass, forward-only result you can either
**buffer** or **stream**. Statements with no rows (DDL, `INSERT … SELECT`) give
an empty result.


| Method | Description |
|--------|-------------|
| `fetchAll(): array` | All remaining rows at once (buffers). |
| `fetch(): ?array` | The next row (`column => value`), or `null` at the end. |
| `fetchOne(): mixed` | The first column of the next row; handy for scalar queries. |
| `foreach ($result as $row)` | Lazy streaming, one block at a time. |
| `affectedRows(): int` | Rows written by `INSERT … SELECT` (= `summary()->writtenRows`). |
| `summary(): Summary` | Server statistics for the query (see below). |

A `Result` is **single-pass**: pick one consumption style. Don't mix `foreach`
and `fetch()` on the same object; they share one stream position.

### Buffer vs. stream

```php
// Buffer: simplest; the whole result in memory.
$rows = $client->query("SELECT * FROM small_table")->fetchAll();

// Stream: bounded memory, ideal for large result sets. Rows arrive block by
// block in your coroutine.
foreach ($client->query("SELECT * FROM huge_table") as $row) {
    handle($row);
}

// Scalar.
$count = $client->query("SELECT count() FROM events")->fetchOne();
```

Streaming is single-pass; breaking out early is fine. The connection (which
still holds unread data) is dropped and the pool hands out a fresh one next time.
The same applies to a result you discard without reading: prefer `fetchAll()` (or
just rely on DDL / `INSERT … SELECT`, which complete on their own) when you only
want the side effect, otherwise that connection is dropped rather than reused.

### Query statistics: `summary()`

`summary()` returns a `Summary` value object with server-reported counters. It is
final once the result is fully consumed (and immediate for no-row statements):

```php
$result = $client->query("SELECT * FROM events WHERE day = today() LIMIT 100");
$rows   = $result->fetchAll();

$s = $result->summary();
$s->readRows;          // rows the server scanned
$s->readBytes;
$s->rowsBeforeLimit;   // rows that matched before LIMIT (null if no LIMIT); paging
$s->elapsed;           // seconds

// "Affected rows" for a server-side insert:
$n = $client->query("INSERT INTO dst SELECT * FROM src")->affectedRows();
```

`Summary` fields: `readRows`, `readBytes`, `writtenRows`, `writtenBytes`,
`totalRowsToRead`, `rowsBeforeLimit` (`?int`), `elapsed` (`float`).

---

## Inserting

### `insert(string $table, array $columns, array $rows): void`

Columnar batch insert. Column types come from the server's INSERT sample block,
so PHP values are encoded to the real column types (no client-side guessing).

```php
$client->insert("events", ["id", "name", "score"], [
    [1, "a", 1.5],
    [2, "b", 2.5],
]);
```

Each row is positional: its values must line up with `$columns`. A wrong value
count raises `\ValueError`. See [types.md](types.md) for what each ClickHouse
type accepts.

### Streaming inserts: `insertBatch()`

### `insertBatch(string $table, array $columns): Batch`

For large inserts that should not be built fully in memory. Append rows, then
`flush()` to send them as one insert. Flushing applies **async backpressure**:
if the socket buffer fills, the coroutine yields until it drains, so the
producer cannot outrun the server.

```php
$batch = $client->insertBatch("events", ["id", "name"]);

foreach ($source as $i => $name) {
    $batch->append([$i, $name]);

    if ($batch->count() >= 10_000) {
        $batch->flush();          // send this chunk
    }
}

$batch->flush();                  // send the remainder
```

`Batch` methods:

| Method | Description |
|--------|-------------|
| `append(array $row): void` | Buffer one positional row (no network). |
| `flush(): void` | Send all buffered rows as one insert; a no-op when empty. |
| `count(): int` | Number of buffered rows not yet flushed. |

The batch holds one pooled connection for its lifetime and returns it when the
object is destroyed. **Unflushed rows are discarded on destruction**, so always
`flush()` to persist the tail.

---

## The connection pool: `getPool()`

### `getPool(): \Async\Pool`

Returns the underlying TrueAsync `Async\Pool` wrapper, an advanced escape
hatch for stats, lifecycle and circuit-breaking. Normal `query`/`insert` calls
acquire and release connections automatically; you rarely need this. See
[pool.md](pool.md).

```php
$pool = $client->getPool();
echo $pool->count(), " connections, ", $pool->idleCount(), " idle\n";
```

---

## Error handling

All errors are exceptions. The client hierarchy (all extend
`\RuntimeException`):

| Exception | When |
|-----------|------|
| `ClickHouseException` | base class for everything below |
| `ConnectionException` | connect/read/write failure, EOF, cancellation |
| `ServerException` | the server rejected the query (`getCode()` is the ClickHouse error code) |
| `ProtocolException` | protocol decode, checksum or compression error |

**Caller mistakes** (a malformed row, a wrong value count) raise PHP's standard
`\ValueError` instead; they are a `LogicException`, not a runtime fault.

```php
use TrueAsync\ClickHouse\ServerException;

try {
    $client->query("SELECT * FROM does_not_exist");
} catch (ServerException $e) {
    echo "ClickHouse error {$e->getCode()}: {$e->getMessage()}\n";
}
```

A connection that dies mid-operation is never returned to the pool; the next
call transparently gets a fresh one.
