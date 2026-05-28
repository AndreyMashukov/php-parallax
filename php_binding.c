#include "php_parallax.h"
#include "bridge.h"

#include <Zend/zend_API.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_interfaces.h>

#include <stdlib.h>

ZEND_DECLARE_MODULE_GLOBALS(parallax)

zend_class_entry *parallax_waitgroup_ce      = NULL;
zend_class_entry *parallax_result_ce         = NULL;
zend_class_entry *parallax_worker_error_ce   = NULL;
zend_class_entry *parallax_capture_error_ce  = NULL;
zend_class_entry *parallax_spawn_error_ce    = NULL;

static zend_object_handlers parallax_waitgroup_handlers;

/* ── per-task payload carried via task->user_data ────────────────────────── */

typedef struct {
	px_callable_t callable;
} px_task_payload_t_local; /* mirrors bridge_worker.c's payload struct */

/* ── WaitGroup userland object lifecycle ─────────────────────────────────── */

static zend_object *parallax_waitgroup_create(zend_class_entry *ce)
{
	parallax_waitgroup_object_t *obj =
		(parallax_waitgroup_object_t *)ecalloc(1, sizeof(parallax_waitgroup_object_t) + zend_object_properties_size(ce));
	zend_object_std_init(&obj->std, ce);
	object_properties_init(&obj->std, ce);
	obj->std.handlers = &parallax_waitgroup_handlers;
	obj->wg = NULL;          /* lazily created in __construct */
	return &obj->std;
}

static void parallax_waitgroup_free(zend_object *object)
{
	parallax_waitgroup_object_t *obj = parallax_waitgroup_from_obj(object);
	if (obj->wg != NULL) {
		wg_destroy(obj->wg);
		obj->wg = NULL;
	}
	if (obj->bootstrap != NULL) {
		free(obj->bootstrap);
		obj->bootstrap = NULL;
	}
	zend_object_std_dtor(object);
}

/* ── WaitGroup methods ───────────────────────────────────────────────────── */

PHP_METHOD(parallax_WaitGroup, __construct)
{
	zend_string *bootstrap = NULL;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_STR_OR_NULL(bootstrap)
	ZEND_PARSE_PARAMETERS_END();

	parallax_waitgroup_object_t *self = parallax_waitgroup_from_obj(Z_OBJ_P(getThis()));
	self->wg = wg_create(px_worker_main);
	if (bootstrap != NULL && ZSTR_LEN(bootstrap) > 0) {
		self->bootstrap = (char *)malloc(ZSTR_LEN(bootstrap) + 1);
		if (self->bootstrap == NULL) { abort(); }
		memcpy(self->bootstrap, ZSTR_VAL(bootstrap), ZSTR_LEN(bootstrap));
		self->bootstrap[ZSTR_LEN(bootstrap)] = '\0';
	}
}

PHP_METHOD(parallax_WaitGroup, go)
{
	zval *task = NULL;
	HashTable *args_ht = NULL;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ZVAL(task)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY_HT_OR_NULL(args_ht)
	ZEND_PARSE_PARAMETERS_END();

	parallax_waitgroup_object_t *self = parallax_waitgroup_from_obj(Z_OBJ_P(getThis()));
	if (self->wg == NULL) {
		zend_throw_exception(parallax_spawn_error_ce, "WaitGroup is uninitialised", 0);
		RETURN_THROWS();
	}

	/* 1. Resolve the callable into a thread-portable recipe. */
	px_callable_t cb;
	if (px_resolve_callable(task, &cb) != 0) {
		RETURN_THROWS();
	}

	/* 2. Pack arguments into a VAL_ARR snapshot. */
	value_t *args_val = value_arr(0);
	if (args_ht != NULL) {
		zend_ulong   idx;
		zend_string *key;
		zval        *v;
		ZEND_HASH_FOREACH_KEY_VAL(args_ht, idx, key, v) {
			if (Z_TYPE_P(v) == IS_REFERENCE) {
				value_free(args_val);
				px_callable_free(&cb);
				zend_throw_exception(parallax_capture_error_ce,
					"by-reference argument cannot be captured into a parallax task", 0);
				RETURN_THROWS();
			}
			value_t *child = px_zval_to_value(v);
			if (child == NULL) {
				value_free(args_val);
				px_callable_free(&cb);
				RETURN_THROWS();
			}
			if (key != NULL) {
				value_arr_set_str(args_val, ZSTR_VAL(key), ZSTR_LEN(key), child);
			} else {
				value_arr_set_int(args_val, (int64_t)idx, child);
			}
		} ZEND_HASH_FOREACH_END();
	}

	/* 3. Wrap callable into a malloc'd payload owned by the task. */
	if (self->bootstrap != NULL) {
		size_t blen = strlen(self->bootstrap);
		cb.bootstrap = (char *)malloc(blen + 1);
		if (cb.bootstrap == NULL) { abort(); }
		memcpy(cb.bootstrap, self->bootstrap, blen + 1);
	}
	px_task_payload_t_local *payload =
		(px_task_payload_t_local *)malloc(sizeof(*payload));
	if (payload == NULL) { abort(); }
	payload->callable = cb;
	cb.class_name = NULL;
	cb.fn_name = NULL;
	cb.bootstrap = NULL;

	(void)wg_go(self->wg, args_val, payload);
}

