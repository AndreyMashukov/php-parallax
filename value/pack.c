#include "pack.h"

#include <stdlib.h>
#include <string.h>

/* ── varint encoding ─────────────────────────────────────────────────────── */

static void varint_write(uint8_t **p, uint64_t v)
{
	while (v >= 0x80) {
		*(*p)++ = (uint8_t)(v | 0x80);
		v >>= 7;
	}
	*(*p)++ = (uint8_t)v;
}

static int varint_read(const uint8_t **p, const uint8_t *end, uint64_t *out)
{
	uint64_t v = 0;
	int shift = 0;
	while (*p < end) {
		uint8_t byte = *(*p)++;
		v |= ((uint64_t)(byte & 0x7f)) << shift;
		if ((byte & 0x80) == 0) {
			*out = v;
			return 0;
		}
		shift += 7;
		if (shift >= 64) {
			return -1;
		}
	}
	return -1;
}

/* ── growable write buffer ───────────────────────────────────────────────── */

typedef struct {
	uint8_t *data;
	size_t   len;
	size_t   cap;
} wbuf_t;

static void wbuf_init(wbuf_t *b)
{
	b->data = NULL;
	b->len = 0;
	b->cap = 0;
}

static void wbuf_reserve(wbuf_t *b, size_t extra)
{
	if (b->len + extra <= b->cap) {
		return;
	}
	size_t new_cap = (b->cap == 0) ? 64 : (b->cap * 2);
	while (new_cap < b->len + extra) {
		new_cap *= 2;
	}
	uint8_t *new_data = (uint8_t *)realloc(b->data, new_cap);
	if (new_data == NULL) {
		abort();
	}
	b->data = new_data;
	b->cap = new_cap;
}

static void wbuf_u8(wbuf_t *b, uint8_t v)
{
	wbuf_reserve(b, 1);
	b->data[b->len++] = v;
}

static void wbuf_bytes(wbuf_t *b, const void *p, size_t n)
{
	wbuf_reserve(b, n);
	if (n > 0) {
		memcpy(b->data + b->len, p, n);
		b->len += n;
	}
}

static void wbuf_varint(wbuf_t *b, uint64_t v)
{
	wbuf_reserve(b, 10);
	uint8_t *p = b->data + b->len;
	uint8_t *start = p;
	varint_write(&p, v);
	b->len += (size_t)(p - start);
}

/* ── doubles: little-endian IEEE 754 ─────────────────────────────────────── */

static void wbuf_double(wbuf_t *b, double d)
{
	uint64_t bits;
	memcpy(&bits, &d, 8);
	uint8_t out[8];
	for (int i = 0; i < 8; i++) {
		out[i] = (uint8_t)(bits >> (i * 8));
	}
	wbuf_bytes(b, out, 8);
}

static int read_double(const uint8_t **p, const uint8_t *end, double *out)
{
	if (end - *p < 8) {
		return -1;
	}
	uint64_t bits = 0;
	for (int i = 0; i < 8; i++) {
		bits |= ((uint64_t)(*p)[i]) << (i * 8);
	}
	*p += 8;
	memcpy(out, &bits, 8);
	return 0;
}

/* ── recursive pack ──────────────────────────────────────────────────────── */

static void pack_one(wbuf_t *b, const value_t *v);

static void pack_kv_block(wbuf_t *b, const kv_t *items, size_t n)
{
	wbuf_varint(b, (uint64_t)n);
	for (size_t i = 0; i < n; i++) {
		const kv_t *kv = &items[i];
		wbuf_u8(b, kv->is_int_key ? 1u : 0u);
		if (kv->is_int_key) {
			wbuf_varint(b, (uint64_t)kv->ikey);
		} else {
			wbuf_varint(b, (uint64_t)kv->skey_len);
			wbuf_bytes(b, kv->skey, kv->skey_len);
		}
		pack_one(b, kv->val);
	}
}

static void pack_one(wbuf_t *b, const value_t *v)
{
	if (v == NULL) {
		wbuf_u8(b, (uint8_t)VAL_NULL);
		return;
	}
	wbuf_u8(b, (uint8_t)v->tag);
	switch (v->tag) {
		case VAL_NULL:
			break;
		case VAL_BOOL:
			wbuf_u8(b, v->as.b ? 1u : 0u);
			break;
		case VAL_LONG:
			wbuf_varint(b, (uint64_t)v->as.i);
			break;
		case VAL_DOUBLE:
			wbuf_double(b, v->as.d);
			break;
		case VAL_STR:
			wbuf_varint(b, (uint64_t)v->as.str.len);
			wbuf_bytes(b, v->as.str.p, v->as.str.len);
			break;
		case VAL_ARR:
			pack_kv_block(b, v->as.arr.items, v->as.arr.n);
			break;
		case VAL_OBJ:
			wbuf_varint(b, (uint64_t)v->as.obj.clen);
			wbuf_bytes(b, v->as.obj.cls, v->as.obj.clen);
			pack_kv_block(b, v->as.obj.props, v->as.obj.n);
			break;
		case VAL_ERR: {
			size_t cls_len = v->as.err.cls ? strlen(v->as.err.cls) : 0;
			size_t msg_len = v->as.err.msg ? strlen(v->as.err.msg) : 0;
			size_t tr_len  = v->as.err.trace ? strlen(v->as.err.trace) : 0;
			wbuf_varint(b, (uint64_t)cls_len);
			wbuf_bytes(b, v->as.err.cls, cls_len);
			wbuf_varint(b, (uint64_t)msg_len);
			wbuf_bytes(b, v->as.err.msg, msg_len);
			wbuf_varint(b, (uint64_t)v->as.err.code);
			wbuf_varint(b, (uint64_t)tr_len);
			wbuf_bytes(b, v->as.err.trace, tr_len);
			break;
		}
	}
}

