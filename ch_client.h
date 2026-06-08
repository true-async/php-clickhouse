/*
  +----------------------------------------------------------------------+
  | php-clickhouse — native async ClickHouse client for PHP TrueAsync    |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License").      |
  +----------------------------------------------------------------------+

  C facade over a clickhouse::Client connection. Keeps the C++ client headers
  out of the main module TU. Includers must include php.h before this header.
*/

#ifndef CH_CLIENT_H
#define CH_CLIENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ch_client ch_client_t;

typedef struct {
	const char *host;
	uint16_t    port;
} ch_endpoint_t;

typedef struct {
	const char *host;
	uint16_t    port;
	const char *database;
	const char *user;
	const char *password;
	int         compression;  /* 0 none, 1 lz4, 2 zstd */
	bool        tls;          /* connect over TLS (php_stream ssl://) */
	bool        tls_verify;   /* verify the server certificate */
	/* Additional failover endpoints tried after host:port, in order. */
	const ch_endpoint_t *endpoints;
	size_t               endpoint_count;
} ch_client_config_t;

/*
 * Open a connection. On failure the matching PHP exception is thrown
 * (ConnectionException for network/connect failures, ServerException when the
 * server rejects the handshake) and NULL is returned.
 */
ch_client_t *ch_client_connect(const ch_client_config_t *cfg);

void ch_client_free(ch_client_t *client);

/* True if the connection died mid-operation and must not be reused. */
bool ch_client_is_broken(ch_client_t *client);

/*
 * Columnar batch insert. `columns` is a list of column-name strings, `rows` a
 * list of positional row arrays. Column types come from the server INSERT
 * sample block. Must run inside a coroutine. On error throws and returns false.
 */
bool ch_client_insert(ch_client_t *client, const char *table, HashTable *columns, HashTable *rows);

/* Server-reported statistics, accumulated from progress/profile packets. Final
 * once the result is fully consumed. */
typedef struct {
	uint64_t read_rows;
	uint64_t read_bytes;
	uint64_t written_rows;
	uint64_t written_bytes;
	uint64_t total_rows_to_read;
	uint64_t rows_before_limit;
	bool     has_rows_before_limit;
	double   elapsed;            /* wall-clock seconds */
} ch_summary_t;

/*
 * Start a query and return an opaque Result handle. Runs BeginExecute and primes
 * the first row block, so the statement has executed by the time this returns
 * (DDL / INSERT … SELECT take effect even if the result is never read) and
 * server errors surface here. Takes ownership of `conn_zv`: a no-row statement
 * releases it immediately; a streaming result holds it until the end (or free).
 * `params` and `settings` may be NULL. Returns NULL on failure (the handle
 * releases conn_zv itself before returning).
 */
void *ch_result_start(ch_client_t *client, zend_async_pool_t *pool, zval *conn_zv,
		const char *sql, HashTable *params, HashTable *settings);

/* Pull the next row (assoc array column => value) into `out`, fetching the next
 * block when the current one drains. Returns false at end of stream; throws on a
 * server/connection error. */
bool ch_result_next(void *result, zval *out);

/* Copy the statistics accumulated so far. */
void ch_result_get_summary(void *result, ch_summary_t *out);

/* Release the result: if abandoned mid-stream the connection is dropped (dirty),
 * otherwise it returns to the pool. */
void ch_result_free(void *result);

/*
 * Streaming batch insert. Holds the pooled connection (conn_zv) for the batch's
 * lifetime; each flush is a self-contained insert of the buffered rows, so the
 * connection is left idle (never mid-session) between flushes. Returns an opaque
 * handle. `columns` is a list of column-name strings.
 */
void *ch_batch_start(ch_client_t *client, zend_async_pool_t *pool, zval *conn_zv,
		const char *table, HashTable *columns);

/* Buffer one row (a positional array of cell values). Validates the shape and
 * throws \ValueError on a mismatch (returns false). No network IO. */
bool ch_batch_append(void *batch, zval *row);

/* Send all buffered rows as one insert (BeginInsert/SendInsertBlock/EndInsert);
 * the block write applies async backpressure. A no-op when nothing is buffered.
 * On error throws and returns false. Must run inside a coroutine. */
bool ch_batch_flush(void *batch);

/* Number of buffered rows not yet flushed. */
uint64_t ch_batch_count(void *batch);

/* Release the batch: drops any unflushed rows and returns the connection to the
 * pool (or drops it if a flush left it broken). No network IO. */
void ch_batch_free(void *batch);

#ifdef __cplusplus
}
#endif

#endif /* CH_CLIENT_H */
