--TEST--
phpinfo() reports the extension's module info block
--EXTENSIONS--
true_async_clickhouse
--FILE--
<?php

ob_start();
phpinfo(INFO_MODULES);
$info = ob_get_clean();

echo "support: ", (str_contains($info, 'true_async_clickhouse support') ? 'yes' : 'no'), "\n";
echo "version: ", (str_contains($info, phpversion('true_async_clickhouse')) ? 'yes' : 'no'), "\n";
echo "clickhouse-cpp: ", (str_contains($info, 'clickhouse-cpp') ? 'yes' : 'no'), "\n";
?>
--EXPECT--
support: yes
version: yes
clickhouse-cpp: yes
