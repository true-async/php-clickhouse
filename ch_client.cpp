/*
  +----------------------------------------------------------------------+
  | php-clickhouse — native async ClickHouse client for PHP TrueAsync    |
  +----------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License").      |
  +----------------------------------------------------------------------+

  Builds and owns a clickhouse::Client over the async transport. The connection
  is opened from the hidden pool factory in coroutine context, so the handshake
  IO can suspend; reads and writes are reactor-driven.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
#include "php.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_interfaces.h"
#include "Zend/zend_async_API.h"
#include "ext/date/php_date.h"
}

#include <clickhouse/client.h>
#include <clickhouse/base/socket.h>
#include <clickhouse/block.h>
#include <clickhouse/columns/numeric.h>
#include <clickhouse/columns/string.h>
#include <clickhouse/columns/bool.h>
#include <clickhouse/columns/decimal.h>
#include <clickhouse/columns/enum.h>
#include <clickhouse/columns/ip4.h>
#include <clickhouse/columns/ip6.h>
#include <clickhouse/columns/uuid.h>
#include <clickhouse/columns/array.h>
#include <clickhouse/columns/nullable.h>
#include <clickhouse/columns/tuple.h>
#include <clickhouse/columns/map.h>
#include <clickhouse/columns/date.h>
#include <clickhouse/columns/lowcardinality.h>
#include <clickhouse/columns/itemview.h>
#include <clickhouse/columns/factory.h>
#include <clickhouse/exceptions.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ch_transport.h"
#include "ch_exceptions.h"
#include "ch_client.h"

struct ch_client {
	clickhouse::Client *client;
	bool               broken;  /* connection died mid-operation; do not reuse */
};

static clickhouse::CompressionMethod ch_map_compression(int method)
{
	switch (method) {
		case 1:  return clickhouse::CompressionMethod::LZ4;
		case 2:  return clickhouse::CompressionMethod::ZSTD;
		default: return clickhouse::CompressionMethod::None;
	}
}

/* True if a failed operation leaves the connection unusable — it must be
 * dropped from the pool, not reused. A dead transport (ConnectionError) or a
 * mid-stream wire desync (protocol decode / checksum / assertion failure) is
 * unrecoverable; a ServerException (clean error packet) or a ValidationError
 * (our own pre-send shape check) leaves the connection intact. */
static bool ch_exception_poisons_connection(const std::exception &e)
{
	return dynamic_cast<const chasync::ConnectionError *>(&e) != nullptr
		|| dynamic_cast<const clickhouse::ProtocolError *>(&e) != nullptr
		|| dynamic_cast<const clickhouse::CompressionError *>(&e) != nullptr
		|| dynamic_cast<const clickhouse::AssertionError *>(&e) != nullptr;
}

extern "C" ch_client_t *ch_client_connect(const ch_client_config_t *cfg)
{
	try {
		clickhouse::ClientOptions opts;
		opts.SetHost(cfg->host != nullptr ? cfg->host : "127.0.0.1")
			.SetPort(cfg->port != 0 ? cfg->port : 9000)
			.SetDefaultDatabase(cfg->database != nullptr ? cfg->database : "default")
			.SetUser(cfg->user != nullptr ? cfg->user : "default")
			.SetPassword(cfg->password != nullptr ? cfg->password : "")
			.SetCompressionMethod(ch_map_compression(cfg->compression));

		if (cfg->endpoint_count > 0) {
			std::vector<clickhouse::Endpoint> endpoints;
			endpoints.reserve(cfg->endpoint_count);

			for (size_t i = 0; i < cfg->endpoint_count; ++i) {
				endpoints.push_back({cfg->endpoints[i].host, cfg->endpoints[i].port});
			}

			opts.SetEndpoints(std::move(endpoints));
		}

		auto client = std::make_unique<clickhouse::Client>(opts,
			ch_make_async_socket_factory(cfg->tls, cfg->tls_verify));

		auto *wrapper = new ch_client();
		wrapper->client = client.release();
		wrapper->broken = false;

		return wrapper;
	} catch (const std::exception &e) {
		if (EG(exception) == nullptr) {
			/* A server-sent error during the handshake (auth, etc.) carries a
			 * code; anything else here is a connect/network failure. */
			if (dynamic_cast<const clickhouse::ServerException *>(&e) != nullptr) {
				ch_translate_and_throw(e);
			} else {
				zend_throw_exception(ce_ch_connection_exception, e.what(), 0);
			}
		}

		return nullptr;
	}
}

extern "C" void ch_client_free(ch_client_t *client)
{
	if (client != nullptr) {
		delete client->client;
		delete client;
	}
}

extern "C" bool ch_client_is_broken(ch_client_t *client)
{
	return client != nullptr && client->broken;
}

/* Parse a UUID string (dashes optional) into {high64, low64} — inverse of
 * ch_uuid_to_string. Non-hex characters are skipped. */
