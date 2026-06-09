<?php

/* CI smoke check: the statically-built clickhouse_async extension loads and
 * registers its public API. No ClickHouse server required. Kept as a file
 * (not an inline php -r) so cmd does not mangle the '!' and quotes. */

if (!extension_loaded('true_async_clickhouse')) {
    fwrite(STDERR, 'FAIL: true_async_clickhouse not loaded' . PHP_EOL);
    exit(1);
}

foreach (['TrueAsync\\ClickHouse\\Client', 'TrueAsync\\ClickHouse\\Result'] as $class) {
    if (!class_exists($class)) {
        fwrite(STDERR, 'FAIL: ' . $class . ' missing' . PHP_EOL);
        exit(1);
    }
}

echo 'OK: true_async_clickhouse loaded; Client and Result present' . PHP_EOL;
