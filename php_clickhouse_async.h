/*
  +----------------------------------------------------------------------+
  | php-clickhouse — native async ClickHouse client for PHP TrueAsync    |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License").      |
  +----------------------------------------------------------------------+
  | Author: Edmond                                                       |
  +----------------------------------------------------------------------+
*/

#ifndef PHP_CLICKHOUSE_ASYNC_H
#define PHP_CLICKHOUSE_ASYNC_H

/* This translation unit is C++, but the engine references the module entry as
 * a C symbol (e.g. from the generated internal_functions table of a static
 * build). Give it C linkage so the mangled C++ name does not cause an
 * unresolved external at link time. Harmless for the shared build. */
#ifdef __cplusplus
extern "C" {
#endif

extern zend_module_entry clickhouse_async_module_entry;

#ifdef __cplusplus
}
#endif

#define phpext_clickhouse_async_ptr &clickhouse_async_module_entry

#define PHP_CLICKHOUSE_ASYNC_VERSION "0.1.0-dev"

#ifdef ZTS
#include "TSRM.h"
#endif

#endif /* PHP_CLICKHOUSE_ASYNC_H */