static clickhouse::UUID ch_parse_uuid(const char *str, size_t len)
{
	uint64_t hi = 0;
	uint64_t lo = 0;
	int nibbles = 0;

	for (size_t i = 0; i < len && nibbles < 32; ++i) {
		const char c = str[i];
		int nib;

		if (c >= '0' && c <= '9') {
			nib = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			nib = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			nib = c - 'A' + 10;
		} else {
			continue;
		}

		if (nibbles < 16) {
			hi = (hi << 4) | (uint64_t) nib;
		} else {
			lo = (lo << 4) | (uint64_t) nib;
		}

		++nibbles;
	}

	return clickhouse::UUID{hi, lo};
}

/* Unix timestamp from a PHP value: a DateTimeInterface or a plain int. */
static zend_long ch_zval_to_timestamp(zval *value)
{
	if (Z_TYPE_P(value) == IS_OBJECT
		&& instanceof_function(Z_OBJCE_P(value), php_date_get_interface_ce())) {
		zval retval;
		zend_call_method_with_0_params(Z_OBJ_P(value), Z_OBJCE_P(value), NULL, "gettimestamp", &retval);
		zend_long ts = zval_get_long(&retval);
		zval_ptr_dtor(&retval);
		return ts;
	}

	return zval_get_long(value);
}

/* Per-insert cache of empty key/value template columns for Map cells, keyed by
 * the Map type. Cloning a cached template per row beats re-parsing the key and
 * value type names on every row. Scoped to one ch_send_block call, so the cached
 * Type pointers stay alive. */
using ch_map_templates =
	std::unordered_map<const clickhouse::Type *,
		std::pair<clickhouse::ColumnRef, clickhouse::ColumnRef>>;

/* Append one PHP value to a typed column for INSERT. Other types throw (caught
 * and surfaced as a PHP exception). `tmpl` memoizes Map key/value templates. */
