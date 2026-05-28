#ifndef PARALLAX_WAITGROUP_H
#define PARALLAX_WAITGROUP_H

#include <pthread.h>
#include <stddef.h>
#include <stdbool.h>

#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A single task slot. Owned by the wait_group_t. */
typedef struct task task_t;

struct task {
	pthread_t   thread;
	bool        thread_joinable;
	size_t      slot;             /* index in WaitGroup; preserves caller's ordering */

	/* Inputs — set by the spawning side. */
	value_t    *args;             /* malloc'd, ownership transferred to task */
	void       *user_data;        /* opaque pointer passed to runner */

	/* Output — set by the worker after the runner returns. */
	bool        ok;
	value_t    *result;           /* set when ok */
	value_t    *error;            /* VAL_ERR; set when !ok */
};

/* The worker entry point invoked on each spawned thread.
 * `args` is the value_t snapshot moved into the task; the runner takes
 * ownership and must free it. On success, store the produced value_t
 * into *result. On failure, set *result = NULL and return a VAL_ERR via
 * *error. Either *result or *error must be set, never both. */
typedef void (*task_runner_t)(task_t *task);

typedef struct {
	pthread_mutex_t lock;
	pthread_cond_t  cond;
	int             remaining;
	task_t        **tasks;
	size_t          count;
	size_t          cap;
	task_runner_t   runner;
} wait_group_t;

/* Lifecycle ---------------------------------------------------------------- */
wait_group_t *wg_create(task_runner_t runner);
void          wg_destroy(wait_group_t *wg);

/* Spawn — args ownership is transferred. Returns the slot index assigned
 * to the new task, or (size_t)-1 on pthread_create failure (rare). */
size_t wg_go(wait_group_t *wg, value_t *args, void *user_data);

/* Block until every spawned task has signalled done. */
void   wg_wait(wait_group_t *wg);

/* Borrow a task by slot index — only valid after wg_wait() returns. */
const task_t *wg_get(const wait_group_t *wg, size_t slot);
size_t        wg_size(const wait_group_t *wg);

/* Internal — runner calls this to signal completion. */
void   wg_signal_done(wait_group_t *wg);

#ifdef __cplusplus
}
#endif

#endif /* PARALLAX_WAITGROUP_H */
