/* This is a generated file, edit clickhouse_async.stub.php instead.
 * Stub hash: 37b9a0d55467953d58e57181c9ec8e9ce7f6b340 */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Batch_append, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, row, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Batch_flush, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Batch_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Result_fetch, 0, 0, IS_ARRAY, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Result_fetchAll, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Result_fetchOne, 0, 0, IS_MIXED, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_TrueAsync_ClickHouse_Result_affectedRows arginfo_class_TrueAsync_ClickHouse_Batch_count

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Result_summary, 0, 0, TrueAsync\\ClickHouse\\Summary, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_TrueAsync_ClickHouse_Result_current arginfo_class_TrueAsync_ClickHouse_Result_fetchOne

#define arginfo_class_TrueAsync_ClickHouse_Result_key arginfo_class_TrueAsync_ClickHouse_Batch_count

#define arginfo_class_TrueAsync_ClickHouse_Result_next arginfo_class_TrueAsync_ClickHouse_Batch_flush

#define arginfo_class_TrueAsync_ClickHouse_Result_rewind arginfo_class_TrueAsync_ClickHouse_Batch_flush

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Result_valid, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Client___construct, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, config, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Client_query, 0, 1, TrueAsync\\ClickHouse\\Result, 0)
	ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, params, IS_ARRAY, 0, "[]")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Client_insert, 0, 3, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, table, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, columns, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, rows, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Client_insertBatch, 0, 2, TrueAsync\\ClickHouse\\Batch, 0)
	ZEND_ARG_TYPE_INFO(0, table, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, columns, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(arginfo_class_TrueAsync_ClickHouse_Client_getPool, 0, 0, Async\\Pool, 0)
ZEND_END_ARG_INFO()

ZEND_METHOD(TrueAsync_ClickHouse_Batch, append);
ZEND_METHOD(TrueAsync_ClickHouse_Batch, flush);
ZEND_METHOD(TrueAsync_ClickHouse_Batch, count);
ZEND_METHOD(TrueAsync_ClickHouse_Result, fetch);
ZEND_METHOD(TrueAsync_ClickHouse_Result, fetchAll);
ZEND_METHOD(TrueAsync_ClickHouse_Result, fetchOne);
ZEND_METHOD(TrueAsync_ClickHouse_Result, affectedRows);
ZEND_METHOD(TrueAsync_ClickHouse_Result, summary);
ZEND_METHOD(TrueAsync_ClickHouse_Result, current);
ZEND_METHOD(TrueAsync_ClickHouse_Result, key);
ZEND_METHOD(TrueAsync_ClickHouse_Result, next);
ZEND_METHOD(TrueAsync_ClickHouse_Result, rewind);
ZEND_METHOD(TrueAsync_ClickHouse_Result, valid);
ZEND_METHOD(TrueAsync_ClickHouse_Client, __construct);
ZEND_METHOD(TrueAsync_ClickHouse_Client, query);
ZEND_METHOD(TrueAsync_ClickHouse_Client, insert);
ZEND_METHOD(TrueAsync_ClickHouse_Client, insertBatch);
ZEND_METHOD(TrueAsync_ClickHouse_Client, getPool);

static const zend_function_entry class_TrueAsync_ClickHouse_Batch_methods[] = {
	ZEND_ME(TrueAsync_ClickHouse_Batch, append, arginfo_class_TrueAsync_ClickHouse_Batch_append, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Batch, flush, arginfo_class_TrueAsync_ClickHouse_Batch_flush, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Batch, count, arginfo_class_TrueAsync_ClickHouse_Batch_count, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry class_TrueAsync_ClickHouse_Result_methods[] = {
	ZEND_ME(TrueAsync_ClickHouse_Result, fetch, arginfo_class_TrueAsync_ClickHouse_Result_fetch, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Result, fetchAll, arginfo_class_TrueAsync_ClickHouse_Result_fetchAll, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Result, fetchOne, arginfo_class_TrueAsync_ClickHouse_Result_fetchOne, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Result, affectedRows, arginfo_class_TrueAsync_ClickHouse_Result_affectedRows, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Result, summary, arginfo_class_TrueAsync_ClickHouse_Result_summary, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Result, current, arginfo_class_TrueAsync_ClickHouse_Result_current, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Result, key, arginfo_class_TrueAsync_ClickHouse_Result_key, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Result, next, arginfo_class_TrueAsync_ClickHouse_Result_next, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Result, rewind, arginfo_class_TrueAsync_ClickHouse_Result_rewind, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Result, valid, arginfo_class_TrueAsync_ClickHouse_Result_valid, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static const zend_function_entry class_TrueAsync_ClickHouse_Client_methods[] = {
	ZEND_ME(TrueAsync_ClickHouse_Client, __construct, arginfo_class_TrueAsync_ClickHouse_Client___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Client, query, arginfo_class_TrueAsync_ClickHouse_Client_query, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Client, insert, arginfo_class_TrueAsync_ClickHouse_Client_insert, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Client, insertBatch, arginfo_class_TrueAsync_ClickHouse_Client_insertBatch, ZEND_ACC_PUBLIC)
	ZEND_ME(TrueAsync_ClickHouse_Client, getPool, arginfo_class_TrueAsync_ClickHouse_Client_getPool, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_TrueAsync_ClickHouse_Compression(void)
{
	zend_class_entry *class_entry = zend_register_internal_enum("TrueAsync\\ClickHouse\\Compression", IS_STRING, NULL);

	zval enum_case_None_value;
	zend_string *enum_case_None_value_str = zend_string_init("none", strlen("none"), 1);
	ZVAL_STR(&enum_case_None_value, enum_case_None_value_str);
	zend_enum_add_case_cstr(class_entry, "None", &enum_case_None_value);

	zval enum_case_LZ4_value;
	zend_string *enum_case_LZ4_value_str = zend_string_init("lz4", strlen("lz4"), 1);
	ZVAL_STR(&enum_case_LZ4_value, enum_case_LZ4_value_str);
	zend_enum_add_case_cstr(class_entry, "LZ4", &enum_case_LZ4_value);

	zval enum_case_ZSTD_value;
	zend_string *enum_case_ZSTD_value_str = zend_string_init("zstd", strlen("zstd"), 1);
	ZVAL_STR(&enum_case_ZSTD_value, enum_case_ZSTD_value_str);
	zend_enum_add_case_cstr(class_entry, "ZSTD", &enum_case_ZSTD_value);

	return class_entry;
}

static zend_class_entry *register_class_TrueAsync_ClickHouse_OpenStrategy(void)
{
	zend_class_entry *class_entry = zend_register_internal_enum("TrueAsync\\ClickHouse\\OpenStrategy", IS_STRING, NULL);

	zval enum_case_InOrder_value;
	zend_string *enum_case_InOrder_value_str = zend_string_init("in_order", strlen("in_order"), 1);
	ZVAL_STR(&enum_case_InOrder_value, enum_case_InOrder_value_str);
	zend_enum_add_case_cstr(class_entry, "InOrder", &enum_case_InOrder_value);

	zval enum_case_RoundRobin_value;
	zend_string *enum_case_RoundRobin_value_str = zend_string_init("round_robin", strlen("round_robin"), 1);
	ZVAL_STR(&enum_case_RoundRobin_value, enum_case_RoundRobin_value_str);
	zend_enum_add_case_cstr(class_entry, "RoundRobin", &enum_case_RoundRobin_value);

	zval enum_case_Random_value;
	zend_string *enum_case_Random_value_str = zend_string_init("random", strlen("random"), 1);
	ZVAL_STR(&enum_case_Random_value, enum_case_Random_value_str);
	zend_enum_add_case_cstr(class_entry, "Random", &enum_case_Random_value);

	return class_entry;
}

static zend_class_entry *register_class_TrueAsync_ClickHouse_ClickHouseException(zend_class_entry *class_entry_RuntimeException)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "TrueAsync\\ClickHouse", "ClickHouseException", NULL);
	class_entry = zend_register_internal_class_with_flags(&ce, class_entry_RuntimeException, 0);

	return class_entry;
}

static zend_class_entry *register_class_TrueAsync_ClickHouse_ConnectionException(zend_class_entry *class_entry_TrueAsync_ClickHouse_ClickHouseException)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "TrueAsync\\ClickHouse", "ConnectionException", NULL);
	class_entry = zend_register_internal_class_with_flags(&ce, class_entry_TrueAsync_ClickHouse_ClickHouseException, 0);

	return class_entry;
}

static zend_class_entry *register_class_TrueAsync_ClickHouse_ServerException(zend_class_entry *class_entry_TrueAsync_ClickHouse_ClickHouseException)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "TrueAsync\\ClickHouse", "ServerException", NULL);
	class_entry = zend_register_internal_class_with_flags(&ce, class_entry_TrueAsync_ClickHouse_ClickHouseException, 0);

	return class_entry;
}

static zend_class_entry *register_class_TrueAsync_ClickHouse_ProtocolException(zend_class_entry *class_entry_TrueAsync_ClickHouse_ClickHouseException)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "TrueAsync\\ClickHouse", "ProtocolException", NULL);
	class_entry = zend_register_internal_class_with_flags(&ce, class_entry_TrueAsync_ClickHouse_ClickHouseException, 0);

	return class_entry;
}

