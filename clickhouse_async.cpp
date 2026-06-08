/*
  +----------------------------------------------------------------------+
  | php-clickhouse — native async ClickHouse client for PHP TrueAsync    |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License").      |
  +----------------------------------------------------------------------+
  | Author: Edmond                                                       |
  +----------------------------------------------------------------------+

  Module entry: registers the public classes, the result/batch/client objects
  and the exception hierarchy. Method bodies live across the ch_*.cpp layers.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
#include "php.h"
#include "ext/standard/info.h"
#include "ext/spl/spl_exceptions.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_enum.h"
#include "Zend/zend_async_API.h"
#include "Zend/zend_interfaces.h"
#include "ext/random/php_random.h"
}

#include "php_clickhouse_async.h"
#include "clickhouse_async_arginfo.h"
#include "ch_transport.h"
#include "ch_exceptions.h"
#include "ch_client.h"

/* Non-exception class entries, resolved in MINIT. The exception class entries
 * (ce_ch_*) live in ch_exceptions.cpp and are shared across TUs. */
static zend_class_entry *ce_clickhouse_compression;
static zend_class_entry *ce_clickhouse_open_strategy;
static zend_class_entry *ce_clickhouse_batch;
static zend_class_entry *ce_clickhouse_summary;
static zend_class_entry *ce_clickhouse_client;

/* --- Client object: owns the config and a hidden connection pool -------- */

typedef struct {
	char              *host;        /* owned */
	char              *database;    /* owned */
	char              *user;        /* owned */
	char              *password;    /* owned */
	uint16_t           port;
	int                compression;
	bool               tls;
	bool               tls_verify;
	uint32_t           pool_max;
	int                open_strategy;  /* 0 in_order, 1 round_robin, 2 random */
	uint32_t           conn_seq;       /* cursor for round-robin endpoint rotation */
	ch_endpoint_t     *endpoints;     /* owned host strings; failover after host:port */
	size_t             endpoint_count;
	zend_async_pool_t *pool;          /* lazily created on first use */
	zend_object       *pool_wrapper;  /* lazily created Async\Pool wrapper */
	zend_object        std;
} ch_client_object;

static zend_object_handlers ch_client_object_handlers;

static inline ch_client_object *ch_client_object_from(zend_object *obj)
{
	return (ch_client_object *) ((char *) obj - offsetof(ch_client_object, std));
}

#define Z_CH_CLIENT_OBJ_P(zv) ch_client_object_from(Z_OBJ_P(zv))

/* Pool factory: open a new connection from the owning Client's config. Runs in
 * coroutine context (called from acquire), so the handshake can suspend. */
static bool ch_pool_factory(zend_async_pool_t *pool, zval *result)
{
	ch_client_object *obj = (ch_client_object *) pool->user_data;

	/* Endpoint index 0 is the primary (host:port); 1.. are the failover hosts.
	 * The open strategy decides which one this connection tries first; rotating
	 * the start across connections spreads them over the hosts (round_robin /
	 * random), while in_order always starts at the primary (failover only). */
	const size_t total = 1 + obj->endpoint_count;
	size_t start = 0;

	if (total > 1) {
		if (obj->open_strategy == 1) {
			start = obj->conn_seq++ % total;
		} else if (obj->open_strategy == 2) {
			start = (size_t) php_mt_rand_range(0, (zend_long) total - 1);
		}
	}

	/* The remaining hosts become this connection's failover list, in order
	 * after the chosen start (wrapping around). ch_client_connect copies them, so
	 * the temporary array is freed right after. */
	ch_endpoint_t *failover = NULL;

	if (total > 1) {
		failover = (ch_endpoint_t *) ecalloc(total - 1, sizeof(ch_endpoint_t));

		for (size_t k = 1; k < total; ++k) {
			const size_t idx = (start + k) % total;
			failover[k - 1].host = (idx == 0) ? obj->host : obj->endpoints[idx - 1].host;
			failover[k - 1].port = (idx == 0) ? obj->port : obj->endpoints[idx - 1].port;
		}
	}

	ch_client_config_t cfg;
	cfg.host = (start == 0) ? obj->host : obj->endpoints[start - 1].host;
	cfg.port = (start == 0) ? obj->port : obj->endpoints[start - 1].port;
	cfg.database = obj->database;
	cfg.user = obj->user;
	cfg.password = obj->password;
	cfg.compression = obj->compression;
	cfg.tls = obj->tls;
	cfg.tls_verify = obj->tls_verify;
	cfg.endpoints = failover;
	cfg.endpoint_count = total - 1;

	ch_client_t *conn = ch_client_connect(&cfg);

	if (failover != NULL) {
		efree(failover);
	}

	if (conn == NULL) {
		return false; /* PHP exception already thrown */
	}

	ZVAL_PTR(result, conn);
	return true;
}

