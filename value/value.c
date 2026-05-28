#include "value.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static char *dup_bytes(const char *src, size_t len)
{
	if (len == 0) {
		return NULL;
	}
	char *p = (char *)malloc(len);
	if (p == NULL) {
		abort();
	}
	memcpy(p, src, len);
	return p;
}

static char *dup_cstr(const char *src)
{
	if (src == NULL) {
		return NULL;
	}
	size_t len = strlen(src);
	char *p = (char *)malloc(len + 1);
	if (p == NULL) {
		abort();
	}
	memcpy(p, src, len + 1);
	return p;
}

static value_t *alloc_value(val_tag tag)
{
	value_t *v = (value_t *)calloc(1, sizeof(*v));
	if (v == NULL) {
		abort();
	}
	v->tag = tag;
	return v;
}

value_t *value_null(void)
{
	return alloc_value(VAL_NULL);
}

value_t *value_bool(bool b)
{
	value_t *v = alloc_value(VAL_BOOL);
	v->as.b = b;
	return v;
}

value_t *value_long(int64_t i)
{
	value_t *v = alloc_value(VAL_LONG);
	v->as.i = i;
	return v;
}

value_t *value_double(double d)
{
	value_t *v = alloc_value(VAL_DOUBLE);
	v->as.d = d;
	return v;
}

value_t *value_str(const char *p, size_t len)
{
	value_t *v = alloc_value(VAL_STR);
	v->as.str.p = dup_bytes(p, len);
	v->as.str.len = len;
	return v;
}

value_t *value_arr(size_t initial_cap)
{
	value_t *v = alloc_value(VAL_ARR);
	if (initial_cap > 0) {
		v->as.arr.items = (kv_t *)calloc(initial_cap, sizeof(kv_t));
		if (v->as.arr.items == NULL) {
			abort();
		}
		v->as.arr.cap = initial_cap;
	}
	return v;
}

value_t *value_obj(const char *cls, size_t clen, size_t initial_cap)
{
	value_t *v = alloc_value(VAL_OBJ);
	v->as.obj.cls = dup_bytes(cls, clen);
	v->as.obj.clen = clen;
	if (initial_cap > 0) {
		v->as.obj.props = (kv_t *)calloc(initial_cap, sizeof(kv_t));
		if (v->as.obj.props == NULL) {
			abort();
		}
		v->as.obj.cap = initial_cap;
	}
	return v;
}

value_t *value_err(const char *cls, const char *msg, int64_t code, const char *trace)
{
	value_t *v = alloc_value(VAL_ERR);
	v->as.err.cls = dup_cstr(cls);
	v->as.err.msg = dup_cstr(msg);
	v->as.err.code = code;
	v->as.err.trace = dup_cstr(trace);
	return v;
}

static void grow(kv_t **items, size_t *cap)
{
	size_t new_cap = (*cap == 0) ? 4 : (*cap * 2);
	kv_t *new_items = (kv_t *)realloc(*items, new_cap * sizeof(kv_t));
	if (new_items == NULL) {
		abort();
	}
	memset(new_items + *cap, 0, (new_cap - *cap) * sizeof(kv_t));
	*items = new_items;
	*cap = new_cap;
}

static int64_t next_int_key(const value_t *arr)
{
	int64_t max = -1;
	for (size_t i = 0; i < arr->as.arr.n; i++) {
		const kv_t *kv = &arr->as.arr.items[i];
		if (kv->is_int_key && kv->ikey > max) {
			max = kv->ikey;
		}
	}
	return max + 1;
}

void value_arr_push(value_t *arr, value_t *child)
{
	assert(arr->tag == VAL_ARR);
	int64_t key = next_int_key(arr);
	if (arr->as.arr.n == arr->as.arr.cap) {
		grow(&arr->as.arr.items, &arr->as.arr.cap);
	}
	kv_t *kv = &arr->as.arr.items[arr->as.arr.n++];
	kv->is_int_key = true;
	kv->ikey = key;
	kv->skey = NULL;
	kv->skey_len = 0;
	kv->val = child;
}

void value_arr_set_int(value_t *arr, int64_t key, value_t *child)
{
	assert(arr->tag == VAL_ARR);
	if (arr->as.arr.n == arr->as.arr.cap) {
		grow(&arr->as.arr.items, &arr->as.arr.cap);
	}
	kv_t *kv = &arr->as.arr.items[arr->as.arr.n++];
	kv->is_int_key = true;
	kv->ikey = key;
	kv->skey = NULL;
	kv->skey_len = 0;
	kv->val = child;
}

void value_arr_set_str(value_t *arr, const char *key, size_t klen, value_t *child)
{
	assert(arr->tag == VAL_ARR);
	if (arr->as.arr.n == arr->as.arr.cap) {
		grow(&arr->as.arr.items, &arr->as.arr.cap);
	}
	kv_t *kv = &arr->as.arr.items[arr->as.arr.n++];
	kv->is_int_key = false;
	kv->ikey = 0;
	kv->skey = dup_bytes(key, klen);
	kv->skey_len = klen;
	kv->val = child;
}

void value_obj_set(value_t *obj, const char *key, size_t klen, value_t *child)
{
	assert(obj->tag == VAL_OBJ);
	if (obj->as.obj.n == obj->as.obj.cap) {
		grow(&obj->as.obj.props, &obj->as.obj.cap);
	}
	kv_t *kv = &obj->as.obj.props[obj->as.obj.n++];
	kv->is_int_key = false;
	kv->ikey = 0;
	kv->skey = dup_bytes(key, klen);
	kv->skey_len = klen;
	kv->val = child;
}

const value_t *value_arr_get_str(const value_t *arr, const char *key, size_t klen)
{
	if (arr == NULL || arr->tag != VAL_ARR) {
		return NULL;
	}
	for (size_t i = 0; i < arr->as.arr.n; i++) {
		const kv_t *kv = &arr->as.arr.items[i];
		if (kv->is_int_key) {
			continue;
		}
		if (kv->skey_len == klen && memcmp(kv->skey, key, klen) == 0) {
			return kv->val;
		}
	}
	return NULL;
}

static void free_kv_array(kv_t *items, size_t n)
{
	if (items == NULL) {
		return;
	}
	for (size_t i = 0; i < n; i++) {
		free(items[i].skey);
		value_free(items[i].val);
	}
	free(items);
}

void value_free(value_t *v)
{
	if (v == NULL) {
		return;
	}
	switch (v->tag) {
		case VAL_NULL:
		case VAL_BOOL:
		case VAL_LONG:
		case VAL_DOUBLE:
			break;
		case VAL_STR:
			free(v->as.str.p);
			break;
		case VAL_ARR:
			free_kv_array(v->as.arr.items, v->as.arr.n);
			break;
		case VAL_OBJ:
			free(v->as.obj.cls);
			free_kv_array(v->as.obj.props, v->as.obj.n);
			break;
		case VAL_ERR:
			free(v->as.err.cls);
			free(v->as.err.msg);
			free(v->as.err.trace);
			break;
	}
	free(v);
}