int value_pack(const value_t *v, uint8_t **out_buf, size_t *out_len)
{
	wbuf_t b;
	wbuf_init(&b);
	pack_one(&b, v);
	*out_buf = b.data;
	*out_len = b.len;
	return 0;
}

/* ── recursive unpack ────────────────────────────────────────────────────── */

static value_t *unpack_one(const uint8_t **p, const uint8_t *end);

static int unpack_kv_block(const uint8_t **p, const uint8_t *end, value_t *parent, bool is_arr)
{
	uint64_t n = 0;
	if (varint_read(p, end, &n) != 0) {
		return -1;
	}
	for (uint64_t i = 0; i < n; i++) {
		if (*p >= end) {
			return -1;
		}
		uint8_t is_int_key = *(*p)++;
		bool int_key = (is_int_key != 0);
		int64_t ikey = 0;
		char *skey = NULL;
		uint64_t klen = 0;
		if (int_key) {
			uint64_t k = 0;
			if (varint_read(p, end, &k) != 0) {
				return -1;
			}
			ikey = (int64_t)k;
		} else {
			if (varint_read(p, end, &klen) != 0) {
				return -1;
			}
			if ((uint64_t)(end - *p) < klen) {
				return -1;
			}
			skey = (char *)malloc(klen ? klen : 1);
			if (skey == NULL) {
				abort();
			}
			if (klen > 0) {
				memcpy(skey, *p, klen);
				*p += klen;
			}
		}
		value_t *child = unpack_one(p, end);
		if (child == NULL) {
			free(skey);
			return -1;
		}
		if (is_arr) {
			if (int_key) {
				value_arr_set_int(parent, ikey, child);
			} else {
				value_arr_set_str(parent, skey, (size_t)klen, child);
				free(skey);
			}
		} else {
			value_obj_set(parent, skey, (size_t)klen, child);
			free(skey);
		}
	}
	return 0;
}

static value_t *unpack_one(const uint8_t **p, const uint8_t *end)
{
	if (*p >= end) {
		return NULL;
	}
	val_tag tag = (val_tag)*(*p)++;
	switch (tag) {
		case VAL_NULL:
			return value_null();
		case VAL_BOOL: {
			if (*p >= end) {
				return NULL;
			}
			uint8_t b = *(*p)++;
			return value_bool(b != 0);
		}
		case VAL_LONG: {
			uint64_t i = 0;
			if (varint_read(p, end, &i) != 0) {
				return NULL;
			}
			return value_long((int64_t)i);
		}
		case VAL_DOUBLE: {
			double d;
			if (read_double(p, end, &d) != 0) {
				return NULL;
			}
			return value_double(d);
		}
		case VAL_STR: {
			uint64_t slen = 0;
			if (varint_read(p, end, &slen) != 0) {
				return NULL;
			}
			if ((uint64_t)(end - *p) < slen) {
				return NULL;
			}
			value_t *v = value_str((const char *)*p, (size_t)slen);
			*p += slen;
			return v;
		}
		case VAL_ARR: {
			value_t *arr = value_arr(0);
			if (unpack_kv_block(p, end, arr, true) != 0) {
				value_free(arr);
				return NULL;
			}
			return arr;
		}
		case VAL_OBJ: {
			uint64_t clen = 0;
			if (varint_read(p, end, &clen) != 0) {
				return NULL;
			}
			if ((uint64_t)(end - *p) < clen) {
				return NULL;
			}
			value_t *obj = value_obj((const char *)*p, (size_t)clen, 0);
			*p += clen;
			if (unpack_kv_block(p, end, obj, false) != 0) {
				value_free(obj);
				return NULL;
			}
			return obj;
		}
		case VAL_ERR: {
			uint64_t cls_len = 0, msg_len = 0, tr_len = 0, code = 0;
			if (varint_read(p, end, &cls_len) != 0 || (uint64_t)(end - *p) < cls_len) {
				return NULL;
			}
			char *cls = (char *)malloc(cls_len + 1);
			if (cls == NULL) { abort(); }
			memcpy(cls, *p, cls_len);
			cls[cls_len] = '\0';
			*p += cls_len;

			if (varint_read(p, end, &msg_len) != 0 || (uint64_t)(end - *p) < msg_len) {
				free(cls);
				return NULL;
			}
			char *msg = (char *)malloc(msg_len + 1);
			if (msg == NULL) { abort(); }
			memcpy(msg, *p, msg_len);
			msg[msg_len] = '\0';
			*p += msg_len;

			if (varint_read(p, end, &code) != 0) {
				free(cls); free(msg);
				return NULL;
			}

			if (varint_read(p, end, &tr_len) != 0 || (uint64_t)(end - *p) < tr_len) {
				free(cls); free(msg);
				return NULL;
			}
			char *tr = (char *)malloc(tr_len + 1);
			if (tr == NULL) { abort(); }
			memcpy(tr, *p, tr_len);
			tr[tr_len] = '\0';
			*p += tr_len;

			value_t *v = value_err(cls, msg, (int64_t)code, tr);
			free(cls); free(msg); free(tr);
			return v;
		}
	}
	return NULL;
}

value_t *value_unpack(const uint8_t *buf, size_t len)
{
	const uint8_t *p = buf;
	const uint8_t *end = buf + len;
	value_t *v = unpack_one(&p, end);
	if (v == NULL) {
		return NULL;
	}
	if (p != end) {
		value_free(v);
		return NULL;
	}
	return v;
}