static void ch_pool_destructor(zend_async_pool_t *pool, zval *resource)
{
	ch_client_t *conn = (ch_client_t *) Z_PTR_P(resource);

	if (conn != NULL) {
		ch_client_free(conn);
	}
}

/* healthcheck / before_acquire — accept-all for now. */
static bool ch_pool_accept(zend_async_pool_t *pool, zval *resource)
{
	return true;
}

/* before_release: a connection that died mid-operation must not be reused —
 * returning false makes the pool destroy it instead of returning it to idle. */
static bool ch_pool_before_release(zend_async_pool_t *pool, zval *resource)
{
	return !ch_client_is_broken((ch_client_t *) Z_PTR_P(resource));
}

/* Lazily create the pool. min=0 (no pre-warm): the ABI pre-warms inside the
 * constructor, before user_data is set, so the factory cannot run there yet —
 * connections are created on demand instead (in coroutine context). */
static bool ch_pool_ensure(ch_client_object *obj)
{
	if (obj->pool != NULL) {
		return true;
	}

	obj->pool = ZEND_ASYNC_NEW_POOL(
		ch_pool_factory, ch_pool_destructor,
		ch_pool_accept, ch_pool_accept, ch_pool_before_release,
		0, obj->pool_max, 0);

	if (obj->pool == NULL) {
		if (EG(exception) == NULL) {
			zend_throw_exception(ce_ch_connection_exception, "failed to create connection pool", 0);
		}

		return false;
	}

	obj->pool->user_data = obj;
	return true;
}

/* Acquire a pooled connection for the current operation. Returns NULL on
 * failure with a PHP exception pending. */
static ch_client_t *ch_pool_acquire(ch_client_object *obj, zval *conn_zv)
{
	if (!ch_pool_ensure(obj)) {
		return NULL;
	}

	/* ACQUIRE returns false (and sets the exception) on failure; on success it
	 * fills conn_zv. Don't also gate on EG(exception) — that would drop an
	 * acquired connection without releasing it. */
	if (!ZEND_ASYNC_POOL_ACQUIRE(obj->pool, conn_zv, 0)) {
		return NULL;
	}

	return (ch_client_t *) Z_PTR_P(conn_zv);
}

static zend_object *ch_client_object_create(zend_class_entry *ce)
{
	ch_client_object *obj =
		(ch_client_object *) zend_object_alloc(sizeof(ch_client_object), ce);

	zend_object_std_init(&obj->std, ce);
	object_properties_init(&obj->std, ce);
	obj->std.handlers = &ch_client_object_handlers;

	obj->host = NULL;
	obj->database = NULL;
	obj->user = NULL;
	obj->password = NULL;
	obj->port = 0;
	obj->compression = 0;
	obj->tls = false;
	obj->tls_verify = true;
	obj->pool_max = 10;
	obj->open_strategy = 0;
	obj->conn_seq = 0;
	obj->endpoints = NULL;
	obj->endpoint_count = 0;
	obj->pool = NULL;
	obj->pool_wrapper = NULL;

	return &obj->std;
}

static void ch_client_object_free(zend_object *object)
{
	ch_client_object *obj = ch_client_object_from(object);

	/* Release the getPool() wrapper before the pool it points at. */
	if (obj->pool_wrapper != NULL) {
		OBJ_RELEASE(obj->pool_wrapper);
		obj->pool_wrapper = NULL;
	}

	if (obj->pool != NULL) {
		ZEND_ASYNC_POOL_CLOSE(obj->pool);
		ZEND_ASYNC_EVENT_RELEASE(&obj->pool->event);
		obj->pool = NULL;
	}

	if (obj->host != NULL) {
		efree(obj->host);
	}

	if (obj->database != NULL) {
		efree(obj->database);
	}

	if (obj->user != NULL) {
		efree(obj->user);
	}

	if (obj->password != NULL) {
		efree(obj->password);
	}

	if (obj->endpoints != NULL) {
		for (size_t i = 0; i < obj->endpoint_count; ++i) {
			efree((void *) obj->endpoints[i].host);
		}

		efree(obj->endpoints);
	}

	zend_object_std_dtor(&obj->std);
}

