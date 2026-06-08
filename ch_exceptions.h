/*
  +----------------------------------------------------------------------+
  | php-clickhouse — native async ClickHouse client for PHP TrueAsync    |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License").      |
  +----------------------------------------------------------------------+

  PHP exception hierarchy and the C++ -> PHP exception translator. Includers
  must include php.h before this header.
*/

#ifndef CH_EXCEPTIONS_H
#define CH_EXCEPTIONS_H

#include <exception>
#include <stdexcept>

/* php.h is included by every TU before this header. */
typedef struct _zend_class_entry zend_class_entry;

namespace chasync {

/* Connection / network / IO failure raised by the transport layer. Kept
 * separate from clickhouse::Error so the translator can map it to
 * ConnectionException. */
class ConnectionError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

} // namespace chasync

/* Registered in MINIT, shared across translation units. */
extern zend_class_entry *ce_ch_exception;             /* ClickHouseException (base) */
extern zend_class_entry *ce_ch_connection_exception;  /* ConnectionException */
extern zend_class_entry *ce_ch_server_exception;      /* ServerException */
extern zend_class_entry *ce_ch_protocol_exception;    /* ProtocolException */

/*
 * Translate a C++ exception to the matching PHP exception and throw it. Use at
 * every PHP <-> C++ method boundary that calls into clickhouse-cpp. No-op when
 * a PHP exception is already pending (e.g. coroutine cancellation wins).
 */
void ch_translate_and_throw(const std::exception &e);

#endif /* CH_EXCEPTIONS_H */
