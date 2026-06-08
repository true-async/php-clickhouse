--TEST--
Public classes, enums and exception hierarchy are registered
--EXTENSIONS--
true_async_clickhouse
--FILE--
<?php
use TrueAsync\ClickHouse\Client;
use TrueAsync\ClickHouse\Batch;
use TrueAsync\ClickHouse\Compression;
use TrueAsync\ClickHouse\OpenStrategy;
use TrueAsync\ClickHouse\ClickHouseException;
use TrueAsync\ClickHouse\ConnectionException;
use TrueAsync\ClickHouse\ServerException;
use TrueAsync\ClickHouse\ProtocolException;

var_dump(class_exists(Client::class));
var_dump(class_exists(Batch::class));
var_dump(enum_exists(Compression::class));
var_dump(enum_exists(OpenStrategy::class));

echo Compression::None->value, "\n";
echo Compression::LZ4->value, "\n";
echo Compression::ZSTD->value, "\n";
echo implode(',', array_map(fn($c) => $c->name, OpenStrategy::cases())), "\n";

var_dump(is_subclass_of(ClickHouseException::class, RuntimeException::class));
var_dump(is_subclass_of(ConnectionException::class, ClickHouseException::class));
var_dump(is_subclass_of(ServerException::class, ClickHouseException::class));
var_dump(is_subclass_of(ProtocolException::class, ClickHouseException::class));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
none
lz4
zstd
InOrder,RoundRobin,Random
bool(true)
bool(true)
bool(true)
bool(true)
