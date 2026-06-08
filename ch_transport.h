/*
  +----------------------------------------------------------------------+
  | php-clickhouse — native async ClickHouse client for PHP TrueAsync    |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License").      |
  +----------------------------------------------------------------------+

  Transport bridge: a clickhouse-cpp SocketFactory whose read/write run over
  the TrueAsync IO layer (the reactor drives poll/suspend/timeout/cancel).

  All clickhouse-cpp <-> php.h interaction is confined to ch_transport.cpp to
  keep the C++ client headers out of the rest of the module.
*/

#ifndef CH_TRANSPORT_H
#define CH_TRANSPORT_H

#include <memory>

namespace clickhouse { class SocketFactory; }

/* Build a socket factory for clickhouse::Client. With tls, connections use a
 * php_stream ssl:// socket (verify toggles peer-certificate verification);
 * otherwise the reactor-backed plaintext path. */
std::unique_ptr<clickhouse::SocketFactory> ch_make_async_socket_factory(bool tls, bool verify);

#endif /* CH_TRANSPORT_H */
