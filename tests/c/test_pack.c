#include "harness.h"
#include "value.h"
#include "pack.h"

#include <stdlib.h>
#include <string.h>

static value_t *roundtrip(const value_t *src)
{
	uint8_t *buf = NULL;
	size_t   len = 0;
	int rc = value_pack(src, &buf, &len);
	if (rc != 0 || buf == NULL) {
		return NULL;
	}
	value_t *out = value_unpack(buf, len);
	free(buf);
	return out;
}

static void test_pack_null(void)
{
	value_t *src = value_null();
	value_t *got = roundtrip(src);
	ASSERT_NOT_NULL(got);
	ASSERT_EQ_INT(got->tag, VAL_NULL);
	value_free(src);
	value_free(got);
}

static void test_pack_bool(void)
{
	value_t *t = value_bool(true);
	value_t *f = value_bool(false);
	value_t *tg = roundtrip(t);
	value_t *fg = roundtrip(f);
	ASSERT_TRUE(tg->as.b == true);
	ASSERT_TRUE(fg->as.b == false);
	value_free(t); value_free(f); value_free(tg); value_free(fg);
}

static void test_pack_long(void)
{
	int64_t cases[] = { 0, 1, -1, 127, 128, 16384, -16384, INT64_MAX, INT64_MIN };
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		value_t *src = value_long(cases[i]);
		value_t *got = roundtrip(src);
		ASSERT_NOT_NULL(got);
		ASSERT_EQ_INT(got->as.i, cases[i]);
		value_free(src); value_free(got);
	}
}

static void test_pack_double(void)
{
	double cases[] = { 0.0, 1.0, -1.0, 3.141592653589793, 1.7e308, -1.7e308 };
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		value_t *src = value_double(cases[i]);
		value_t *got = roundtrip(src);
		ASSERT_NOT_NULL(got);
		ASSERT_TRUE(got->as.d == cases[i]);
		value_free(src); value_free(got);
	}
}

static void test_pack_string(void)
{
	const char *cases[] = { "", "x", "hello", "ünicödé", "with\0nul" };
	size_t lens[]       = {  0,   1,       5,         9,          8 };
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		value_t *src = value_str(cases[i], lens[i]);
		value_t *got = roundtrip(src);
		ASSERT_NOT_NULL(got);
		ASSERT_EQ_INT(got->as.str.len, (int)lens[i]);
		ASSERT_EQ_STR(got->as.str.p, got->as.str.len, cases[i], lens[i]);
		value_free(src); value_free(got);
	}
}

static void test_pack_array_seq(void)
{
	value_t *arr = value_arr(0);
	value_arr_push(arr, value_long(10));
	value_arr_push(arr, value_str("two", 3));
	value_arr_push(arr, value_bool(true));
	value_t *got = roundtrip(arr);
	ASSERT_NOT_NULL(got);
	ASSERT_EQ_INT(got->tag, VAL_ARR);
	ASSERT_EQ_INT(got->as.arr.n, 3);
	ASSERT_EQ_INT(got->as.arr.items[0].ikey, 0);
	ASSERT_EQ_INT(got->as.arr.items[0].val->as.i, 10);
	ASSERT_EQ_STR(got->as.arr.items[1].val->as.str.p, got->as.arr.items[1].val->as.str.len, "two", 3);
	ASSERT_TRUE(got->as.arr.items[2].val->as.b);
	value_free(arr); value_free(got);
}

static void test_pack_array_assoc(void)
{
	value_t *arr = value_arr(0);
	value_arr_set_str(arr, "id", 2, value_long(7));
	value_arr_set_str(arr, "name", 4, value_str("Andrei", 6));
	value_t *got = roundtrip(arr);
	ASSERT_NOT_NULL(got);
	const value_t *id = value_arr_get_str(got, "id", 2);
	ASSERT_EQ_INT(id->as.i, 7);
	const value_t *name = value_arr_get_str(got, "name", 4);
	ASSERT_EQ_STR(name->as.str.p, name->as.str.len, "Andrei", 6);
	value_free(arr); value_free(got);
}

static void test_pack_object(void)
{
	value_t *obj = value_obj("DTO", 3, 0);
	value_obj_set(obj, "x", 1, value_long(1));
	value_obj_set(obj, "y", 1, value_long(2));
	value_t *got = roundtrip(obj);
	ASSERT_EQ_INT(got->tag, VAL_OBJ);
	ASSERT_EQ_STR(got->as.obj.cls, got->as.obj.clen, "DTO", 3);
	ASSERT_EQ_INT(got->as.obj.n, 2);
	value_free(obj); value_free(got);
}

static void test_pack_err(void)
{
	value_t *e = value_err("E", "msg", 99, "trace");
	value_t *g = roundtrip(e);
	ASSERT_TRUE(strcmp(g->as.err.cls, "E") == 0);
	ASSERT_TRUE(strcmp(g->as.err.msg, "msg") == 0);
	ASSERT_EQ_INT(g->as.err.code, 99);
	ASSERT_TRUE(strcmp(g->as.err.trace, "trace") == 0);
	value_free(e); value_free(g);
}

static void test_pack_deep_nesting(void)
{
	value_t *deep = value_long(0);
	for (int i = 0; i < 50; i++) {
		value_t *wrap = value_arr(0);
		value_arr_push(wrap, deep);
		deep = wrap;
	}
	value_t *got = roundtrip(deep);
	ASSERT_NOT_NULL(got);

	const value_t *cur = got;
	for (int i = 0; i < 50; i++) {
		ASSERT_EQ_INT(cur->tag, VAL_ARR);
		ASSERT_EQ_INT(cur->as.arr.n, 1);
		cur = cur->as.arr.items[0].val;
	}
	ASSERT_EQ_INT(cur->tag, VAL_LONG);
	ASSERT_EQ_INT(cur->as.i, 0);

	value_free(deep); value_free(got);
}

static void test_pack_truncated_buffer_rejected(void)
{
	value_t *src = value_str("hello", 5);
	uint8_t *buf = NULL; size_t len = 0;
	value_pack(src, &buf, &len);
	value_t *bad = value_unpack(buf, len - 1);
	ASSERT_NULL(bad);
	free(buf); value_free(src);
}

int main(void)
{
	RUN_TEST(test_pack_null);
	RUN_TEST(test_pack_bool);
	RUN_TEST(test_pack_long);
	RUN_TEST(test_pack_double);
	RUN_TEST(test_pack_string);
	RUN_TEST(test_pack_array_seq);
	RUN_TEST(test_pack_array_assoc);
	RUN_TEST(test_pack_object);
	RUN_TEST(test_pack_err);
	RUN_TEST(test_pack_deep_nesting);
	RUN_TEST(test_pack_truncated_buffer_rejected);
	HARNESS_REPORT();
}
