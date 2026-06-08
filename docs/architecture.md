# php-clickhouse: Design

Native asynchronous ClickHouse client for **PHP TrueAsync**, built on the official
[`ClickHouse/clickhouse-cpp`](https://github.com/ClickHouse/clickhouse-cpp) native-protocol
C++ client (Apache-2.0, included as a git submodule).

The extension exposes a PDO-like API. Every network operation looks synchronous but
transparently yields the current coroutine: "write sync, run async".

> **This is the design document.** For the API as currently implemented, see
> [usage.md](usage.md), [configuration.md](configuration.md) and
> [types.md](types.md). Some sections below (e.g. the fluent config builder,
> result metadata, transactions) describe the intended direction and may not be
> implemented yet.

---

## 1. Architecture

clickhouse-cpp owns the protocol (framing, blocks, LZ4/ZSTD compression, type system).
We only replace its **transport** through the public seam:

```cpp
Client(const ClientOptions& opts, std::unique_ptr<SocketFactory> socket_factory);
```

We implement four small classes:

| Class | Overrides |
|-------|-----------|
| `SocketFactory` | `connect(opts, endpoint)` → our `SocketBase` |
| `SocketBase`    | `makeInputStream()`, `makeOutputStream()` |
| `InputStream`   | `DoRead(buf, len)`, `Skip(bytes)` |
| `OutputStream`  | `DoWrite(data, len)`, `DoFlush()` |

`DoRead`/`DoWrite` are the bottom layer; clickhouse-cpp already wraps them in
`BufferedInput`/`BufferedOutput` (and `CompressedInput`/`CompressedOutput`). They must
return raw bytes; **no `php_stream` underneath** (that would double-buffer the hot path).

On EOF / timeout / cancellation, `DoRead`/`DoWrite` **throw** so clickhouse-cpp unwinds and
the connection is treated as dirty.

## 2. Transport (hybrid)

`SocketFactory::connect()` picks transport per connection:

- **TLS off** (common case, port 9000): the **TrueAsync IO layer**, via `ZEND_ASYNC_IO_CREATE(fd, …)`
  then `ZEND_ASYNC_IO_READ` / `ZEND_ASYNC_IO_WRITE`, awaiting the request. The reactor owns
  poll / suspend / timeout / cancel. This is the fast path: a single copy kernel → library buffer.
- **TLS on** (port 9440): a `php_stream` `tls://` socket. The low-level TrueAsync IO layer is
  plaintext only (the reactor does not run a user-space TLS stack), and "TLS out of the box"
  lives in the `php_stream` layer. The buffering overhead is negligible next to TLS crypto cost.

Async connect + DNS resolution use TrueAsync primitives (implementation detail).

## 3. Connection pool

The pool is **built into `Client`** and hidden as a mechanism: there is no separate `Pool`
class, and normal `query` / `cursor` / `insert` acquire and release a physical connection
transparently (one connection per coroutine, like the PDO pool).

It is implemented **directly on the TrueAsync async-pool ABI** (`ZEND_ASYNC_NEW_POOL` with
internal C callbacks), not via any PDO wrapper. The ABI pool pre-warms `min` connections
eagerly, grows to `max` on demand, and maintains `min` warm on release.

- **Pool size is explicit**: `->pool(min, max)` is required, no hidden defaults.
- **Acquire timeout is explicit**: when all `max` are busy, acquire waits up to the timeout
  then throws (no infinite wait).

ABI callbacks (lessons mirrored from the PDO/MySQL pool):

| Callback | Behaviour |
|----------|-----------|
| `factory` | open a new `clickhouse::Client` over our transport |
| `destructor` | close / delete the client |
| `healthcheck` | cheap native **Ping** (`Client::Ping()`); a dead idle connection is never handed out |
| `before_acquire` | optional session reset before hand-out |
| `before_release` | **dead connection never returns to the pool**: if the conn is marked `broken`, return `false` so the pool destroys it. Otherwise clear per-query/error state for a clean slate |

**Transparent "take another":** because dead connections are destroyed and the pool refills to
`min` / grows to `max`, the next acquire lands on a fresh healthy connection automatically.

**Coroutine-finish safety net:** a per-coroutine binding returns the connection to the pool if a
coroutine ends without releasing (exception / cancel). A refcount keeps the connection alive while
a `cursor()` iterator is still open.

**`$client->getPool(): Async\Pool`** returns the ABI's existing pool wrapper (lazily created and
cached) as an advanced escape hatch. It already exposes:

- stats: `count()`, `idleCount()`, `activeCount()`
- circuit breaker: `setCircuitBreakerStrategy()`, `getState()`
- lifecycle: `close()`, `isClosed()`, `activate()`, `deactivate()`, `recover()`
- manual: `acquire()`, `tryAcquire()`, `release()`

## 4. PHP API

Namespace: **`TrueAsync\ClickHouse`**.

### Configuration: fluent builder (primary), array shortcut (secondary)

```php
use TrueAsync\ClickHouse\{Config, Client, Compression};

$config = Config::builder()
    ->hosts(['ch1:9000', 'ch2:9000'])    // failover / load-balancing
    ->strategy(OpenStrategy::InOrder)    // InOrder (failover) | RoundRobin | Random
    ->database('default')
    ->credentials('default', '')
    ->compression(Compression::LZ4)      // enum: LZ4 | ZSTD | None
    ->clientName('true-async-clickhouse')
    ->settings(['max_execution_time' => 60])
    ->pool(min: 2, max: 10)              // required, no defaults
    ->acquireTimeout(5.0)               // pool-exhaustion timeout
    ->connectTimeout(3.0)
    ->recvTimeout(30.0)
    ->sendTimeout(30.0)
    ->tls(verify: true)                 // secure-by-default; + tlsCa()/tlsCert()/tlsKey()
    ->build();

$client = new Client($config);

// Array shortcut runs through the same Config:
$client = new Client(['hosts' => ['127.0.0.1:9000'], 'pool' => ['min' => 2, 'max' => 10]]);
```

