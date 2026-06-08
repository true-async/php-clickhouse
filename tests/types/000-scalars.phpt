--TEST--
Type mapping: UUID, IPv4/IPv6, Decimal, Bool
--EXTENSIONS--
true_async_clickhouse
--SKIPIF--
<?php
require __DIR__ . '/../inc/clickhouse.inc';
clickhouse_skip_if_no_server();
?>
--FILE--
<?php
require __DIR__ . '/../inc/clickhouse.inc';

use TrueAsync\ClickHouse\Client;
use function Async\spawn;
use function Async\await;

$cfg = clickhouse_test_config();

$row = await(spawn(function () use ($cfg) {
    $client = new Client($cfg);
    $sql = "SELECT"
        . " toUUID('61f0c404-5cb3-11e7-907b-a6006ad3dba0') AS uuid,"
        . " toIPv4('1.2.3.4') AS ip4,"
        . " toIPv6('2001:db8::1') AS ip6,"
        . " toDecimal64(1234.5678, 4) AS dec,"
        . " CAST(true AS Bool) AS b";
    return $client->query($sql)->fetchAll()[0];
}));

var_dump($row);
?>
--EXPECT--
array(5) {
  ["uuid"]=>
  string(36) "61f0c404-5cb3-11e7-907b-a6006ad3dba0"
  ["ip4"]=>
  string(7) "1.2.3.4"
  ["ip6"]=>
  string(11) "2001:db8::1"
  ["dec"]=>
  string(9) "1234.5678"
  ["b"]=>
  bool(true)
}
