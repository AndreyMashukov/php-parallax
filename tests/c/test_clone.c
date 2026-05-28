#include "harness.h"
#include "value.h"
#include "clone.h"

#include <string.h>

static void test_clone_independence(void)
{
	value_t *src = value_arr(0);
	value_arr_set_str(src, "name", 4, value_str("orig", 4));

	value_t *copy = value_clone(src);
	ASSERT_NOT_NULL(copy);

	/* Mutate the original by appending — copy must be unaffected. */
	value_arr_push(src, value_long(99));

	ASSERT_EQ_INT(copy->as.arr.n, 1);
	ASSERT_EQ_INT(src->as.arr.n, 2);

	value_free(src);
	value_free(copy);
}

static void test_clone_deep(void)
{
	value_t *src = value_obj("Foo", 3, 0);
	value_t *inner = value_arr(0);
	value_arr_push(inner, value_str("hello", 5));
	value_obj_set(src, "list", 4, inner);

	value_t *copy = value_clone(src);
	ASSERT_NOT_NULL(copy);
	ASSERT_EQ_INT(copy->tag, VAL_OBJ);
	ASSERT_EQ_STR(copy->as.obj.cls, copy->as.obj.clen, "Foo", 3);
	ASSERT_EQ_INT(copy->as.obj.n, 1);

	const kv_t *kv = &copy->as.obj.props[0];
	ASSERT_EQ_INT(kv->val->tag, VAL_ARR);
	ASSERT_EQ_INT(kv->val->as.arr.n, 1);
	ASSERT_EQ_STR(kv->val->as.arr.items[0].val->as.str.p,
	              kv->val->as.arr.items[0].val->as.str.len,
	              "hello", 5);

	/* Pointers must differ — clone is an independent allocation. */
	ASSERT_TRUE(copy->as.obj.cls != src->as.obj.cls);
	ASSERT_TRUE(kv->val != src->as.obj.props[0].val);

	value_free(src);
	value_free(copy);
}

static void test_clone_null(void)
{
	ASSERT_NULL(value_clone(NULL));
}

static void test_clone_all_scalar(void)
{
	value_t *vals[] = {
		value_null(),
		value_bool(true),
		value_long(123),
		value_double(2.5),
		value_str("s", 1),
		value_err("E", "m", 1, "t"),
	};
	for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
		value_t *c = value_clone(vals[i]);
		ASSERT_NOT_NULL(c);
		ASSERT_EQ_INT(c->tag, vals[i]->tag);
		value_free(c);
		value_free(vals[i]);
	}
}

int main(void)
{
	RUN_TEST(test_clone_independence);
	RUN_TEST(test_clone_deep);
	RUN_TEST(test_clone_null);
	RUN_TEST(test_clone_all_scalar);
	HARNESS_REPORT();
}
