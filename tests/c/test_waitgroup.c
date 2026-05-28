#include "harness.h"
#include "value.h"
#include "waitgroup.h"

#include <stdatomic.h>

static _Atomic int run_counter;

/* Runner that doubles the integer in args. */
static void double_runner(task_t *task)
{
	atomic_fetch_add(&run_counter, 1);
	int64_t in = task->args->as.i;
	task->ok = true;
	task->result = value_long(in * 2);
}

static void test_basic_spawn(void)
{
	atomic_store(&run_counter, 0);
	wait_group_t *wg = wg_create(double_runner);
	for (int i = 0; i < 5; i++) {
		wg_go(wg, value_long(i), NULL);
	}
	wg_wait(wg);
	ASSERT_EQ_INT(atomic_load(&run_counter), 5);
	ASSERT_EQ_INT(wg_size(wg), 5);
	for (size_t s = 0; s < 5; s++) {
		const task_t *t = wg_get(wg, s);
		ASSERT_NOT_NULL(t);
		ASSERT_TRUE(t->ok);
		ASSERT_EQ_INT(t->result->as.i, (int64_t)(s * 2));
	}
	wg_destroy(wg);
}

/* Runner that yields an error for one slot, ok for others. */
static void error_isolating_runner(task_t *task)
{
	int64_t in = task->args->as.i;
	if (in == 1) {
		task->ok = false;
		task->error = value_err("RuntimeException", "boom", 0, "");
	} else {
		task->ok = true;
		task->result = value_long(in + 100);
	}
}

static void test_error_isolation(void)
{
	wait_group_t *wg = wg_create(error_isolating_runner);
	wg_go(wg, value_long(0), NULL);
	wg_go(wg, value_long(1), NULL);
	wg_go(wg, value_long(2), NULL);
	wg_wait(wg);

	const task_t *t0 = wg_get(wg, 0);
	const task_t *t1 = wg_get(wg, 1);
	const task_t *t2 = wg_get(wg, 2);

	ASSERT_TRUE(t0->ok);
	ASSERT_EQ_INT(t0->result->as.i, 100);

	ASSERT_TRUE(!t1->ok);
	ASSERT_NOT_NULL(t1->error);
	ASSERT_EQ_INT(t1->error->tag, VAL_ERR);

	ASSERT_TRUE(t2->ok);
	ASSERT_EQ_INT(t2->result->as.i, 102);

	wg_destroy(wg);
}

int main(void)
{
	RUN_TEST(test_basic_spawn);
	RUN_TEST(test_error_isolation);
	HARNESS_REPORT();
}
