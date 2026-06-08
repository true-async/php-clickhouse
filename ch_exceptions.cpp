/*
  +----------------------------------------------------------------------+
  | php-clickhouse — native async ClickHouse client for PHP TrueAsync    |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License").      |
  +----------------------------------------------------------------------+

  C++ -> PHP exception translation. Maps clickhouse-cpp's exception hierarchy
  (and our transport's ConnectionError) onto the registered PHP classes.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
#include "php.h"
#include "Zend/zend_exceptions.h"
}

#include <memory>
#include <string>
#include <clickhouse/exceptions.h>

#include "ch_exceptions.h"

zend_class_entry *ce_ch_exception = nullptr;
zend_class_entry *ce_ch_connection_exception = nullptr;
zend_class_entry *ce_ch_server_exception = nullptr;
zend_class_entry *ce_ch_protocol_exception = nullptr;

void ch_translate_and_throw(const std::exception &e)
{
	/* A PHP exception (e.g. coroutine cancellation) is already pending — let
	 * it win rather than overwrite it. */
	if (EG(exception) != nullptr) {
		return;
	}

	/* Server-side error carries a ClickHouse error code. */
	if (const auto *se = dynamic_cast<const clickhouse::ServerException *>(&e)) {
		zend_throw_exception(ce_ch_server_exception, se->what(), se->GetCode());
		return;
	}

	/* Decode / checksum / compression failures. */
	if (dynamic_cast<const clickhouse::ProtocolError *>(&e) != nullptr
		|| dynamic_cast<const clickhouse::CompressionError *>(&e) != nullptr
		|| dynamic_cast<const clickhouse::AssertionError *>(&e) != nullptr) {
		zend_throw_exception(ce_ch_protocol_exception, e.what(), 0);
		return;
	}

	/* Transport-level network / IO failure. */
	if (dynamic_cast<const chasync::ConnectionError *>(&e) != nullptr) {
		zend_throw_exception(ce_ch_connection_exception, e.what(), 0);
		return;
	}

	/* Caller-side mistakes (bad arguments / data shape) are a LogicException,
	 * not a runtime ClickHouseException — surface PHP's standard ValueError. */
	if (dynamic_cast<const clickhouse::ValidationError *>(&e) != nullptr) {
		zend_throw_exception(zend_ce_value_error, e.what(), 0);
		return;
	}

	/* clickhouse::Error base (UnimplementedError, OpenSSLError) and any other
	 * std::exception fall back to the base. */
	zend_throw_exception(ce_ch_exception, e.what(), 0);
}