static void ch_append_value(const clickhouse::ColumnRef &col, zval *value, ch_map_templates &tmpl)
{
	using clickhouse::Type;

	switch (col->Type()->GetCode()) {
		case Type::Int8:    col->As<clickhouse::ColumnInt8>()->Append((int8_t) zval_get_long(value)); break;
		case Type::Int16:   col->As<clickhouse::ColumnInt16>()->Append((int16_t) zval_get_long(value)); break;
		case Type::Int32:   col->As<clickhouse::ColumnInt32>()->Append((int32_t) zval_get_long(value)); break;
		case Type::Int64:   col->As<clickhouse::ColumnInt64>()->Append((int64_t) zval_get_long(value)); break;
		case Type::UInt8:   col->As<clickhouse::ColumnUInt8>()->Append((uint8_t) zval_get_long(value)); break;
		case Type::UInt16:  col->As<clickhouse::ColumnUInt16>()->Append((uint16_t) zval_get_long(value)); break;
		case Type::UInt32:  col->As<clickhouse::ColumnUInt32>()->Append((uint32_t) zval_get_long(value)); break;
		case Type::UInt64:  col->As<clickhouse::ColumnUInt64>()->Append((uint64_t) zval_get_long(value)); break;
		case Type::Float32: col->As<clickhouse::ColumnFloat32>()->Append((float) zval_get_double(value)); break;
		case Type::Float64: col->As<clickhouse::ColumnFloat64>()->Append(zval_get_double(value)); break;
		case Type::Bool:    col->As<clickhouse::ColumnBool>()->Append(zend_is_true(value)); break;

		case Type::String: {
			zend_string *str = zval_get_string(value);
			col->As<clickhouse::ColumnString>()->Append(std::string_view(ZSTR_VAL(str), ZSTR_LEN(str)));
			zend_string_release(str);
			break;
		}

		case Type::FixedString: {
			zend_string *str = zval_get_string(value);
			col->As<clickhouse::ColumnFixedString>()->Append(std::string_view(ZSTR_VAL(str), ZSTR_LEN(str)));
			zend_string_release(str);
			break;
		}

		case Type::Date:
			col->As<clickhouse::ColumnDate>()->Append((std::time_t) ch_zval_to_timestamp(value));
			break;

		case Type::Date32:
			col->As<clickhouse::ColumnDate32>()->Append((std::time_t) ch_zval_to_timestamp(value));
			break;

		case Type::DateTime:
			col->As<clickhouse::ColumnDateTime>()->Append((std::time_t) ch_zval_to_timestamp(value));
			break;

		case Type::DateTime64: {
			/* Stored as ticks = unix_seconds * 10^precision. A DateTimeInterface
			 * contributes sub-second microseconds; a number is whole seconds. */
			auto c = col->As<clickhouse::ColumnDateTime64>();
			int64_t scale = 1;
			for (size_t i = 0, p = c->GetPrecision(); i < p; ++i) {
				scale *= 10;
			}

			int64_t ticks;
			if (Z_TYPE_P(value) == IS_OBJECT
				&& instanceof_function(Z_OBJCE_P(value), php_date_get_interface_ce())) {
				const int64_t sec = ch_zval_to_timestamp(value);
				const int64_t us = (int64_t) Z_PHPDATE_P(value)->time->us;
				ticks = sec * scale + (us * scale) / 1000000;
			} else if (Z_TYPE_P(value) == IS_DOUBLE) {
				ticks = (int64_t) std::llround(Z_DVAL_P(value) * (double) scale);
			} else {
				ticks = (int64_t) zval_get_long(value) * scale;
			}

			c->Append(ticks);
			break;
		}

		case Type::IPv4: {
			zend_string *str = zval_get_string(value);
			col->As<clickhouse::ColumnIPv4>()->Append(std::string(ZSTR_VAL(str), ZSTR_LEN(str)));
			zend_string_release(str);
			break;
		}

		case Type::IPv6: {
			zend_string *str = zval_get_string(value);
			col->As<clickhouse::ColumnIPv6>()->Append(std::string_view(ZSTR_VAL(str), ZSTR_LEN(str)));
			zend_string_release(str);
			break;
		}

		case Type::UUID: {
			zend_string *str = zval_get_string(value);
			col->As<clickhouse::ColumnUUID>()->Append(ch_parse_uuid(ZSTR_VAL(str), ZSTR_LEN(str)));
			zend_string_release(str);
			break;
		}

		case Type::Enum8:
			if (Z_TYPE_P(value) == IS_STRING) {
				col->As<clickhouse::ColumnEnum8>()->Append(std::string(Z_STRVAL_P(value), Z_STRLEN_P(value)));
			} else {
				col->As<clickhouse::ColumnEnum8>()->Append((int8_t) zval_get_long(value));
			}
			break;

		case Type::Enum16:
			if (Z_TYPE_P(value) == IS_STRING) {
				col->As<clickhouse::ColumnEnum16>()->Append(std::string(Z_STRVAL_P(value), Z_STRLEN_P(value)));
			} else {
				col->As<clickhouse::ColumnEnum16>()->Append((int16_t) zval_get_long(value));
			}
			break;

		case Type::Decimal:
		case Type::Decimal32:
		case Type::Decimal64:
		case Type::Decimal128: {
			/* ColumnDecimal parses the string per its own scale. */
			zend_string *str = zval_get_string(value);
			col->As<clickhouse::ColumnDecimal>()->Append(std::string(ZSTR_VAL(str), ZSTR_LEN(str)));
			zend_string_release(str);
			break;
		}

		case Type::Nullable: {
			auto nullable = col->As<clickhouse::ColumnNullable>();
			const clickhouse::ColumnRef nested = nullable->Nested();

			if (Z_TYPE_P(value) == IS_NULL) {
				/* The nested cell is masked by the null flag; append a
				 * placeholder so nested and flags stay the same length. */
				zval zero;
				ZVAL_LONG(&zero, 0);
				ch_append_value(nested, &zero, tmpl);
				nullable->Append(true);
			} else {
				ch_append_value(nested, value, tmpl);
				nullable->Append(false);
			}

			break;
		}

		case Type::Array: {
			if (Z_TYPE_P(value) != IS_ARRAY) {
				throw clickhouse::ValidationError("clickhouse: an Array column expects a PHP array");
			}

			/* Clone the array's element column as an empty template — cheaper than
			 * re-parsing the item type name on every row — fill it (recursing for
			 * nested arrays / nullables), then append it as one array row. */
			auto array = col->As<clickhouse::ColumnArray>();
			clickhouse::ColumnRef elements = array->GetData()->CloneEmpty();

			zval *el;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(value), el) {
				ch_append_value(elements, el, tmpl);
			} ZEND_HASH_FOREACH_END();

			array->AppendAsColumn(elements);
			break;
		}

		case Type::Tuple: {
			if (Z_TYPE_P(value) != IS_ARRAY) {
				throw clickhouse::ValidationError("clickhouse: a Tuple column expects a PHP array");
			}

			/* A Tuple has no own storage: append one value to each member column
			 * positionally (PHP order matches the tuple's element order). */
			auto tuple = col->As<clickhouse::ColumnTuple>();
			const size_t arity = tuple->TupleSize();
			size_t i = 0;
			zval *el;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(value), el) {
				if (i >= arity) {
					throw clickhouse::ValidationError("clickhouse: too many Tuple elements");
				}

				ch_append_value((*tuple)[i], el, tmpl);
				++i;
			} ZEND_HASH_FOREACH_END();

			if (i != arity) {
				throw clickhouse::ValidationError("clickhouse: too few Tuple elements");
			}

			break;
		}

		case Type::Map: {
			if (Z_TYPE_P(value) != IS_ARRAY) {
				throw clickhouse::ValidationError("clickhouse: a Map column expects a PHP array");
			}

			/* A Map is Array(Tuple(key, value)). Fill key/value columns from the
			 * PHP assoc array, wrap them in a one-row Array(Tuple) and append it
			 * to the map as a single row. */
			auto map_type = col->Type()->As<clickhouse::MapType>();

			/* Parse the key/value types once per Map type, then clone the cached
			 * templates per row instead of re-parsing the type names each time. */
			auto &tpl = tmpl[col->Type().get()];
			if (tpl.first == nullptr) {
				clickhouse::ColumnRef k = clickhouse::CreateColumnByType(map_type->GetKeyType()->GetName());
				clickhouse::ColumnRef v = clickhouse::CreateColumnByType(map_type->GetValueType()->GetName());

				if (k == nullptr || v == nullptr) {
					throw clickhouse::ValidationError("clickhouse: unsupported Map key/value type");
				}

				tpl.first = k;
				tpl.second = v;
			}

			clickhouse::ColumnRef keys = tpl.first->CloneEmpty();
			clickhouse::ColumnRef values = tpl.second->CloneEmpty();

			zend_string *str_key;
			zend_ulong num_key;
			zval *entry;
			ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(value), num_key, str_key, entry) {
				zval key;
				if (str_key != nullptr) {
					ZVAL_STR(&key, str_key);
				} else {
					ZVAL_LONG(&key, (zend_long) num_key);
				}

				ch_append_value(keys, &key, tmpl);
				ch_append_value(values, entry, tmpl);
			} ZEND_HASH_FOREACH_END();

			auto entries = std::make_shared<clickhouse::ColumnTuple>(
				std::vector<clickhouse::ColumnRef>{keys, values});

			/* Wrap the filled entries in a one-row Array(Tuple) (an empty tuple of
			 * the same type is the array's element template), then a one-row Map. */
			auto array = std::make_shared<clickhouse::ColumnArray>(entries->CloneEmpty());
			array->AppendAsColumn(entries);

			col->As<clickhouse::ColumnMap>()->Append(std::make_shared<clickhouse::ColumnMap>(array));
			break;
		}

		default:
			throw std::runtime_error(
				std::string("clickhouse: insert not supported for column type ") + col->Type()->GetName());
	}
}

