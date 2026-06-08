<?php

/* Shared helpers for true_async_clickhouse tests. */

/* Connection config, overridable from the environment so CI can point the
 * suite at a service container. */
function clickhouse_test_config(): array
{
    return [
        'host'        => getenv('CLICKHOUSE_HOST') ?: '127.0.0.1',
        'port'        => (int) (getenv('CLICKHOUSE_PORT') ?: 9000),
        'user'        => getenv('CLICKHOUSE_USER') ?: 'default',
        'password'    => getenv('CLICKHOUSE_PASSWORD') ?: '',
        'database'    => getenv('CLICKHOUSE_DATABASE') ?: 'default',
        // Default tests to uncompressed; query/003 overrides to exercise LZ4/ZSTD.
        'compression' => \TrueAsync\ClickHouse\Compression::None,
    ];
}

/* Use in --SKIPIF-- to gate server-dependent tests. This is an environment
 * gate, not a test smoother: tests skip only when no ClickHouse is reachable. */
function clickhouse_skip_if_no_server(): void
{
    $cfg = clickhouse_test_config();
    $fp = @fsockopen($cfg['host'], $cfg['port'], $errno, $errstr, 1.0);

    if ($fp === false) {
        die("skip ClickHouse not reachable at {$cfg['host']}:{$cfg['port']}");
    }

    fclose($fp);
}
