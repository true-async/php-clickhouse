<div align="center">

# php-clickhouse

**Native asynchronous ClickHouse client for [PHP TrueAsync](https://github.com/true-async).**
*Write sync, run async.*

[![CI](https://github.com/true-async/php-clickhouse/actions/workflows/ci.yml/badge.svg)](https://github.com/true-async/php-clickhouse/actions/workflows/ci.yml)
[![PHP](https://img.shields.io/badge/PHP-8.x_ZTS-777BB4?style=flat-square&logo=php&logoColor=white)](https://www.php.net/)
[![ClickHouse](https://img.shields.io/badge/ClickHouse-native_protocol-FFCC00?style=flat-square&logo=clickhouse&logoColor=black)](https://clickhouse.com/)
[![License](https://img.shields.io/badge/license-Apache_2.0-blue?style=flat-square)](LICENSE)

</div>

Built on the official [`ClickHouse/clickhouse-cpp`](https://github.com/ClickHouse/clickhouse-cpp)
native-protocol library. Every network call looks synchronous but transparently
yields the current coroutine while it waits on the socket. One `Client` serves
many concurrent coroutines through a hidden per-coroutine connection pool.

```php
use TrueAsync\ClickHouse\Client;
use function Async\spawn;
use function Async\await_all;

$client = new Client(['host' => '127.0.0.1', 'user' => 'default']);

// Two coroutines query at the same time; each transparently borrows its own
// connection from the hidden pool, so the work overlaps on the wire.
[$results] = await_all([
    spawn(fn() => $client->query("SELECT count() AS c FROM events")->fetchOne()),
    spawn(fn() => $client->query(
        "SELECT name, count() AS c FROM events WHERE day = {d:Date} GROUP BY name",
        ['d' => '2026-06-07']
    )->fetchAll()),
]);
```

## Features

- **Async over the TrueAsync reactor:** non-blocking reads/writes; the
  coroutine yields instead of blocking the thread.
- **Native protocol** with **LZ4 / ZSTD** compression.
- **Hidden per-coroutine pool:** concurrent queries each get their own
  connection; dead connections are dropped and replaced automatically.
- **`query()` → `Result`:** buffer with `fetchAll()`, stream with `foreach`
  (lazy, block by block), or read a scalar with `fetchOne()`; carries server
  statistics (`summary()`, `affectedRows()`).
- **`insert()`:** columnar batch insert; **`insertBatch()`:** streaming insert
  with built-in backpressure.
- **Native `{name:Type}` parameter binding:** typed and injection-safe.
- **Rich type mapping:** integers, floats, `Bool`, `String`, `UUID`,
  `IPv4/IPv6`, `Decimal`, `Enum`, `Date*`/`DateTime*` → `DateTimeImmutable`,
  `Array`/`Tuple`/`Map`/`Nullable`, `Int128`, `LowCardinality(String)`.
- **Multi-host failover** and **TLS** (`ssl://`).
- **Typed exceptions:** `ConnectionException`, `ServerException` (with the
  server error code), `ProtocolException`; caller mistakes raise `\ValueError`.

## Requirements

- PHP 8.x built with **ZTS** and the **TrueAsync** runtime.
- A **C++17** compiler and **CMake** (to build the bundled clickhouse-cpp).

## Install

```sh
git clone --recurse-submodules https://github.com/true-async/php-clickhouse.git
cd php-clickhouse
# build the bundled clickhouse-cpp, then phpize && ./configure && make
```

Full steps: **[docs/installation.md](docs/installation.md)**.

## Documentation

- [Installation](docs/installation.md)
- [Configuration](docs/configuration.md): connection, auth, compression, pool, failover, TLS
- [Usage](docs/usage.md): query, the Result object, insert, insertBatch, errors
- [Type mapping](docs/types.md): ClickHouse ↔ PHP
- [Connection pool](docs/pool.md)
- [Architecture](docs/architecture.md): internal design

See the [`tests/`](tests) directory for runnable examples of every feature.

## License

Apache-2.0.
