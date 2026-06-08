# Changelog

All notable user-facing changes to php-clickhouse.

## [Unreleased]

### Added
- `TrueAsync\ClickHouse\Client` — native async ClickHouse client over the
  TrueAsync reactor, built on the official clickhouse-cpp native protocol.
- Hidden per-coroutine connection pool: concurrent coroutines each get their
  own connection; `pool['max']` configurable; `getPool()` exposes the
  `Async\Pool` (stats, circuit breaker). A connection that dies mid-operation
  is not returned to the pool.
- `query(sql, params, options)` — runs a SELECT, DDL or `INSERT … SELECT` and
  returns a `Result`. Native `{name:Type}` parameter binding (typed,
  injection-safe); per-query settings via `options['settings']`. The statement
  has executed by the time `query()` returns (no-row statements give an empty
  result).
- `Result` — single-pass query result: `fetchAll()` buffers all rows,
  `foreach`/`fetch()` streams them block by block (bounded memory), `fetchOne()`
  reads a scalar. `summary()` returns a `Summary` of server statistics
  (`readRows`, `writtenRows`, `rowsBeforeLimit`, `elapsed`, …) and
  `affectedRows()` is a shortcut for `INSERT … SELECT` written rows.
- `insert(table, columns, rows)` — columnar batch insert; column types come
  from the server. Supports integers, floats, strings, Bool, Date/DateTime/
  DateTime64 (int/float timestamp or `DateTimeInterface`, sub-second preserved),
  UUID, IPv4, IPv6, Decimal, Enum, Nullable, Array (nested), Tuple and Map.
- `insertBatch(table, columns)` — streaming batch insert returning a `Batch`:
  `append()` rows, then `flush()` to send them as one insert. Flushing applies
  async backpressure (a full socket buffer yields the coroutine), so the
  producer cannot outrun the server; `count()` reports buffered rows.
- Full read-side type mapping: integers (UInt64 past `PHP_INT_MAX` → float;
  Int128/UInt128 → string), floats, String, Bool, UUID/IPv4/IPv6/Enum/Decimal
  → string, Date/DateTime/DateTime64 → `DateTimeImmutable`,
  Array/Nullable/Tuple/Map, and `LowCardinality(String)` (incl. nullable).
- `'hosts' => [...]` multi-host config with an `'open_strategy'` (`OpenStrategy`
  enum): `InOrder` (failover, default), `RoundRobin` or `Random` spread a pool's
  connections across the hosts.
- `'tls' => true` connects over TLS (php_stream `ssl://`); `'tls_verify'`
  toggles certificate verification; the port defaults to 9440.
- LZ4 / ZSTD wire compression via the `'compression'` option, a `Compression`
  enum (`Compression::None` / `LZ4` / `ZSTD`).
- Exceptions: `ClickHouseException` (base) with `ConnectionException`,
  `ServerException` (carries the server error code) and `ProtocolException`.
  Caller mistakes (bad data shape) raise PHP's standard `\ValueError`.
