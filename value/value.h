#ifndef PARALLAX_VALUE_H
#define PARALLAX_VALUE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	VAL_NULL   = 0,
	VAL_BOOL   = 1,
	VAL_LONG   = 2,
	VAL_DOUBLE = 3,
	VAL_STR    = 4,
	VAL_ARR    = 5,
	VAL_OBJ    = 6,
	VAL_ERR    = 7
} val_tag;

typedef struct value value_t;

typedef struct {
	bool      is_int_key;
	int64_t   ikey;
	char     *skey;          /* malloc'd; NULL when is_int_key */
	size_t    skey_len;
	value_t  *val;           /* malloc'd, owned by the parent */
} kv_t;

struct value {
	val_tag tag;
	union {
		bool    b;
		int64_t i;
		double  d;
		struct {
			char  *p;        /* malloc'd; not necessarily NUL-terminated */
			size_t len;
		} str;
		struct {
			kv_t  *items;    /* malloc'd, length = n, capacity tracked externally */
			size_t n;
			size_t cap;
		} arr;
		struct {
			char  *cls;
			size_t clen;
			kv_t  *props;
			size_t n;
			size_t cap;
		} obj;
		struct {
			char    *cls;
			char    *msg;
			int64_t  code;
			char    *trace;
		} err;
	} as;
};

/* Constructors — every returned pointer is independently malloc'd
 * and owns all transitive heap state. Pass to value_free() to release. */
value_t *value_null(void);
value_t *value_bool(bool b);
value_t *value_long(int64_t i);
value_t *value_double(double d);
value_t *value_str(const char *p, size_t len);
value_t *value_arr(size_t initial_cap);
value_t *value_obj(const char *cls, size_t clen, size_t initial_cap);
value_t *value_err(const char *cls, const char *msg, int64_t code, const char *trace);

/* Builders — value_t arguments are taken by ownership transfer. */
void value_arr_push(value_t *arr, value_t *child);                            /* sequential int key */
void value_arr_set_int(value_t *arr, int64_t key, value_t *child);            /* explicit int key */
void value_arr_set_str(value_t *arr, const char *key, size_t klen, value_t *child);
void value_obj_set(value_t *obj, const char *key, size_t klen, value_t *child);

/* Lookup — return borrowed pointer or NULL. */
const value_t *value_arr_get_str(const value_t *arr, const char *key, size_t klen);

/* Destructor — recursive; safe on NULL. */
void value_free(value_t *v);

#ifdef __cplusplus
}
#endif

#endif /* PARALLAX_VALUE_H */