PHP_METHOD(parallax_WaitGroup, wait)
{
	ZEND_PARSE_PARAMETERS_NONE();
	parallax_waitgroup_object_t *self = parallax_waitgroup_from_obj(Z_OBJ_P(getThis()));
	if (self->wg == NULL) {
		zend_throw_exception(parallax_spawn_error_ce, "WaitGroup is uninitialised", 0);
		RETURN_THROWS();
	}

	wg_wait(self->wg);

	size_t n = wg_size(self->wg);
	array_init_size(return_value, (uint32_t)n);
	for (size_t i = 0; i < n; i++) {
		const task_t *t = wg_get(self->wg, i);
		zval slot;
		object_init_ex(&slot, parallax_result_ce);
		zend_update_property_bool(parallax_result_ce, Z_OBJ(slot),
			"ok", sizeof("ok") - 1, t->ok ? 1 : 0);
		if (t->ok && t->result != NULL) {
			zval val_zv;
			px_value_to_zval(t->result, &val_zv);
			zend_update_property(parallax_result_ce, Z_OBJ(slot),
				"value", sizeof("value") - 1, &val_zv);
			zval_ptr_dtor(&val_zv);
		}
		if (!t->ok && t->error != NULL) {
			zval err_zv;
			px_value_to_zval(t->error, &err_zv);
			zend_update_property(parallax_result_ce, Z_OBJ(slot),
				"error", sizeof("error") - 1, &err_zv);
			zval_ptr_dtor(&err_zv);
		}
		add_next_index_zval(return_value, &slot);
	}
}

PHP_METHOD(parallax_WaitGroup, count)
{
	ZEND_PARSE_PARAMETERS_NONE();
	parallax_waitgroup_object_t *self = parallax_waitgroup_from_obj(Z_OBJ_P(getThis()));
	if (self->wg == NULL) {
		RETURN_LONG(0);
	}
	RETURN_LONG((zend_long)wg_size(self->wg));
}

/* ── arginfo ─────────────────────────────────────────────────────────────── */

