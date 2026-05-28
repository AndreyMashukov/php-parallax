#include "clone.h"

#include <stdlib.h>
#include <string.h>

static kv_t *clone_kv_block(const kv_t *src, size_t n)
{
	if (n == 0) {
		return NULL;
	}
	kv_t *dst = (kv_t *)calloc(n, sizeof(kv_t));
	if (dst == NULL) {
		abort();
	}
	for (size_t i = 0; i < n; i++) {
		dst[i].is_int_key = src[i].is_int_key;
		dst[i].ikey = src[i].ikey;
		dst[i].skey_len = src[i].skey_len;
		if (src[i].skey != NULL && src[i].skey_len > 0) {
			dst[i].skey = (char *)malloc(src[i].skey_len);
			if (dst[i].skey == NULL) { abort(); }
			memcpy(dst[i].skey, src[i].skey, src[i].skey_len);
		}
		dst[i].val = value_clone(src[i].val);
	}
	return dst;
}

value_t *value_clone(const value_t *src)
{
	if (src == NULL) {
		return NULL;
	}
	switch (src->tag) {
		case VAL_NULL:
			return value_null();
		case VAL_BOOL:
			return value_bool(src->as.b);
		case VAL_LONG:
			return value_long(src->as.i);
		case VAL_DOUBLE:
			return value_double(src->as.d);
		case VAL_STR:
			return value_str(src->as.str.p, src->as.str.len);
		case VAL_ARR: {
			value_t *dst = value_arr(0);
			dst->as.arr.items = clone_kv_block(src->as.arr.items, src->as.arr.n);
			dst->as.arr.n = src->as.arr.n;
			dst->as.arr.cap = src->as.arr.n;
			return dst;
		}
		case VAL_OBJ: {
			value_t *dst = value_obj(src->as.obj.cls, src->as.obj.clen, 0);
			dst->as.obj.props = clone_kv_block(src->as.obj.props, src->as.obj.n);
			dst->as.obj.n = src->as.obj.n;
			dst->as.obj.cap = src->as.obj.n;
			return dst;
		}
		case VAL_ERR:
			return value_err(src->as.err.cls, src->as.err.msg, src->as.err.code, src->as.err.trace);
	}
	return NULL;
}
