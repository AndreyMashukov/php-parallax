#include "harness.h"
#include "value.h"
#include "waitgroup.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static _Atomic int done;

static void sum_runner(task_t *task)
{
	int64_t in = task->args->as.i;
	int64_t acc = 0;
	for (int64_t i = 0; i < in; i++) {
		acc += i;
	}
	task->ok = true;
	task->result = value_long(acc);
	atomic_fetch_add(&done, 1);
}

int main(int argc, char **argv)
{
	int n = (argc >= 2) ? atoi(argv[1]) : 1000;
	atomic_store(&done, 0);

	wait_group_t *wg = wg_create(sum_runner);
	for (int i = 0; i < n; i++) {
		wg_go(wg, value_long(100 + (i % 50)), NULL);
	}
	wg_wait(wg);

	int got = atomic_load(&done);
	if (got != n) {
		fprintf(stderr, "stress: expected %d tasks, got %d\n", n, got);
		wg_destroy(wg);
		return 1;
	}
	for (size_t s = 0; s < wg_size(wg); s++) {
		const task_t *t = wg_get(wg, s);
		if (!t->ok || t->result == NULL) {
			fprintf(stderr, "stress: slot %zu failed\n", s);
			wg_destroy(wg);
			return 1;
		}
	}
	wg_destroy(wg);

	fprintf(stderr, "stress: %d tasks completed cleanly\n", n);
	return 0;
}
