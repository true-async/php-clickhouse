# Tests

Standard PHP `.phpt` tests, run with the PHP test runner:

```sh
make test
# or target a subset:
TEST_PHP_EXECUTABLE=$(which php) php run-tests.php -P tests/
```

## How they work

Each `.phpt` has a `--TEST--` title, an optional `--SKIPIF--` gate, a `--FILE--`
body and an `--EXPECT--`/`--EXPECTF--` block. The runner executes the body and
diffs its output against the expectation.

`--EXTENSIONS--true_async_clickhouse` makes the runner load the built module.

## Layout

| Path | Needs a server? | Covers |
|------|-----------------|--------|
| `000-load.phpt`, `001-classes.phpt`, `004-phpinfo.phpt` | no | module loads; classes/enums/exception hierarchy; phpinfo block |
| `002-*` config validation | no | builder/array parsing and validation errors |
| `010-connect-refused.phpt` | no | `ConnectionException` against a dead port |
| `connection/` | yes | connect, auth, reconnect |
| `query/` | yes | `query()`, streaming `cursor()`, parameter binding |
| `insert/` | yes | `insert()`, streaming `insertBatch()` |
| `types/` | yes | ClickHouse → PHP type mapping |
| `errors/` | yes | `ServerException` code, protocol errors |
| `async/` | yes | hidden pool, concurrent coroutines, pool-exhaustion timeout |

## Server-dependent tests

Server tests gate themselves with `--SKIPIF--` using
`tests/inc/clickhouse.inc`:

```php
--SKIPIF--
<?php
require __DIR__ . '/inc/clickhouse.inc';
clickhouse_skip_if_no_server();
?>
```

Connection config comes from the environment (`CLICKHOUSE_HOST`,
`CLICKHOUSE_PORT`, `CLICKHOUSE_USER`, `CLICKHOUSE_PASSWORD`,
`CLICKHOUSE_DATABASE`), defaulting to `127.0.0.1:9000`. Skipping is an
environment gate only — it never masks a real failure.

CI runs the full suite against a `clickhouse-server` service container.