/* Copy an optional string config key into an owned buffer (efree'd on free). */
static char *ch_config_dup(HashTable *ht, const char *key, size_t key_len, const char *fallback)
{
	zval *v = zend_hash_str_find(ht, key, key_len);

	if (v != NULL && Z_TYPE_P(v) == IS_STRING) {
		return estrndup(Z_STRVAL_P(v), Z_STRLEN_P(v));
	}

	return estrdup(fallback);
}

/* Parse "host" or "host:port" into an owned host string and a port. */
static void ch_parse_host_port(const char *str, size_t len, char **host, uint16_t *port,
	uint16_t default_port)
{
	const char *colon = NULL;

	for (size_t i = len; i > 0; --i) {
		if (str[i - 1] == ':') {
			colon = str + i - 1;
			break;
		}
	}

	if (colon != NULL) {
		*host = estrndup(str, colon - str);
		long parsed = strtol(colon + 1, NULL, 10);
		*port = (parsed > 0 && parsed <= 65535) ? (uint16_t) parsed : default_port;
	} else {
		*host = estrndup(str, len);
		*port = default_port;
	}
}

/* --- Result object: query result; foreach (stream) or fetch*() (buffer) - */

typedef struct {
	void        *result;      /* ch_result* */
	zend_object *client_obj;  /* keeps the pool owner alive while the result holds a conn */
	zval         current;     /* iterator lookahead row */
	zend_long    pos;
	bool         valid;
	bool         started;
	zend_object  std;
} ch_result_object;

static zend_class_entry *ce_clickhouse_result;
static zend_object_handlers ch_result_object_handlers;

static inline ch_result_object *ch_result_object_from(zend_object *obj)
{
	return (ch_result_object *) ((char *) obj - offsetof(ch_result_object, std));
}

#define Z_CH_RESULT_OBJ_P(zv) ch_result_object_from(Z_OBJ_P(zv))

/* Build a Summary value object from the engine's statistics. */
static void ch_make_summary(void *result, zval *out)
{
	ch_summary_t s = {};

	if (result != NULL) {
		ch_result_get_summary(result, &s);
	}

	object_init_ex(out, ce_clickhouse_summary);
	zend_object *o = Z_OBJ_P(out);

	zend_update_property_long(ce_clickhouse_summary, o, "readRows", sizeof("readRows") - 1, (zend_long) s.read_rows);
	zend_update_property_long(ce_clickhouse_summary, o, "readBytes", sizeof("readBytes") - 1, (zend_long) s.read_bytes);
	zend_update_property_long(ce_clickhouse_summary, o, "writtenRows", sizeof("writtenRows") - 1, (zend_long) s.written_rows);
	zend_update_property_long(ce_clickhouse_summary, o, "writtenBytes", sizeof("writtenBytes") - 1, (zend_long) s.written_bytes);
	zend_update_property_long(ce_clickhouse_summary, o, "totalRowsToRead", sizeof("totalRowsToRead") - 1, (zend_long) s.total_rows_to_read);

	if (s.has_rows_before_limit) {
		zend_update_property_long(ce_clickhouse_summary, o, "rowsBeforeLimit", sizeof("rowsBeforeLimit") - 1, (zend_long) s.rows_before_limit);
	} else {
		zend_update_property_null(ce_clickhouse_summary, o, "rowsBeforeLimit", sizeof("rowsBeforeLimit") - 1);
	}

	zend_update_property_double(ce_clickhouse_summary, o, "elapsed", sizeof("elapsed") - 1, s.elapsed);
}