/* Build "INSERT INTO <table> (<cols>) VALUES" and collect the column names. */
static std::string ch_build_insert_statement(const char *table, HashTable *columns,
		std::vector<std::string> &names)
{
	std::string fields;

	zval *name_zv;
	ZEND_HASH_FOREACH_VAL(columns, name_zv) {
		zend_string *str = zval_get_string(name_zv);

		if (!names.empty()) {
			fields += ", ";
		}

		fields.append(ZSTR_VAL(str), ZSTR_LEN(str));
		names.emplace_back(ZSTR_VAL(str), ZSTR_LEN(str));
		zend_string_release(str);
	} ZEND_HASH_FOREACH_END();

	return std::string("INSERT INTO ") + table + " (" + fields + ") VALUES";
}

/* Send `rows` (a list of positional row arrays) as one INSERT block, building
 * the typed columns from the server's sample block. On error throws the PHP
 * exception, sets *broken when the connection can no longer be reused, and
 * returns false. Shared by Client::insert and Batch::flush. */
static bool ch_send_block(clickhouse::Client *client, const std::string &statement,
		const std::vector<std::string> &names, HashTable *rows, bool *broken)
{
	const size_t count = names.size();

	try {
		/* Server returns the sample block whose column types we build against. */
		clickhouse::Block sample = client->BeginInsert(statement);

		if (sample.GetColumnCount() != count) {
			/* The catch below closes the open insert session (single EndInsert). */
			throw clickhouse::ValidationError("clickhouse: insert column count does not match the table");
		}

		std::vector<clickhouse::ColumnRef> data(count);
		for (size_t i = 0; i < count; ++i) {
			data[i] = sample[i]->CloneEmpty();
		}

		/* Shared across all rows so Map key/value templates are parsed once. */
		ch_map_templates tmpl;

		zval *row_zv;
		ZEND_HASH_FOREACH_VAL(rows, row_zv) {
			if (Z_TYPE_P(row_zv) != IS_ARRAY) {
				throw clickhouse::ValidationError("clickhouse: each insert row must be an array");
			}

			size_t i = 0;
			zval *cell;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(row_zv), cell) {
				if (i >= count) {
					throw clickhouse::ValidationError("clickhouse: insert row has more values than columns");
				}

				ch_append_value(data[i], cell, tmpl);
				++i;
			} ZEND_HASH_FOREACH_END();

			if (i != count) {
				throw clickhouse::ValidationError("clickhouse: insert row has fewer values than columns");
			}
		} ZEND_HASH_FOREACH_END();

		clickhouse::Block block;
		for (size_t i = 0; i < count; ++i) {
			block.AppendColumn(names[i], data[i]);
		}

		client->SendInsertBlock(block);
		client->EndInsert();

		return true;
	} catch (const std::exception &e) {
		if (ch_exception_poisons_connection(e)) {
			*broken = true;
		} else {
			/* Best-effort: close the insert session so the connection stays
			 * reusable (a poisoned connection is already unusable). */
			try {
				client->EndInsert();
			} catch (...) {
				*broken = true;
			}
		}

		ch_translate_and_throw(e);
		return false;
	}
}

extern "C" bool ch_client_insert(ch_client_t *client, const char *table, HashTable *columns, HashTable *rows)
{
	std::vector<std::string> names;
	std::string statement = ch_build_insert_statement(table, columns, names);

	return ch_send_block(client->client, statement, names, rows, &client->broken);
}

