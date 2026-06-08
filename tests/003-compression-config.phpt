--TEST--
Client config: 'compression' must be a Compression enum (lazy, no server)
--EXTENSIONS--
true_async_clickhouse
--FILE--
<?php
use TrueAsync\ClickHouse\Client;
use TrueAsync\ClickHouse\Compression;

// Construction is lazy (no connection opens), so this exercises config parsing
// only — no ClickHouse server is required.

// Every Compression enum case is accepted.
foreach ([Compression::None, Compression::LZ4, Compression::ZSTD] as $c) {
    new Client(['compression' => $c]);
    echo "ok: ", $c->value, "\n";
}

// Omitting the key is fine (defaults to LZ4).
new Client([]);
echo "ok: default\n";

// A bare string is rejected: the option is strictly typed.
try {
    new Client(['compression' => 'lz4']);
    echo "no error\n";
} catch (\TypeError $e) {
    echo "TypeError\n";
}
?>
--EXPECT--
ok: none
ok: lz4
ok: zstd
ok: default
TypeError