/* Advance the iterator lookahead to the next row. */
static void ch_result_object_advance(ch_result_object *obj)
{
	if (!Z_ISUNDEF(obj->current)) {
		zval_ptr_dtor(&obj->current);
		ZVAL_UNDEF(&obj->current);
	}

	zval row;
	if (obj->result != NULL && ch_result_next(obj->result, &row)) {
		ZVAL_COPY_VALUE(&obj->current, &row);
		obj->valid = true;
		obj->pos++;
	} else {
		obj->valid = false;
	}
}

static zend_object *ch_result_object_create(zend_class_entry *ce)
{
	ch_result_object *obj = (ch_result_object *) zend_object_alloc(sizeof(ch_result_object), ce);

	zend_object_std_init(&obj->std, ce);
	object_properties_init(&obj->std, ce);
	obj->std.handlers = &ch_result_object_handlers;
	obj->result = NULL;
	obj->client_obj = NULL;
	ZVAL_UNDEF(&obj->current);
	obj->pos = -1;
	obj->valid = false;
	obj->started = false;

	return &obj->std;
}

static void ch_result_object_free(zend_object *object)
{
	ch_result_object *obj = ch_result_object_from(object);

	if (!Z_ISUNDEF(obj->current)) {
		zval_ptr_dtor(&obj->current);
	}

	/* Release the connection before dropping the client keepalive, so the pool
	 * still exists when an abandoned result returns its connection. */
	if (obj->result != NULL) {
		ch_result_free(obj->result);
		obj->result = NULL;
	}

	if (obj->client_obj != NULL) {
		OBJ_RELEASE(obj->client_obj);
		obj->client_obj = NULL;
	}

	zend_object_std_dtor(&obj->std);
}

ZEND_METHOD(TrueAsync_ClickHouse_Result, fetch)
{
	ZEND_PARSE_PARAMETERS_NONE();

	ch_result_object *obj = Z_CH_RESULT_OBJ_P(ZEND_THIS);
	zval row;

	if (obj->result != NULL && ch_result_next(obj->result, &row)) {
		RETURN_COPY_VALUE(&row);
	}

	RETURN_NULL();
}

ZEND_METHOD(TrueAsync_ClickHouse_Result, fetchAll)
{
	ZEND_PARSE_PARAMETERS_NONE();

	ch_result_object *obj = Z_CH_RESULT_OBJ_P(ZEND_THIS);

	array_init(return_value);

	if (obj->result == NULL) {
		return;
	}

	zval row;
	while (ch_result_next(obj->result, &row)) {
		add_next_index_zval(return_value, &row);
	}
}

ZEND_METHOD(TrueAsync_ClickHouse_Result, fetchOne)
{
	ZEND_PARSE_PARAMETERS_NONE();

	ch_result_object *obj = Z_CH_RESULT_OBJ_P(ZEND_THIS);
	zval row;

	if (obj->result != NULL && ch_result_next(obj->result, &row)) {
		/* First column of the row, by position. */
		zval *first = zend_hash_index_find(Z_ARRVAL(row), 0);

		if (first == NULL) {
			first = (zval *) zend_hash_get_current_data(Z_ARRVAL(row));
		}

		if (first != NULL) {
			RETVAL_COPY(first);
		} else {
			RETVAL_NULL();
		}

		zval_ptr_dtor(&row);
		return;
	}

	RETURN_NULL();
}

ZEND_METHOD(TrueAsync_ClickHouse_Result, affectedRows)
{
	ZEND_PARSE_PARAMETERS_NONE();

	ch_result_object *obj = Z_CH_RESULT_OBJ_P(ZEND_THIS);
	ch_summary_t s = {};

	if (obj->result != NULL) {
		ch_result_get_summary(obj->result, &s);
	}

	RETURN_LONG((zend_long) s.written_rows);
}

ZEND_METHOD(TrueAsync_ClickHouse_Result, summary)
{
	ZEND_PARSE_PARAMETERS_NONE();
	ch_make_summary(Z_CH_RESULT_OBJ_P(ZEND_THIS)->result, return_value);
}

ZEND_METHOD(TrueAsync_ClickHouse_Result, rewind)
{
	ZEND_PARSE_PARAMETERS_NONE();

	ch_result_object *obj = Z_CH_RESULT_OBJ_P(ZEND_THIS);

	/* Single-pass: fetch the first row only on the first rewind. */
	if (!obj->started) {
		obj->started = true;
		ch_result_object_advance(obj);
	}
}

