--TEST--
A query against a dead port throws ConnectionException
--EXTENSIONS--
true_async_clickhouse
--FILE--
<?php
use TrueAsync\ClickHouse\Client;
use TrueAsync\ClickHouse\ConnectionException;
use function Async\spawn;
use function Async\await;

// The pool connects lazily on first use, so the failure surfaces on query().
$result = await(spawn(function () {
    try {
        $client = new Client(['host' => '127.0.0.1', 'port' => 1]);
        $client->query("SELECT 1");
        return "no exception";
    } catch (ConnectionException $e) {
        return "ConnectionException";
    } catch (\Throwable $e) {
        return "other: " . get_class($e) . ": " . $e->getMessage();
    }
}));

echo $result, "\n";
?>
--EXPECT--
ConnectionException