/* Format a clickhouse UUID ({high64, low64}) as the canonical 8-4-4-4-12 hex. */
static std::string ch_uuid_to_string(const clickhouse::UUID &uuid)
{
	const uint64_t hi = uuid.first;
	const uint64_t lo = uuid.second;
	char buf[37];

	std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx",
		(uint32_t) (hi >> 32),
		(uint32_t) ((hi >> 16) & 0xFFFF),
		(uint32_t) (hi & 0xFFFF),
		(uint32_t) (lo >> 48),
		(unsigned long long) (lo & 0xFFFFFFFFFFFFULL));

	return std::string(buf);
}

/* Render a Decimal (raw integer + scale) losslessly as a string. */
static std::string ch_decimal_to_string(clickhouse::Int128 raw, size_t scale)
{
	std::ostringstream oss;
	oss << raw;
	std::string str = oss.str();

	if (scale == 0) {
		return str;
	}

	const bool negative = !str.empty() && str.front() == '-';
	std::string digits = negative ? str.substr(1) : str;

	if (digits.size() <= scale) {
		digits.insert(0, scale - digits.size() + 1, '0');
	}

	const size_t point = digits.size() - scale;
	std::string out = digits.substr(0, point) + "." + digits.substr(point);

	return negative ? "-" + out : out;
}

/* Build a DateTimeImmutable from a unix timestamp (UTC). */
static void ch_make_datetime_immutable(zval *out, zend_long sec, int usec)
{
	php_date_instantiate(php_date_get_immutable_ce(), out);
	php_date_initialize_from_ts_long(Z_PHPDATE_P(out), sec, usec);
}