ZEND_METHOD(TrueAsync_ClickHouse_Result, valid)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_BOOL(Z_CH_RESULT_OBJ_P(ZEND_THIS)->valid);
}

ZEND_METHOD(TrueAsync_ClickHouse_Result, current)
{
	ZEND_PARSE_PARAMETERS_NONE();

	ch_result_object *obj = Z_CH_RESULT_OBJ_P(ZEND_THIS);

	if (obj->valid) {
		RETURN_COPY(&obj->current);
	}

	RETURN_NULL();
}

ZEND_METHOD(TrueAsync_ClickHouse_Result, key)
{
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_LONG(Z_CH_RESULT_OBJ_P(ZEND_THIS)->pos);
}

ZEND_METHOD(TrueAsync_ClickHouse_Result, next)
{
	ZEND_PARSE_PARAMETERS_NONE();
	ch_result_object_advance(Z_CH_RESULT_OBJ_P(ZEND_THIS));
}

/* --- Batch object: a streaming batch insert ----------------------------- */

typedef struct {
	void        *batch;       /* ch_batch* */
	zend_object *client_obj;  /* keeps the pool owner alive while the batch holds a conn */
	zend_object  std;
} ch_batch_object;

/* ce_clickhouse_batch is declared with the other class entries near the top. */
static zend_object_handlers ch_batch_object_handlers;

static inline ch_batch_object *ch_batch_object_from(zend_object *obj)
{
	return (ch_batch_object *) ((char *) obj - offsetof(ch_batch_object, std));
}

#define Z_CH_BATCH_OBJ_P(zv) ch_batch_object_from(Z_OBJ_P(zv))

static zend_object *ch_batch_object_create(zend_class_entry *ce)
{
	ch_batch_object *obj = (ch_batch_object *) zend_object_alloc(sizeof(ch_batch_object), ce);

	zend_object_std_init(&obj->std, ce);
	object_properties_init(&obj->std, ce);
	obj->std.handlers = &ch_batch_object_handlers;
	obj->batch = NULL;
	obj->client_obj = NULL;

	return &obj->std;
}

static void ch_batch_object_free(zend_object *object)
{
	ch_batch_object *obj = ch_batch_object_from(object);

	/* Release the connection (back into the pool) before dropping the client
	 * keepalive, so the pool still exists when the connection returns. */
	if (obj->batch != NULL) {
		ch_batch_free(obj->batch);
		obj->batch = NULL;
	}

	if (obj->client_obj != NULL) {
		OBJ_RELEASE(obj->client_obj);
		obj->client_obj = NULL;
	}

	zend_object_std_dtor(&obj->std);
}

/* Batch is only ever produced by Client::insertBatch(); reject a directly
 * constructed (batch == NULL) object instead of dereferencing it. */
static void *ch_batch_this(zval *zthis)
{
	void *batch = ch_batch_object_from(Z_OBJ_P(zthis))->batch;

	if (batch == NULL) {
		zend_throw_error(NULL, "TrueAsync\\ClickHouse\\Batch cannot be constructed directly; "
			"use Client::insertBatch()");
	}

	return batch;
}

ZEND_METHOD(TrueAsync_ClickHouse_Batch, append)
{
	zval *row;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY(row)
	ZEND_PARSE_PARAMETERS_END();

	void *batch = ch_batch_this(ZEND_THIS);

	if (batch == NULL || !ch_batch_append(batch, row)) {
		RETURN_THROWS();
	}
}

ZEND_METHOD(TrueAsync_ClickHouse_Batch, flush)
{
	ZEND_PARSE_PARAMETERS_NONE();

	void *batch = ch_batch_this(ZEND_THIS);

	if (batch == NULL || !ch_batch_flush(batch)) {
		RETURN_THROWS();
	}
}

ZEND_METHOD(TrueAsync_ClickHouse_Batch, count)
{
	ZEND_PARSE_PARAMETERS_NONE();

	void *batch = ch_batch_this(ZEND_THIS);

	if (batch == NULL) {
		RETURN_THROWS();
	}

	RETURN_LONG((zend_long) ch_batch_count(batch));
}

/* --- Client ------------------------------------------------------------- */

