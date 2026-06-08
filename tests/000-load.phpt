--TEST--
Extension loads and reports its version
--EXTENSIONS--
true_async_clickhouse
--FILE--
<?php
var_dump(extension_loaded('true_async_clickhouse'));
var_dump(phpversion('true_async_clickhouse'));
?>
--EXPECT--
bool(true)
string(9) "0.1.0-dev"