Port defaults: `9000` plaintext, `9440` under TLS.

### Methods

```php
// Buffered SELECT — returns all rows at once (also handles DDL; no separate execute()).
$rows = $client->query('SELECT id, name FROM users WHERE age > {age:UInt8}', ['age' => 18]);

// Streaming SELECT — foreach-able internal iterator; coroutine yields between blocks.
foreach ($client->cursor('SELECT * FROM huge') as $row) { /* ... */ }

// Columnar batch insert (all rows at once).
$client->insert('users', ['id', 'name'], [[1, 'alice'], [2, 'bob']]);

// Streaming batch insert — bounded memory + async backpressure on flush.
$batch = $client->insertBatch('events', ['id', 'name']);
foreach ($hugeSource as $row) {
    $batch->append($row);
    if ($batch->count() >= 100_000) { $batch->flush(); }
}
$batch->flush();

// Per-query options (3rd arg): settings override + query_id + progress callback.
$client->query($sql, $params, [
    'settings'   => ['max_threads' => 4],
    'query_id'   => '...',
    'onProgress' => fn($p) => /* $p->rows, $p->bytes, $p->totalRows */ null,
]);
```

- **`query()`** materialises into an array, convenient for small results.
- **`cursor()`** streams block-by-block, giving bounded memory for analytics-sized results. It is a
  C-level internal iterator (`get_iterator` → `zend_object_iterator`), no per-row PHP method
  dispatch, not a `Generator`.
- **INSERT column types** come from the server-provided sample block of the INSERT handshake
  (client sends `INSERT INTO t (cols) VALUES`, server returns the empty typed structure), never
  inferred from PHP values and never via a separate `DESCRIBE`.

### Parameter binding

Native ClickHouse `{name:Type}`: server-side, typed, injection-safe.

### Result metadata

After a query the result exposes final statistics from the `Progress` / `ProfileInfo` packets:
`rows_read`, `bytes_read`, `rows_before_limit`. An optional `onProgress` callback in the query
options reports live progress for heavy queries.

## 5. Type mapping (ClickHouse → PHP)

| ClickHouse | PHP |
|------------|-----|
| Int8…64, UInt8…64 | `int`; values beyond `PHP_INT_MAX` → `float` (PHP-native overflow) |
| Float32/64 | `float` |
| Decimal | string (lossless; PDO precedent) |
| UUID | string |
| DateTime, DateTime64 | `DateTimeImmutable` |
| Enum8/16 | string (label) |
| String, FixedString | string |
| Bool | `bool` |
| IPv4, IPv6 | string |
| Array(T) | list array |
| Map(K,V) | associative array |
| Tuple(...) | list array |
| Nullable(T) | `null` \| value |
| LowCardinality(T) | as `T` |

## 6. Error handling

Always exceptions; base class extends an SPL exception (`RuntimeException`). C++ exceptions from
clickhouse-cpp are caught at the boundary and converted to PHP exceptions.

## 7. Query cancellation

On coroutine cancellation we send the native **`Cancel` packet** (so the server stops the heavy
query and stops burning resources), then mark the connection `broken` and close it; a
half-drained connection is never reused. The pool transparently provides a fresh one.
(Drain-and-reuse is a possible later optimisation; closing is the safe default.)

## 8. Retry & idempotency

Retry is driven by query type, leveraging ClickHouse's built-in idempotency:

- **SELECT**: read-only, always safe to auto-retry on a fresh connection.
- **INSERT**: auto-retry is safe because we set a stable **`insert_deduplication_token`** per
  `insert()` / `insertBatch()` call. Deduplication is on by default for `*MergeTree` / Replicated
  tables, so a retry after a mid-flight failure cannot double-write.
- **DDL / other**: surfaced as an error, never blindly replayed.

Retry applies to *acquiring* a connection; an already-sent query is only replayed when the rules
above make it provably safe.

## 9. Transactions

Not implemented. ClickHouse transactions are experimental (`allow_experimental_transactions`),
MergeTree-only (no Replicated), non-nestable, and cannot be committed after any exception.
`begin` / `commit` / `rollback` are omitted (or throw an explicit "unsupported" exception).

## 10. Failover

`hosts[]` + `strategy` (`InOrder` = failover, default; `RoundRobin`; `Random`). clickhouse-cpp
already supports `endpoints` with round-robin failover, so this is nearly free to wire.

---

## Implementation phases

1. Repo scaffold: `clickhouse-cpp` submodule, `config.m4` (link OpenSSL / lz4 / zstd), PHP stubs.
2. Transport: `SocketFactory` on the TrueAsync IO layer (plaintext), then `php_stream tls://` for TLS.
3. `Client` + `Config` (builder + array), `query` / `cursor`, type mapping, native binding, exceptions.
4. Hidden pool on the async-pool ABI: factory / healthcheck (Ping) / before_release, getPool, acquire timeout.
5. `insert` (server sample-block typing) and streaming `insertBatch` with backpressure.
6. Cancellation, type-driven retry + dedup token, per-query settings, result metadata, failover.
</content>