ZEND_METHOD(TrueAsync_ClickHouse_Client, __construct)
{
	zval *config;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY(config)
	ZEND_PARSE_PARAMETERS_END();

	ch_client_object *obj = Z_CH_CLIENT_OBJ_P(ZEND_THIS);
	HashTable *ht = Z_ARRVAL_P(config);

	obj->host = ch_config_dup(ht, "host", sizeof("host") - 1, "127.0.0.1");
	obj->database = ch_config_dup(ht, "database", sizeof("database") - 1, "default");
	obj->user = ch_config_dup(ht, "user", sizeof("user") - 1, "default");
	obj->password = ch_config_dup(ht, "password", sizeof("password") - 1, "");

	zval *tls = zend_hash_str_find(ht, "tls", sizeof("tls") - 1);
	obj->tls = tls != NULL && zend_is_true(tls);

	zval *tls_verify = zend_hash_str_find(ht, "tls_verify", sizeof("tls_verify") - 1);
	obj->tls_verify = tls_verify == NULL || zend_is_true(tls_verify);

	zval *port = zend_hash_str_find(ht, "port", sizeof("port") - 1);
	if (port != NULL) {
		obj->port = (uint16_t) zval_get_long(port);
	} else {
		obj->port = obj->tls ? 9440 : 9000;
	}

	/* Wire compression is a Compression enum; defaults to LZ4. */
	obj->compression = 1; /* lz4 */
	zval *compression = zend_hash_str_find(ht, "compression", sizeof("compression") - 1);
	if (compression != NULL) {
		if (Z_TYPE_P(compression) != IS_OBJECT
			|| !instanceof_function(Z_OBJCE_P(compression), ce_clickhouse_compression)) {
			zend_type_error("TrueAsync\\ClickHouse\\Client: 'compression' must be a "
				"TrueAsync\\ClickHouse\\Compression enum");
			RETURN_THROWS();
		}

		const zval *method = zend_enum_fetch_case_value(Z_OBJ_P(compression));

		if (zend_string_equals_literal(Z_STR_P(method), "none")) {
			obj->compression = 0;
		} else if (zend_string_equals_literal(Z_STR_P(method), "zstd")) {
			obj->compression = 2;
		}
	}

	/* Multi-host open strategy is an OpenStrategy enum; defaults to InOrder.
	 * Only meaningful with several 'hosts' (it picks which one each pooled
	 * connection tries first). */
	zval *strategy = zend_hash_str_find(ht, "open_strategy", sizeof("open_strategy") - 1);
	if (strategy != NULL) {
		if (Z_TYPE_P(strategy) != IS_OBJECT
			|| !instanceof_function(Z_OBJCE_P(strategy), ce_clickhouse_open_strategy)) {
			zend_type_error("TrueAsync\\ClickHouse\\Client: 'open_strategy' must be a "
				"TrueAsync\\ClickHouse\\OpenStrategy enum");
			RETURN_THROWS();
		}

		const zval *mode = zend_enum_fetch_case_value(Z_OBJ_P(strategy));

		if (zend_string_equals_literal(Z_STR_P(mode), "round_robin")) {
			obj->open_strategy = 1;
		} else if (zend_string_equals_literal(Z_STR_P(mode), "random")) {
			obj->open_strategy = 2;
		}
	}

	/* pool => ['max' => N]; min pre-warm is a follow-up (see ch_pool_ensure). */
	zval *pool = zend_hash_str_find(ht, "pool", sizeof("pool") - 1);
	if (pool != NULL && Z_TYPE_P(pool) == IS_ARRAY) {
		zval *max = zend_hash_str_find(Z_ARRVAL_P(pool), "max", sizeof("max") - 1);

		if (max != NULL && zval_get_long(max) >= 1) {
			obj->pool_max = (uint32_t) zval_get_long(max);
		}
	}

	/* 'hosts' => ['h1', 'h2:port', ...]: the first becomes the primary host,
	 * the rest are failover endpoints tried in order. */
	zval *hosts = zend_hash_str_find(ht, "hosts", sizeof("hosts") - 1);
	if (hosts != NULL && Z_TYPE_P(hosts) == IS_ARRAY) {
		HashTable *list = Z_ARRVAL_P(hosts);
		uint32_t count = zend_hash_num_elements(list);

		if (count > 1) {
			obj->endpoints = (ch_endpoint_t *) ecalloc(count - 1, sizeof(ch_endpoint_t));
		}

		size_t index = 0;
		zval *entry;
		ZEND_HASH_FOREACH_VAL(list, entry) {
			zend_string *str = zval_get_string(entry);
			char *host;
			uint16_t port;
			ch_parse_host_port(ZSTR_VAL(str), ZSTR_LEN(str), &host, &port, obj->port);

			if (index == 0) {
				efree(obj->host);
				obj->host = host;
				obj->port = port;
			} else {
				obj->endpoints[index - 1].host = host;
				obj->endpoints[index - 1].port = port;
				obj->endpoint_count = index;
			}

			zend_string_release(str);
			++index;
		} ZEND_HASH_FOREACH_END();
	}
}