/* Convert one column cell to a PHP value. Recurses for composite types. */
static void ch_column_value_to_zval(const clickhouse::ColumnRef &col, size_t row, zval *out)
{
	using clickhouse::Type;

	switch (col->Type()->GetCode()) {
		case Type::Int8:    ZVAL_LONG(out, (*col->As<clickhouse::ColumnInt8>())[row]); break;
		case Type::Int16:   ZVAL_LONG(out, (*col->As<clickhouse::ColumnInt16>())[row]); break;
		case Type::Int32:   ZVAL_LONG(out, (*col->As<clickhouse::ColumnInt32>())[row]); break;
		case Type::Int64:   ZVAL_LONG(out, (zend_long) (*col->As<clickhouse::ColumnInt64>())[row]); break;
		case Type::UInt8:   ZVAL_LONG(out, (*col->As<clickhouse::ColumnUInt8>())[row]); break;
		case Type::UInt16:  ZVAL_LONG(out, (*col->As<clickhouse::ColumnUInt16>())[row]); break;
		case Type::UInt32:  ZVAL_LONG(out, (zend_long) (*col->As<clickhouse::ColumnUInt32>())[row]); break;

		case Type::UInt64: {
			uint64_t v = (*col->As<clickhouse::ColumnUInt64>())[row];

			/* Values past PHP_INT_MAX overflow to float, matching PHP's own
			 * integer-overflow behaviour. */
			if (v > (uint64_t) ZEND_LONG_MAX) {
				ZVAL_DOUBLE(out, (double) v);
			} else {
				ZVAL_LONG(out, (zend_long) v);
			}

			break;
		}

		case Type::Float32: ZVAL_DOUBLE(out, (*col->As<clickhouse::ColumnFloat32>())[row]); break;
		case Type::Float64: ZVAL_DOUBLE(out, (*col->As<clickhouse::ColumnFloat64>())[row]); break;

		case Type::String: {
			auto s = (*col->As<clickhouse::ColumnString>())[row];
			ZVAL_STRINGL(out, s.data(), s.size());
			break;
		}

		case Type::FixedString: {
			auto s = (*col->As<clickhouse::ColumnFixedString>())[row];
			ZVAL_STRINGL(out, s.data(), s.size());
			break;
		}

		case Type::Bool:
			ZVAL_BOOL(out, (*col->As<clickhouse::ColumnBool>())[row]);
			break;

		case Type::Enum8: {
			auto name = col->As<clickhouse::ColumnEnum8>()->NameAt(row);
			ZVAL_STRINGL(out, name.data(), name.size());
			break;
		}

		case Type::Enum16: {
			auto name = col->As<clickhouse::ColumnEnum16>()->NameAt(row);
			ZVAL_STRINGL(out, name.data(), name.size());
			break;
		}

		case Type::UUID: {
			std::string s = ch_uuid_to_string((*col->As<clickhouse::ColumnUUID>())[row]);
			ZVAL_STRINGL(out, s.data(), s.size());
			break;
		}

		case Type::IPv4: {
			std::string s = col->As<clickhouse::ColumnIPv4>()->AsString(row);
			ZVAL_STRINGL(out, s.data(), s.size());
			break;
		}

		case Type::IPv6: {
			std::string s = col->As<clickhouse::ColumnIPv6>()->AsString(row);
			ZVAL_STRINGL(out, s.data(), s.size());
			break;
		}

		case Type::Decimal:
		case Type::Decimal32:
		case Type::Decimal64:
		case Type::Decimal128: {
			auto dec = col->As<clickhouse::ColumnDecimal>();
			std::string s = ch_decimal_to_string(dec->At(row), dec->GetScale());
			ZVAL_STRINGL(out, s.data(), s.size());
			break;
		}

		case Type::Int128: {
			std::ostringstream oss;
			oss << (*col->As<clickhouse::ColumnInt128>())[row];
			std::string s = oss.str();
			ZVAL_STRINGL(out, s.data(), s.size());
			break;
		}

		case Type::UInt128: {
			std::ostringstream oss;
			oss << (*col->As<clickhouse::ColumnUInt128>())[row];
			std::string s = oss.str();
			ZVAL_STRINGL(out, s.data(), s.size());
			break;
		}

		case Type::Date:
			ch_make_datetime_immutable(out, (zend_long) (*col->As<clickhouse::ColumnDate>())[row], 0);
			break;

		case Type::Date32:
			ch_make_datetime_immutable(out, (zend_long) (*col->As<clickhouse::ColumnDate32>())[row], 0);
			break;

		case Type::DateTime:
			ch_make_datetime_immutable(out, (zend_long) (*col->As<clickhouse::ColumnDateTime>())[row], 0);
			break;

		case Type::DateTime64: {
			auto column = col->As<clickhouse::ColumnDateTime64>();
			const int64_t ticks = column->At(row);

			int64_t scale = 1;
			for (size_t i = 0; i < column->GetPrecision(); ++i) {
				scale *= 10;
			}

			int64_t sec = ticks / scale;
			int64_t frac = ticks % scale;

			/* Floor toward negative infinity for pre-epoch timestamps. */
			if (frac < 0) {
				frac += scale;
				sec -= 1;
			}

			const int usec = scale > 1 ? (int) (frac * 1000000 / scale) : 0;
			ch_make_datetime_immutable(out, (zend_long) sec, usec);
			break;
		}

		case Type::Nullable: {
			auto nullable = col->As<clickhouse::ColumnNullable>();

			if (nullable->IsNull(row)) {
				ZVAL_NULL(out);
			} else {
				ch_column_value_to_zval(nullable->Nested(), row, out);
			}

			break;
		}

		case Type::Array: {
			const clickhouse::ColumnRef elements = col->As<clickhouse::ColumnArray>()->GetAsColumn(row);
			const size_t count = elements->Size();

			array_init_size(out, count);

			for (size_t i = 0; i < count; ++i) {
				zval element;
				ch_column_value_to_zval(elements, i, &element);
				add_next_index_zval(out, &element);
			}

			break;
		}

		case Type::Tuple: {
			auto tuple = col->As<clickhouse::ColumnTuple>();
			const size_t count = tuple->TupleSize();

			array_init_size(out, count);

			for (size_t i = 0; i < count; ++i) {
				zval element;
				ch_column_value_to_zval((*tuple)[i], row, &element);
				add_next_index_zval(out, &element);
			}

			break;
		}

		case Type::Map: {
			/* GetAsColumn(row) is a Tuple(key, value) column, one row per entry. */
			const clickhouse::ColumnRef entries = col->As<clickhouse::ColumnMap>()->GetAsColumn(row);
			auto pair = entries->As<clickhouse::ColumnTuple>();
			const clickhouse::ColumnRef keys = (*pair)[0];
			const clickhouse::ColumnRef values = (*pair)[1];
			const size_t count = entries->Size();

			array_init_size(out, count);

			for (size_t i = 0; i < count; ++i) {
				zval key;
				zval value;
				ch_column_value_to_zval(keys, i, &key);
				ch_column_value_to_zval(values, i, &value);

				if (Z_TYPE(key) == IS_LONG) {
					add_index_zval(out, Z_LVAL(key), &value);
				} else {
					if (Z_TYPE(key) != IS_STRING) {
						convert_to_string(&key);
					}

					add_assoc_zval_ex(out, Z_STRVAL(key), Z_STRLEN(key), &value);
					zval_ptr_dtor(&key);
				}
			}

			break;
		}

		case Type::LowCardinality: {
			/* clickhouse-cpp only supports LowCardinality over (Fixed)String.
			 * GetItem yields a Void ItemView for a null in LowCardinality(
			 * Nullable(String)). */
			const clickhouse::ItemView iv = col->As<clickhouse::ColumnLowCardinality>()->GetItem(row);

			if (iv.type == Type::String || iv.type == Type::FixedString) {
				auto s = iv.get<std::string_view>();
				ZVAL_STRINGL(out, s.data(), s.size());
			} else {
				ZVAL_NULL(out);
			}

			break;
		}

		default:
			ZVAL_NULL(out);
			break;
	}
}

/* Convert a PHP value to a native query parameter string (the server casts it
 * per the {name:Type} placeholder). PHP null becomes a NULL parameter. */