ZEND_BEGIN_ARG_INFO_EX(arginfo_waitgroup_ctor, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, bootstrap, IS_STRING, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_waitgroup_go, 0, 0, 1)
	ZEND_ARG_TYPE_MASK(0, task, MAY_BE_STRING|MAY_BE_ARRAY|MAY_BE_OBJECT, NULL)
	ZEND_ARG_TYPE_INFO(0, args, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_waitgroup_wait, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_waitgroup_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry parallax_waitgroup_methods[] = {
	PHP_ME(parallax_WaitGroup, __construct, arginfo_waitgroup_ctor,  ZEND_ACC_PUBLIC)
	PHP_ME(parallax_WaitGroup, go,          arginfo_waitgroup_go,    ZEND_ACC_PUBLIC)
	PHP_ME(parallax_WaitGroup, wait,        arginfo_waitgroup_wait,  ZEND_ACC_PUBLIC)
	PHP_ME(parallax_WaitGroup, count,       arginfo_waitgroup_count, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/* ── class registration ──────────────────────────────────────────────────── */

static void register_waitgroup(void)
{
	zend_class_entry tmp;
	INIT_CLASS_ENTRY(tmp, "WaitGroup", parallax_waitgroup_methods);
	parallax_waitgroup_ce = zend_register_internal_class(&tmp);
	parallax_waitgroup_ce->create_object = parallax_waitgroup_create;
	parallax_waitgroup_ce->ce_flags |= ZEND_ACC_FINAL;

	memcpy(&parallax_waitgroup_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	parallax_waitgroup_handlers.offset = XtOffsetOf(parallax_waitgroup_object_t, std);
	parallax_waitgroup_handlers.free_obj = parallax_waitgroup_free;
}

static void register_result(void)
{
	zend_class_entry tmp;
	INIT_CLASS_ENTRY(tmp, "ParallaxResult", NULL);
	parallax_result_ce = zend_register_internal_class(&tmp);
	parallax_result_ce->ce_flags |= ZEND_ACC_FINAL;

	zval default_false; ZVAL_FALSE(&default_false);
	zval default_null;  ZVAL_NULL(&default_null);
	zend_declare_property(parallax_result_ce, "ok",    sizeof("ok") - 1,    &default_false, ZEND_ACC_PUBLIC);
	zend_declare_property(parallax_result_ce, "value", sizeof("value") - 1, &default_null,  ZEND_ACC_PUBLIC);
	zend_declare_property(parallax_result_ce, "error", sizeof("error") - 1, &default_null,  ZEND_ACC_PUBLIC);
}

static void register_worker_error(void)
{
	zend_class_entry tmp;
	INIT_CLASS_ENTRY(tmp, "ParallaxWorkerError", NULL);
	parallax_worker_error_ce = zend_register_internal_class(&tmp);
	parallax_worker_error_ce->ce_flags |= ZEND_ACC_FINAL;

	zval empty_str; ZVAL_EMPTY_STRING(&empty_str);
	zval zero_long; ZVAL_LONG(&zero_long, 0);
	zend_declare_property(parallax_worker_error_ce, "class",   sizeof("class") - 1,   &empty_str, ZEND_ACC_PUBLIC);
	zend_declare_property(parallax_worker_error_ce, "message", sizeof("message") - 1, &empty_str, ZEND_ACC_PUBLIC);
	zend_declare_property(parallax_worker_error_ce, "code",    sizeof("code") - 1,    &zero_long, ZEND_ACC_PUBLIC);
	zend_declare_property(parallax_worker_error_ce, "trace",   sizeof("trace") - 1,   &empty_str, ZEND_ACC_PUBLIC);
}

static void register_exceptions(void)
{
	zend_class_entry tmp1;
	INIT_CLASS_ENTRY(tmp1, "CaptureError", NULL);
	parallax_capture_error_ce = zend_register_internal_class_ex(&tmp1, zend_ce_error);

	zend_class_entry tmp2;
	INIT_CLASS_ENTRY(tmp2, "SpawnError", NULL);
	parallax_spawn_error_ce = zend_register_internal_class_ex(&tmp2, zend_ce_error);
}

/* ── module entry ────────────────────────────────────────────────────────── */

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("parallax.max_workers",     "0",    PHP_INI_SYSTEM, OnUpdateLong, max_workers,     zend_parallax_globals, parallax_globals)
	STD_PHP_INI_ENTRY("parallax.worker_stack_kb", "8192", PHP_INI_SYSTEM, OnUpdateLong, worker_stack_kb, zend_parallax_globals, parallax_globals)
PHP_INI_END()

PHP_MINIT_FUNCTION(parallax)
{
	REGISTER_INI_ENTRIES();
	register_waitgroup();
	register_result();
	register_worker_error();
	register_exceptions();
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(parallax)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_RINIT_FUNCTION(parallax)
{
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(parallax)
{
	return SUCCESS;
}

PHP_MINFO_FUNCTION(parallax)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "parallax support", "enabled");
	php_info_print_table_row(2, "version", PHP_PARALLAX_VERSION);
#ifdef ZTS
	php_info_print_table_row(2, "thread safety", "yes (ZTS)");
#else
	php_info_print_table_row(2, "thread safety", "NO");
#endif
	php_info_print_table_end();
	DISPLAY_INI_ENTRIES();
}

PHP_GINIT_FUNCTION(parallax)
{
	parallax_globals->max_workers = 0;
	parallax_globals->worker_stack_kb = 8192;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_internal_capture, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, ex, Throwable, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(parallax_internal_capture_exception);

static const zend_function_entry parallax_functions[] = {
	PHP_FE(parallax_internal_capture_exception, arginfo_internal_capture)
	PHP_FE_END
};

zend_module_entry parallax_module_entry = {
	STANDARD_MODULE_HEADER,
	"parallax",
	parallax_functions,
	PHP_MINIT(parallax),
	PHP_MSHUTDOWN(parallax),
	PHP_RINIT(parallax),
	PHP_RSHUTDOWN(parallax),
	PHP_MINFO(parallax),
	PHP_PARALLAX_VERSION,
	PHP_MODULE_GLOBALS(parallax),
	PHP_GINIT(parallax),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_PARALLAX
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(parallax)
#endif