ZEND_METHOD(TrueAsync_ClickHouse_Client, query)
{
	char *sql;
	size_t sql_len;
	zval *params = NULL;
	zval *options = NULL;

	ZEND_PARSE_PARAMETERS_START(1, 3)
		Z_PARAM_STRING(sql, sql_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY_OR_NULL(params)
		Z_PARAM_ARRAY_OR_NULL(options)
	ZEND_PARSE_PARAMETERS_END();

	ch_client_object *obj = Z_CH_CLIENT_OBJ_P(ZEND_THIS);

	zval conn_zv;
	ch_client_t *conn = ch_pool_acquire(obj, &conn_zv);

	if (conn == NULL) {
		RETURN_THROWS();
	}

	HashTable *param_ht = params != NULL ? Z_ARRVAL_P(params) : NULL;
	HashTable *settings_ht = NULL;

	if (options != NULL) {
		zval *s = zend_hash_str_find(Z_ARRVAL_P(options), "settings", sizeof("settings") - 1);

		if (s != NULL && Z_TYPE_P(s) == IS_ARRAY) {
			settings_ht = Z_ARRVAL_P(s);
		}
	}

	void *result = ch_result_start(conn, obj->pool, &conn_zv, sql, param_ht, settings_ht);

	if (result == NULL) {
		/* ch_result_start released conn_zv and threw before returning. */
		RETURN_THROWS();
	}

	/* The Result owns conn_zv; wrap it and keep the client (pool owner) alive
	 * until the Result is freed. */
	object_init_ex(return_value, ce_clickhouse_result);
	ch_result_object *robj = ch_result_object_from(Z_OBJ_P(return_value));
	robj->result = result;
	robj->client_obj = Z_OBJ_P(ZEND_THIS);
	GC_ADDREF(robj->client_obj);
}

ZEND_METHOD(TrueAsync_ClickHouse_Client, insert)
{
	char *table;
	size_t table_len;
	zval *columns;
	zval *rows;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_STRING(table, table_len)
		Z_PARAM_ARRAY(columns)
		Z_PARAM_ARRAY(rows)
	ZEND_PARSE_PARAMETERS_END();

	ch_client_object *obj = Z_CH_CLIENT_OBJ_P(ZEND_THIS);

	zval conn_zv;
	ch_client_t *conn = ch_pool_acquire(obj, &conn_zv);

	if (conn == NULL) {
		RETURN_THROWS();
	}

	bool ok = ch_client_insert(conn, table, Z_ARRVAL_P(columns), Z_ARRVAL_P(rows));

	ZEND_ASYNC_POOL_RELEASE(obj->pool, &conn_zv);

	if (!ok) {
		RETURN_THROWS();
	}
}

ZEND_METHOD(TrueAsync_ClickHouse_Client, insertBatch)
{
	char *table;
	size_t table_len;
	zval *columns;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(table, table_len)
		Z_PARAM_ARRAY(columns)
	ZEND_PARSE_PARAMETERS_END();

	ch_client_object *obj = Z_CH_CLIENT_OBJ_P(ZEND_THIS);

	zval conn_zv;
	ch_client_t *conn = ch_pool_acquire(obj, &conn_zv);

	if (conn == NULL) {
		RETURN_THROWS();
	}

	void *batch = ch_batch_start(conn, obj->pool, &conn_zv, table, Z_ARRVAL_P(columns));

	/* The Batch object now owns conn_zv; it releases it back to the pool when
	 * the object is destroyed. Keep the client (pool owner) alive until then. */
	object_init_ex(return_value, ce_clickhouse_batch);
	ch_batch_object *bobj = ch_batch_object_from(Z_OBJ_P(return_value));
	bobj->batch = batch;
	bobj->client_obj = Z_OBJ_P(ZEND_THIS);
	GC_ADDREF(bobj->client_obj);
}

ZEND_METHOD(TrueAsync_ClickHouse_Client, getPool)
{
	ZEND_PARSE_PARAMETERS_NONE();

	ch_client_object *obj = Z_CH_CLIENT_OBJ_P(ZEND_THIS);

	if (!ch_pool_ensure(obj)) {
		RETURN_THROWS();
	}

	if (obj->pool_wrapper == NULL) {
		obj->pool_wrapper = ZEND_ASYNC_NEW_POOL_OBJ(obj->pool);

		if (obj->pool_wrapper == NULL) {
			zend_throw_exception(ce_ch_exception, "failed to create pool wrapper", 0);
			RETURN_THROWS();
		}
	}

	GC_ADDREF(obj->pool_wrapper);
	RETURN_OBJ(obj->pool_wrapper);
}

/* --- Module ------------------------------------------------------------- */

PHP_MINIT_FUNCTION(clickhouse_async)
{
	ce_clickhouse_compression = register_class_TrueAsync_ClickHouse_Compression();
	ce_clickhouse_open_strategy = register_class_TrueAsync_ClickHouse_OpenStrategy();

	ce_ch_exception =
		register_class_TrueAsync_ClickHouse_ClickHouseException(spl_ce_RuntimeException);
	ce_ch_connection_exception =
		register_class_TrueAsync_ClickHouse_ConnectionException(ce_ch_exception);
	ce_ch_server_exception =
		register_class_TrueAsync_ClickHouse_ServerException(ce_ch_exception);
	ce_ch_protocol_exception =
		register_class_TrueAsync_ClickHouse_ProtocolException(ce_ch_exception);

	ce_clickhouse_batch = register_class_TrueAsync_ClickHouse_Batch();
	ce_clickhouse_batch->create_object = ch_batch_object_create;
	memcpy(&ch_batch_object_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	ch_batch_object_handlers.offset = offsetof(ch_batch_object, std);
	ch_batch_object_handlers.free_obj = ch_batch_object_free;

	ce_clickhouse_summary = register_class_TrueAsync_ClickHouse_Summary();

	ce_clickhouse_client = register_class_TrueAsync_ClickHouse_Client();
	ce_clickhouse_client->create_object = ch_client_object_create;
	memcpy(&ch_client_object_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	ch_client_object_handlers.offset = offsetof(ch_client_object, std);
	ch_client_object_handlers.free_obj = ch_client_object_free;

	ce_clickhouse_result = register_class_TrueAsync_ClickHouse_Result(zend_ce_iterator);
	ce_clickhouse_result->create_object = ch_result_object_create;
	memcpy(&ch_result_object_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	ch_result_object_handlers.offset = offsetof(ch_result_object, std);
	ch_result_object_handlers.free_obj = ch_result_object_free;

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(clickhouse_async)
{
	return SUCCESS;
}

PHP_MINFO_FUNCTION(clickhouse_async)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "true_async_clickhouse support", "enabled");
	php_info_print_table_row(2, "Version", PHP_CLICKHOUSE_ASYNC_VERSION);
	php_info_print_table_row(2, "clickhouse-cpp", "v2.6.1 (submodule)");
	php_info_print_table_end();
}

zend_module_entry clickhouse_async_module_entry = {
	STANDARD_MODULE_HEADER,
	"true_async_clickhouse",
	NULL,                              /* functions */
	PHP_MINIT(clickhouse_async),
	PHP_MSHUTDOWN(clickhouse_async),
	NULL,                              /* RINIT */
	NULL,                              /* RSHUTDOWN */
	PHP_MINFO(clickhouse_async),
	PHP_CLICKHOUSE_ASYNC_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_CLICKHOUSE_ASYNC
extern "C" {
ZEND_GET_MODULE(clickhouse_async)
}
#endif