static clickhouse::QueryParamValue ch_param_value(zval *value)
{
	switch (Z_TYPE_P(value)) {
		case IS_NULL:
			return std::nullopt;

		case IS_TRUE:
			return std::string("true");

		case IS_FALSE:
			return std::string("false");

		default: {
			zend_string *str = zval_get_string(value);
			std::string out(ZSTR_VAL(str), ZSTR_LEN(str));
			zend_string_release(str);
			return out;
		}
	}
}

/* Apply native {name:Type} parameters and per-query settings to a Query. */
static void ch_apply_params_settings(clickhouse::Query &query, HashTable *params, HashTable *settings)
{
	if (params != nullptr) {
		zend_string *name;
		zval *value;

		ZEND_HASH_FOREACH_STR_KEY_VAL(params, name, value) {
			if (name != nullptr) {
				query.SetParam(std::string(ZSTR_VAL(name), ZSTR_LEN(name)), ch_param_value(value));
			}
		} ZEND_HASH_FOREACH_END();
	}

	if (settings != nullptr) {
		zend_string *name;
		zval *value;

		ZEND_HASH_FOREACH_STR_KEY_VAL(settings, name, value) {
			if (name != nullptr) {
				zend_string *str = zval_get_string(value);
				clickhouse::QuerySettingsField field;
				field.value = std::string(ZSTR_VAL(str), ZSTR_LEN(str));
				query.SetSetting(std::string(ZSTR_VAL(name), ZSTR_LEN(name)), field);
				zend_string_release(str);
			}
		} ZEND_HASH_FOREACH_END();
	}
}


/* --- Result (lazy pull, single coroutine) ------------------------------- */

/* Holds the in-flight query and current block, pulled lazily via NextBlock() in
 * the consumer coroutine. Column refs and interned name keys are cached per
 * block so the per-row loop avoids shared_ptr copies and name re-hashing. */
struct ch_result {
	ch_client_t        *conn;
	clickhouse::Client *client;
	zend_async_pool_t  *pool;
	zval                conn_zv;
	clickhouse::Block   block;
	std::vector<clickhouse::ColumnRef> cols;
	std::vector<zend_string *>         keys;
	size_t              row = 0;
	size_t              rows = 0;
	bool                done = false;
	bool                broken = false;
	bool                released = false;
	ch_summary_t        summary{};
	std::chrono::steady_clock::time_point started;

	~ch_result()
	{
		for (zend_string *key : keys) {
			zend_string_release(key);
		}
	}
};

static void ch_result_release(ch_result *r)
{
	if (r->released) {
		return;
	}

	if (r->broken) {
		r->conn->broken = true;
	}

	ZEND_ASYNC_POOL_RELEASE(r->pool, &r->conn_zv);
	r->released = true;
}

/* End of stream: stamp elapsed and return the connection to the pool. */
static void ch_result_finish(ch_result *r)
{
	if (!r->done) {
		r->done = true;
		r->summary.elapsed = std::chrono::duration<double>(
			std::chrono::steady_clock::now() - r->started).count();
	}

	ch_result_release(r);
}

/* Adopt a freshly pulled block: cache its column refs and intern its name keys. */
static void ch_result_load_block(ch_result *r, clickhouse::Block &&block)
{
	for (zend_string *key : r->keys) {
		zend_string_release(key);
	}

	r->block = std::move(block);
	r->rows = r->block.GetRowCount();
	r->row = 0;

	const size_t columns = r->block.GetColumnCount();
	r->cols.resize(columns);
	r->keys.resize(columns);

	for (size_t c = 0; c < columns; ++c) {
		r->cols[c] = r->block[c];
		const std::string &name = r->block.GetColumnName(c);
		r->keys[c] = zend_string_init(name.c_str(), name.size(), 0);
	}
}

extern "C" void *ch_result_start(ch_client_t *client, zend_async_pool_t *pool, zval *conn_zv,
		const char *sql, HashTable *params, HashTable *settings)
{
	ch_result *r = new ch_result();
	r->conn = client;
	r->client = client->client;
	r->pool = pool;
	ZVAL_COPY_VALUE(&r->conn_zv, conn_zv);
	r->started = std::chrono::steady_clock::now();

	try {
		clickhouse::Query query(sql);
		ch_apply_params_settings(query, params, settings);

		query.OnProgress([r](const clickhouse::Progress &p) {
			r->summary.read_rows += p.rows;
			r->summary.read_bytes += p.bytes;
			r->summary.written_rows += p.written_rows;
			r->summary.written_bytes += p.written_bytes;

			if (p.total_rows > r->summary.total_rows_to_read) {
				r->summary.total_rows_to_read = p.total_rows;
			}
		});

		query.OnProfile([r](const clickhouse::Profile &profile) {
			if (profile.calculated_rows_before_limit) {
				r->summary.rows_before_limit = profile.rows_before_limit;
				r->summary.has_rows_before_limit = true;
			}
		});

		r->client->BeginExecute(query);

		/* Prime the first data block so the statement has executed by now (DDL /
		 * INSERT … SELECT take effect even if never read) and errors surface here.
		 * Empty header blocks are skipped; no data at all is a no-row result. */
		for (;;) {
			std::optional<clickhouse::Block> next = r->client->NextBlock();

			if (!next.has_value()) {
				ch_result_finish(r);
				break;
			}

			if (next->GetRowCount() > 0) {
				ch_result_load_block(r, std::move(*next));
				break;
			}
		}

		return r;
	} catch (const std::exception &e) {
		r->broken = ch_exception_poisons_connection(e);
		ch_result_finish(r);
		ch_translate_and_throw(e);

		delete r;
		return nullptr;
	}
}

