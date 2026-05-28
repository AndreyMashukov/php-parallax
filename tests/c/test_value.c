#include "harness.h"
#include "value.h"

#include <string.h>

static void test_scalars(void)
{
	value_t *n = value_null();
	ASSERT_EQ_INT(n->tag, VAL_NULL);
	value_free(n);

	value_t *bt = value_bool(true);
	ASSERT_EQ_INT(bt->tag, VAL_BOOL);
	ASSERT_TRUE(bt->as.b == true);
	value_free(bt);

	value_t *bf = value_bool(false);
	ASSERT_TRUE(bf->as.b == false);
	value_free(bf);

	value_t *i = value_long(42);
	ASSERT_EQ_INT(i->as.i, 42);
	value_free(i);

	value_t *neg = value_long(-12345);
	ASSERT_EQ_INT(neg->as.i, -12345);
	value_free(neg);

	value_t *d = value_double(3.14);
	ASSERT_TRUE(d->as.d == 3.14);
	value_free(d);
}

static void test_string(void)
{
	const char *src = "hello, world";
	value_t *s = value_str(src, strlen(src));
	ASSERT_EQ_INT(s->tag, VAL_STR);
	ASSERT_EQ_INT(s->as.str.len, (int)strlen(src));
	ASSERT_EQ_STR(s->as.str.p, s->as.str.len, src, strlen(src));
	value_free(s);

	value_t *empty = value_str("", 0);
	ASSERT_EQ_INT(empty->as.str.len, 0);
	ASSERT_TRUE(empty->as.str.p == NULL);
	value_free(empty);

	const unsigned char binary[] = { 0x00, 0x01, 0x02, 0xff };
	value_t *bin = value_str((const char *)binary, 4);
	ASSERT_EQ_INT(bin->as.str.len, 4);
	ASSERT_EQ_STR(bin->as.str.p, bin->as.str.len, binary, 4);
	value_free(bin);
}

static void test_array_sequential(void)
{
	value_t *arr = value_arr(0);
	value_arr_push(arr, value_long(10));
	value_arr_push(arr, value_long(20));
	value_arr_push(arr, value_long(30));
	ASSERT_EQ_INT(arr->as.arr.n, 3);
	ASSERT_EQ_INT(arr->as.arr.items[0].ikey, 0);
	ASSERT_EQ_INT(arr->as.arr.items[1].ikey, 1);
	ASSERT_EQ_INT(arr->as.arr.items[2].ikey, 2);
	ASSERT_EQ_INT(arr->as.arr.items[2].val->as.i, 30);
	value_free(arr);
}

static void test_array_assoc(void)
{
	value_t *arr = value_arr(0);
	value_arr_set_str(arr, "name", 4, value_str("Andrei", 6));
	value_arr_set_str(arr, "age", 3, value_long(30));
	ASSERT_EQ_INT(arr->as.arr.n, 2);

	const value_t *name = value_arr_get_str(arr, "name", 4);
	ASSERT_NOT_NULL(name);
	ASSERT_EQ_INT(name->tag, VAL_STR);
	ASSERT_EQ_STR(name->as.str.p, name->as.str.len, "Andrei", 6);

	const value_t *age = value_arr_get_str(arr, "age", 3);
	ASSERT_NOT_NULL(age);
	ASSERT_EQ_INT(age->as.i, 30);

	ASSERT_NULL(value_arr_get_str(arr, "missing", 7));
	value_free(arr);
}

static void test_object(void)
{
	value_t *obj = value_obj("App\\User", 8, 0);
	value_obj_set(obj, "id", 2, value_long(7));
	value_obj_set(obj, "email", 5, value_str("a@b.c", 5));

	ASSERT_EQ_INT(obj->tag, VAL_OBJ);
	ASSERT_EQ_INT(obj->as.obj.clen, 8);
	ASSERT_EQ_STR(obj->as.obj.cls, obj->as.obj.clen, "App\\User", 8);
	ASSERT_EQ_INT(obj->as.obj.n, 2);
	value_free(obj);
}

static void test_err(void)
{
	value_t *e = value_err("RuntimeException", "boom", 42, "#0 file.php(1): fn()");
	ASSERT_EQ_INT(e->tag, VAL_ERR);
	ASSERT_TRUE(strcmp(e->as.err.cls, "RuntimeException") == 0);
	ASSERT_TRUE(strcmp(e->as.err.msg, "boom") == 0);
	ASSERT_EQ_INT(e->as.err.code, 42);
	ASSERT_TRUE(strchr(e->as.err.trace, '#') != NULL);
	value_free(e);
}

static void test_nested(void)
{
	/* Build: ['users' => [['name' => 'A'], ['name' => 'B']]] */
	value_t *root = value_arr(0);
	value_t *users = value_arr(0);

	value_t *u1 = value_arr(0);
	value_arr_set_str(u1, "name", 4, value_str("A", 1));
	value_arr_push(users, u1);

	value_t *u2 = value_arr(0);
	value_arr_set_str(u2, "name", 4, value_str("B", 1));
	value_arr_push(users, u2);

	value_arr_set_str(root, "users", 5, users);

	const value_t *got = value_arr_get_str(root, "users", 5);
	ASSERT_NOT_NULL(got);
	ASSERT_EQ_INT(got->as.arr.n, 2);
	value_free(root);
}

int main(void)
{
	RUN_TEST(test_scalars);
	RUN_TEST(test_string);
	RUN_TEST(test_array_sequential);
	RUN_TEST(test_array_assoc);
	RUN_TEST(test_object);
	RUN_TEST(test_err);
	RUN_TEST(test_nested);
	HARNESS_REPORT();
}
