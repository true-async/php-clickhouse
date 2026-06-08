# Configuration

A `Client` is constructed from an options array:

```php
use TrueAsync\ClickHouse\Client;
use TrueAsync\ClickHouse\Compression;

$client = new Client([
    'host'        => '127.0.0.1',
    'port'        => 9000,
    'database'    => 'default',
    'user'        => 'default',
    'password'    => '',
    'compression' => Compression::LZ4,
    'pool'        => ['max' => 10],
]);
```

## Options

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `host` | string | `127.0.0.1` | Server host. |
| `port` | int | `9000` (or `9440` when `tls` is on) | Native-protocol port. |
| `database` | string | `default` | Default database. |
| `user` | string | `default` | Username. |
| `password` | string | `''` | Password. |
| `compression` | `Compression` | `Compression::LZ4` | Wire compression enum: `Compression::None`, `::LZ4` or `::ZSTD`. |
| `pool` | array | `['max' => 10]` | Connection pool settings; see below. |
| `hosts` | string[] | none | Multi-host list; see below. |
| `open_strategy` | `OpenStrategy` | `OpenStrategy::InOrder` | How a multi-host pool picks each connection's host: `InOrder` (failover), `RoundRobin`, `Random`. |
| `tls` | bool | `false` | Connect over TLS. |
| `tls_verify` | bool | `true` | Verify the server certificate (TLS only). |

> The construction is lazy: no connection is opened until the first
> `query`/`insert`. A bad host therefore surfaces as a
> `ConnectionException` on first use, not in the constructor.

## Connection pool

`'pool' => ['max' => N]` caps the number of physical connections a single
`Client` opens (default `10`). Concurrent coroutines each acquire their own
connection up to this limit; further acquirers wait for one to free up. See
[pool.md](pool.md).

## Multi-host

`'hosts'` lists several servers (each entry may carry its own port). When given,
it supersedes `host`/`port` for the primary. `'open_strategy'` decides which host
each pooled connection tries first:

- `OpenStrategy::InOrder` (default) — always start at the first host; the rest
  are pure failover, tried in order only when an earlier one fails to connect.
- `OpenStrategy::RoundRobin` — rotate the starting host across connections, so a
  pool spreads its connections over the hosts (each still fails over to the others).
- `OpenStrategy::Random` — pick a random starting host per connection.

```php
use TrueAsync\ClickHouse\OpenStrategy;

$client = new Client([
    'hosts'         => ['ch-1.internal', 'ch-2.internal:9000', 'ch-3.internal'],
    'open_strategy' => OpenStrategy::RoundRobin,
    'user'          => 'default',
]);
```

Strategy is per *connection*, not per query: ClickHouse pins a connection to one
host for its lifetime, so balancing happens at the granularity of the hidden
pool's connections.

## TLS

```php
$client = new Client([
    'host'       => 'clickhouse.example.com',
    'tls'        => true,     // port defaults to 9440
    'tls_verify' => true,     // set false only for self-signed test certs
    'user'       => 'default',
    'password'   => 'secret',
]);
```

TLS is provided through PHP's stream layer (`ssl://`); the server must expose a
secure native-protocol port (9440 by convention).
