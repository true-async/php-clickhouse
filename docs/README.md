# php-clickhouse documentation

Native asynchronous ClickHouse client for **PHP TrueAsync**, built on the
official [`ClickHouse/clickhouse-cpp`](https://github.com/ClickHouse/clickhouse-cpp)
native-protocol library.

## Contents

- **[Installation](installation.md)**: requirements, building clickhouse-cpp,
  building and loading the extension.
- **[Configuration](configuration.md)**: the `Client` options array: host,
  auth, compression, pool, failover, TLS.
- **[Usage](usage.md)**: querying, the Result object (buffer/stream/summary),
  parameters, settings, inserts, streaming batch inserts, and error handling.
- **[Type mapping](types.md)**: how ClickHouse types map to PHP on read and
  what each accepts on insert.
- **[Connection pool](pool.md)**: the hidden per-coroutine pool, sizing,
  failure handling and `getPool()`.
- **[Architecture](architecture.md)**: internal design covering the transport
  seam, the reactor bridge, and how the pool is built on the TrueAsync engine ABI.

## At a glance

```php
use TrueAsync\ClickHouse\Client;
use function Async\spawn;
use function Async\await_all;

$client = new Client(['host' => '127.0.0.1', 'user' => 'default']);

// One Client, many coroutines: these two queries run at the same time, each
// on its own pooled connection.
[$results] = await_all([
    spawn(fn() => $client->query("SELECT count() AS c FROM hits")->fetchOne()),
    spawn(fn() => $client->query(
        "SELECT url, count() AS c FROM hits WHERE ts >= {since:DateTime} GROUP BY url",
        ['since' => '2026-01-01 00:00:00']
    )->fetchAll()),
]);
```

Every call looks synchronous but yields the coroutine while waiting on the
network: "write sync, run async". All client calls must run inside a coroutine
(`Async\spawn`).