static zend_class_entry *register_class_TrueAsync_ClickHouse_Batch(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "TrueAsync\\ClickHouse", "Batch", class_TrueAsync_ClickHouse_Batch_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	return class_entry;
}

static zend_class_entry *register_class_TrueAsync_ClickHouse_Summary(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "TrueAsync\\ClickHouse", "Summary", NULL);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	zval property_readRows_default_value;
	ZVAL_UNDEF(&property_readRows_default_value);
	zend_string *property_readRows_name = zend_string_init("readRows", sizeof("readRows") - 1, true);
	zend_declare_typed_property(class_entry, property_readRows_name, &property_readRows_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release_ex(property_readRows_name, true);

	zval property_readBytes_default_value;
	ZVAL_UNDEF(&property_readBytes_default_value);
	zend_string *property_readBytes_name = zend_string_init("readBytes", sizeof("readBytes") - 1, true);
	zend_declare_typed_property(class_entry, property_readBytes_name, &property_readBytes_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release_ex(property_readBytes_name, true);

	zval property_writtenRows_default_value;
	ZVAL_UNDEF(&property_writtenRows_default_value);
	zend_string *property_writtenRows_name = zend_string_init("writtenRows", sizeof("writtenRows") - 1, true);
	zend_declare_typed_property(class_entry, property_writtenRows_name, &property_writtenRows_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release_ex(property_writtenRows_name, true);

	zval property_writtenBytes_default_value;
	ZVAL_UNDEF(&property_writtenBytes_default_value);
	zend_string *property_writtenBytes_name = zend_string_init("writtenBytes", sizeof("writtenBytes") - 1, true);
	zend_declare_typed_property(class_entry, property_writtenBytes_name, &property_writtenBytes_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release_ex(property_writtenBytes_name, true);

	zval property_totalRowsToRead_default_value;
	ZVAL_UNDEF(&property_totalRowsToRead_default_value);
	zend_string *property_totalRowsToRead_name = zend_string_init("totalRowsToRead", sizeof("totalRowsToRead") - 1, true);
	zend_declare_typed_property(class_entry, property_totalRowsToRead_name, &property_totalRowsToRead_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release_ex(property_totalRowsToRead_name, true);

	zval property_rowsBeforeLimit_default_value;
	ZVAL_UNDEF(&property_rowsBeforeLimit_default_value);
	zend_string *property_rowsBeforeLimit_name = zend_string_init("rowsBeforeLimit", sizeof("rowsBeforeLimit") - 1, true);
	zend_declare_typed_property(class_entry, property_rowsBeforeLimit_name, &property_rowsBeforeLimit_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG|MAY_BE_NULL));
	zend_string_release_ex(property_rowsBeforeLimit_name, true);

	zval property_elapsed_default_value;
	ZVAL_UNDEF(&property_elapsed_default_value);
	zend_string *property_elapsed_name = zend_string_init("elapsed", sizeof("elapsed") - 1, true);
	zend_declare_typed_property(class_entry, property_elapsed_name, &property_elapsed_default_value, ZEND_ACC_PUBLIC|ZEND_ACC_READONLY, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_DOUBLE));
	zend_string_release_ex(property_elapsed_name, true);

	return class_entry;
}

static zend_class_entry *register_class_TrueAsync_ClickHouse_Result(zend_class_entry *class_entry_Iterator)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "TrueAsync\\ClickHouse", "Result", class_TrueAsync_ClickHouse_Result_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);
	zend_class_implements(class_entry, 1, class_entry_Iterator);

	return class_entry;
}

static zend_class_entry *register_class_TrueAsync_ClickHouse_Client(void)
{
	zend_class_entry ce, *class_entry;

	INIT_NS_CLASS_ENTRY(ce, "TrueAsync\\ClickHouse", "Client", class_TrueAsync_ClickHouse_Client_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL);

	return class_entry;
}
