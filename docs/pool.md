# Connection pool

Each `Client` owns a **hidden, per-coroutine connection pool**. You never create
or manage it directly. `query`, `insert` and `insertBatch` acquire a physical
connection when they start and release it when they finish (a streamed `query()`
result holds its connection until it is fully read).

## Why per-coroutine

Concurrent coroutines must not share one socket; their reads and writes would
interleave on the wire. So each coroutine that runs a query gets its own
connection from the pool, and several queries can be in flight at once:

```php
use function Async\spawn;
use function Async\await_all;

$client = new Client(['host' => '127.0.0.1']);

// These three run concurrently, each on its own pooled connection.
[$results] = await_all([
    spawn(fn() => $client->query("SELECT sleep(1), 1 AS n")->fetchAll()),
    spawn(fn() => $client->query("SELECT sleep(1), 2 AS n")->fetchAll()),
    spawn(fn() => $client->query("SELECT sleep(1), 3 AS n")->fetchAll()),
]);
```

## Sizing

`'pool' => ['max' => N]` (default `10`) caps the number of physical connections.
When all `max` are busy, a further acquirer waits until one is released.
Connections are created lazily, on demand.

## Failure handling

A connection that dies mid-operation (peer reset, timeout, protocol error) is
marked broken and **destroyed instead of being returned to the pool**. The next
acquire transparently gets a fresh, healthy connection, so a single dropped
connection does not surface twice.

## `getPool(): \Async\Pool`

For advanced needs, `getPool()` returns the underlying TrueAsync `Async\Pool`
wrapper:

```php
$pool = $client->getPool();

$pool->count();        // total connections
$pool->idleCount();    // idle (available)
$pool->activeCount();  // in use

$pool->getState();     // circuit-breaker state
$pool->close();        // close the pool
```

This is an escape hatch; for normal use you never touch it. The exact surface
is the standard TrueAsync pool wrapper (stats, circuit breaker, lifecycle,
manual `acquire`/`tryAcquire`/`release`).

The wrapper is valid only while the owning `Client` is alive: destroying the
`Client` closes the pool, so a wrapper kept past that point refers to a closed
pool.
