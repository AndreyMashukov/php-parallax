#include "bridge.h"
#include "bridge_compat.h"

#include <Zend/zend_API.h>
#include <Zend/zend_closures.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_smart_str.h>

#include <stdlib.h>
#include <string.h>

/* ── callable helpers ────────────────────────────────────────────────────── */

void px_callable_free(px_callable_t *c)
{
	if (c == NULL) {
		return;
	}
	free(c->class_name);
	free(c->fn_name);
	free(c->bootstrap);
	c->class_name = NULL;
	c->fn_name = NULL;
	c->bootstrap = NULL;
}

static char *strdup_or_die(const char *src, size_t len)
{
	char *p = (char *)malloc(len + 1);
	if (p == NULL) { abort(); }
	memcpy(p, src, len);
	p[len] = '\0';
	return p;
}

static void throw_capture_error(const char *fmt, ...)
{
	char msg[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	zend_throw_exception(parallax_capture_error_ce, msg, 0);
}

static void throw_spawn_error(const char *fmt, ...)
{
	char msg[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	zend_throw_exception(parallax_spawn_error_ce, msg, 0);
}

int px_resolve_callable(zval *callable, px_callable_t *out)
{
	memset(out, 0, sizeof(*out));

	/* string: either "fname" or "Cls::method" */
	if (Z_TYPE_P(callable) == IS_STRING) {
		const char *s = Z_STRVAL_P(callable);
		size_t len = Z_STRLEN_P(callable);
		const char *sep = memchr(s, ':', len);
		if (sep != NULL && (sep + 1) < (s + len) && sep[1] == ':') {
			size_t cls_len = (size_t)(sep - s);
			size_t fn_off  = (size_t)((sep - s) + 2);
			out->kind = PX_CALL_KIND_STATIC_METH;
			out->class_name = strdup_or_die(s, cls_len);
			out->fn_name = strdup_or_die(s + fn_off, len - fn_off);
		} else {
			out->kind = PX_CALL_KIND_FUNCTION;
			out->fn_name = strdup_or_die(s, len);
		}
		return 0;
	}

	/* array: [class|object, "method"] */
	if (Z_TYPE_P(callable) == IS_ARRAY) {
		HashTable *ht = Z_ARRVAL_P(callable);
		if (zend_hash_num_elements(ht) != 2) {
			throw_spawn_error("callable array must have exactly 2 elements");
			return -1;
		}
		zval *first  = zend_hash_index_find(ht, 0);
		zval *second = zend_hash_index_find(ht, 1);
		if (first == NULL || second == NULL || Z_TYPE_P(second) != IS_STRING) {
			throw_spawn_error("callable array must be [class|object, methodName]");
			return -1;
		}
		if (Z_TYPE_P(first) == IS_STRING) {
			out->kind = PX_CALL_KIND_STATIC_METH;
			out->class_name = strdup_or_die(Z_STRVAL_P(first), Z_STRLEN_P(first));
			out->fn_name = strdup_or_die(Z_STRVAL_P(second), Z_STRLEN_P(second));
			return 0;
		}
		throw_capture_error("instance methods are not transferable; pass [ClassName::class, 'method']");
		return -1;
	}

	/* Closure — accept only first-class-callable form for v1; reject inline. */
	if (Z_TYPE_P(callable) == IS_OBJECT && instanceof_function(Z_OBJCE_P(callable), zend_ce_closure)) {
		const zend_function *fn = zend_get_closure_method_def(Z_OBJ_P(callable));
		if (fn == NULL) {
			throw_spawn_error("could not introspect Closure");
			return -1;
		}
		/* First-class callable wraps a named function or method without bound use(...).
		 * Inline closures carry ZEND_ACC_FAKE_CLOSURE off and may have bound variables. */
		if (!(fn->common.fn_flags & ZEND_ACC_FAKE_CLOSURE)) {
			throw_capture_error("inline closures are not supported in v1; use first-class callable syntax Cls::method(...)");
			return -1;
		}
		zend_string *name = fn->common.function_name;
		zend_class_entry *scope = fn->common.scope;
		if (scope != NULL) {
			out->kind = PX_CALL_KIND_STATIC_METH;
			out->class_name = strdup_or_die(ZSTR_VAL(scope->name), ZSTR_LEN(scope->name));
			out->fn_name = strdup_or_die(ZSTR_VAL(name), ZSTR_LEN(name));
		} else {
			out->kind = PX_CALL_KIND_FUNCTION;
			out->fn_name = strdup_or_die(ZSTR_VAL(name), ZSTR_LEN(name));
		}
		return 0;
	}

	throw_spawn_error("unsupported callable type for parallax");
	return -1;
}

/* ── zval → value_t (snapshot capture) ───────────────────────────────────── */

static value_t *zval_to_value_inner(zval *zv, int depth);

static value_t *array_to_value(zval *zv, int depth)
{
	HashTable *ht = Z_ARRVAL_P(zv);
	value_t *arr = value_arr(zend_hash_num_elements(ht));

	zend_ulong   idx;
	zend_string *key;
	zval        *val;
	ZEND_HASH_FOREACH_KEY_VAL(ht, idx, key, val) {
		if (Z_TYPE_P(val) == IS_REFERENCE) {
			value_free(arr);
			zend_throw_exception(parallax_capture_error_ce,
				"by-reference array element cannot be captured into a parallax task", 0);
			return NULL;
		}
		value_t *child = zval_to_value_inner(val, depth + 1);
		if (child == NULL) {
			value_free(arr);
			return NULL;
		}
		if (key != NULL) {
			value_arr_set_str(arr, ZSTR_VAL(key), ZSTR_LEN(key), child);
		} else {
			value_arr_set_int(arr, (int64_t)idx, child);
		}
	} ZEND_HASH_FOREACH_END();

	return arr;
}

static value_t *object_to_value(zval *zv, int depth)
{
	zend_object *obj = Z_OBJ_P(zv);
	zend_class_entry *ce = obj->ce;

	/* Reject Closures embedded as captures — too easy to smuggle by-ref state through. */
	if (instanceof_function(ce, zend_ce_closure)) {
		throw_capture_error("nested closure cannot be captured into a parallax task");
		return NULL;
	}
	/* Reject any object that ships a custom create_object / clone_obj — those bind
	 * non-portable native state (PDO, Reflection, Generator, ...). Engine-default
	 * objects all share zend_objects_clone_obj. */
	if (ce->create_object != NULL && ce->create_object != zend_objects_new) {
		throw_capture_error("object of class %s carries non-transferable native state; pass a recipe (data) and reconstruct inside the worker", ZSTR_VAL(ce->name));
		return NULL;
	}

	value_t *obj_v = value_obj(ZSTR_VAL(ce->name), ZSTR_LEN(ce->name), 0);

	zend_property_info *prop_info;
	ZEND_HASH_FOREACH_PTR(&ce->properties_info, prop_info) {
		if ((prop_info->flags & ZEND_ACC_STATIC) != 0) {
			continue;
		}
		zval *slot = OBJ_PROP(obj, prop_info->offset);
		if (Z_TYPE_P(slot) == IS_UNDEF) {
			continue;
		}
		ZVAL_DEREF(slot);
		value_t *child = zval_to_value_inner(slot, depth + 1);
		if (child == NULL) {
			value_free(obj_v);
			return NULL;
		}
		zend_string *name = prop_info->name;
		value_obj_set(obj_v, ZSTR_VAL(name), ZSTR_LEN(name), child);
	} ZEND_HASH_FOREACH_END();

	return obj_v;
}

static value_t *zval_to_value_inner(zval *zv, int depth)
{
	if (depth > 256) {
		throw_capture_error("capture exceeds maximum depth of 256");
		return NULL;
	}
	switch (Z_TYPE_P(zv)) {
		case IS_NULL:      return value_null();
		case IS_TRUE:      return value_bool(true);
		case IS_FALSE:     return value_bool(false);
		case IS_LONG:      return value_long((int64_t)Z_LVAL_P(zv));
		case IS_DOUBLE:    return value_double(Z_DVAL_P(zv));
		case IS_STRING:    return value_str(Z_STRVAL_P(zv), Z_STRLEN_P(zv));
		case IS_ARRAY:     return array_to_value(zv, depth);
		case IS_OBJECT:    return object_to_value(zv, depth);
		case IS_RESOURCE:
			throw_capture_error("resource cannot be captured into a parallax task");
			return NULL;
		case IS_REFERENCE:
			throw_capture_error("by-reference capture is not allowed in a parallax task");
			return NULL;
		default:
			throw_capture_error("unsupported zval type %d", (int)Z_TYPE_P(zv));
			return NULL;
	}
}

value_t *px_zval_to_value(zval *zv)
{
	return zval_to_value_inner(zv, 0);
}

/* ── value_t → zval (worker → main, or args into worker) ─────────────────── */

static void value_to_zval_inner(const value_t *v, zval *out);

static void kv_block_to_array(const kv_t *items, size_t n, zval *target)
{
	array_init_size(target, (uint32_t)n);
	for (size_t i = 0; i < n; i++) {
		const kv_t *kv = &items[i];
		zval child;
		value_to_zval_inner(kv->val, &child);
		if (kv->is_int_key) {
			add_index_zval(target, (zend_ulong)kv->ikey, &child);
		} else {
			add_assoc_zval_ex(target, kv->skey, kv->skey_len, &child);
		}
	}
}

static void kv_block_to_object(const kv_t *items, size_t n, zval *target, zend_class_entry *ce)
{
	object_init_ex(target, ce);
	for (size_t i = 0; i < n; i++) {
		const kv_t *kv = &items[i];
		zval child;
		value_to_zval_inner(kv->val, &child);
		zend_update_property_ex(ce, Z_OBJ_P(target),
			zend_string_init(kv->skey, kv->skey_len, 0), &child);
		zval_ptr_dtor(&child);
	}
}

static void value_to_zval_inner(const value_t *v, zval *out)
{
	if (v == NULL) {
		ZVAL_NULL(out);
		return;
	}
	switch (v->tag) {
		case VAL_NULL:   ZVAL_NULL(out); return;
		case VAL_BOOL:   ZVAL_BOOL(out, v->as.b ? 1 : 0); return;
		case VAL_LONG:   ZVAL_LONG(out, (zend_long)v->as.i); return;
		case VAL_DOUBLE: ZVAL_DOUBLE(out, v->as.d); return;
		case VAL_STR:
			ZVAL_STRINGL(out, v->as.str.p ? v->as.str.p : "", v->as.str.len);
			return;
		case VAL_ARR:
			kv_block_to_array(v->as.arr.items, v->as.arr.n, out);
			return;
		case VAL_OBJ: {
			zend_class_entry *ce = zend_lookup_class(zend_string_init(v->as.obj.cls, v->as.obj.clen, 0));
			if (ce == NULL) {
				/* Class missing in the destination scope — fall back to stdClass with the same props. */
				ce = zend_standard_class_def;
			}
			kv_block_to_object(v->as.obj.props, v->as.obj.n, out, ce);
			return;
		}
		case VAL_ERR: {
			/* Materialised as a WorkerError DTO in the receiving scope. */
			object_init_ex(out, parallax_worker_error_ce);
			zend_update_property_string(parallax_worker_error_ce, Z_OBJ_P(out),
				"class", sizeof("class") - 1, v->as.err.cls ? v->as.err.cls : "");
			zend_update_property_string(parallax_worker_error_ce, Z_OBJ_P(out),
				"message", sizeof("message") - 1, v->as.err.msg ? v->as.err.msg : "");
			zend_update_property_long(parallax_worker_error_ce, Z_OBJ_P(out),
				"code", sizeof("code") - 1, (zend_long)v->as.err.code);
			zend_update_property_string(parallax_worker_error_ce, Z_OBJ_P(out),
				"trace", sizeof("trace") - 1, v->as.err.trace ? v->as.err.trace : "");
			return;
		}
	}
	ZVAL_NULL(out);
}

void px_value_to_zval(const value_t *v, zval *out)
{
	value_to_zval_inner(v, out);
}