extern "C" bool ch_result_next(void *result, zval *out)
{
	ch_result *r = (ch_result *) result;

	try {
		for (;;) {
			if (r->row < r->rows) {
				const size_t columns = r->cols.size();

				array_init_size(out, columns);

				for (size_t c = 0; c < columns; ++c) {
					zval value;
					ch_column_value_to_zval(r->cols[c], r->row, &value);
					zend_hash_update(Z_ARRVAL_P(out), r->keys[c], &value);
				}

				++r->row;
				return true;
			}

			if (r->done) {
				return false;
			}

			std::optional<clickhouse::Block> next = r->client->NextBlock();

			if (!next.has_value()) {
				ch_result_finish(r);
				return false;
			}

			ch_result_load_block(r, std::move(*next));
		}
	} catch (const std::exception &e) {
		r->broken = ch_exception_poisons_connection(e);
		ch_result_finish(r);
		ch_translate_and_throw(e);

		return false;
	}
}

extern "C" void ch_result_get_summary(void *result, ch_summary_t *out)
{
	*out = ((ch_result *) result)->summary;
}

extern "C" void ch_result_free(void *result)
{
	ch_result *r = (ch_result *) result;

	if (!r->released) {
		/* Abandoned mid-stream: the connection still has unread blocks, so it
		 * is dirty — drop it rather than risk reusing a half-read connection. */
		if (!r->done) {
			r->broken = true;
		}

		ch_result_release(r);
	}

	delete r;
}

/* --- Streaming batch insert --------------------------------------------- */

/* Pins one pooled connection; rows accumulate in `pending` (a PHP list of row
 * arrays) and each flush sends them as one self-contained insert, leaving the
 * connection idle between flushes — so the destructor never needs network IO. */
struct ch_batch {
	ch_client_t             *conn;
	clickhouse::Client      *client;
	zend_async_pool_t       *pool;
	zval                     conn_zv;
	std::string              statement;  /* "INSERT INTO t (a, b) VALUES" */
	std::vector<std::string> names;
	zend_array              *pending;    /* buffered rows, not yet flushed */
	bool                     broken = false;
	bool                     released = false;
};

static void ch_batch_release(ch_batch *b)
{
	if (b->released) {
		return;
	}

	if (b->broken) {
		b->conn->broken = true;
	}

	ZEND_ASYNC_POOL_RELEASE(b->pool, &b->conn_zv);
	b->released = true;
}

extern "C" void *ch_batch_start(ch_client_t *client, zend_async_pool_t *pool, zval *conn_zv,
		const char *table, HashTable *columns)
{
	ch_batch *b = new ch_batch();
	b->conn = client;
	b->client = client->client;
	b->pool = pool;
	ZVAL_COPY_VALUE(&b->conn_zv, conn_zv);
	b->pending = zend_new_array(0);

	b->statement = ch_build_insert_statement(table, columns, b->names);

	return b;
}

extern "C" bool ch_batch_append(void *batch, zval *row)
{
	ch_batch *b = (ch_batch *) batch;

	if (Z_TYPE_P(row) != IS_ARRAY) {
		zend_throw_error(zend_ce_value_error, "clickhouse: each insert row must be an array");
		return false;
	}

	if (zend_hash_num_elements(Z_ARRVAL_P(row)) != b->names.size()) {
		zend_throw_error(zend_ce_value_error,
			"clickhouse: row has %u values but the batch has %u columns",
			(unsigned) zend_hash_num_elements(Z_ARRVAL_P(row)), (unsigned) b->names.size());
		return false;
	}

	zval copy;
	ZVAL_COPY(&copy, row);
	zend_hash_next_index_insert(b->pending, &copy);

	return true;
}

extern "C" bool ch_batch_flush(void *batch)
{
	ch_batch *b = (ch_batch *) batch;

	if (zend_hash_num_elements(b->pending) == 0) {
		return true;
	}

	if (!ch_send_block(b->client, b->statement, b->names, b->pending, &b->broken)) {
		return false;
	}

	/* Sent: drop the buffered rows so the batch can be reused. */
	zend_array_release(b->pending);
	b->pending = zend_new_array(0);

	return true;
}

extern "C" uint64_t ch_batch_count(void *batch)
{
	ch_batch *b = (ch_batch *) batch;
	return zend_hash_num_elements(b->pending);
}

extern "C" void ch_batch_free(void *batch)
{
	ch_batch *b = (ch_batch *) batch;

	/* Each flush self-finalizes, so the connection is idle here — release it
	 * with no network IO. Unflushed rows are discarded. */
	zend_array_release(b->pending);
	ch_batch_release(b);

	delete b;
}
