#include "waitgroup.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct {
	wait_group_t *wg;
	task_t       *task;
} worker_ctx_t;

static void *worker_thread(void *arg)
{
	worker_ctx_t *ctx = (worker_ctx_t *)arg;
	wait_group_t *wg = ctx->wg;
	task_t *task = ctx->task;
	free(ctx);

	wg->runner(task);
	wg_signal_done(wg);
	return NULL;
}

wait_group_t *wg_create(task_runner_t runner)
{
	assert(runner != NULL);
	wait_group_t *wg = (wait_group_t *)calloc(1, sizeof(*wg));
	if (wg == NULL) { abort(); }
	if (pthread_mutex_init(&wg->lock, NULL) != 0) { abort(); }
	if (pthread_cond_init(&wg->cond, NULL) != 0)  { abort(); }
	wg->runner = runner;
	return wg;
}

static void grow(wait_group_t *wg)
{
	size_t new_cap = (wg->cap == 0) ? 8 : (wg->cap * 2);
	task_t **new_tasks = (task_t **)realloc(wg->tasks, new_cap * sizeof(task_t *));
	if (new_tasks == NULL) { abort(); }
	wg->tasks = new_tasks;
	wg->cap = new_cap;
}

size_t wg_go(wait_group_t *wg, value_t *args, void *user_data)
{
	task_t *task = (task_t *)calloc(1, sizeof(*task));
	if (task == NULL) { abort(); }
	task->args = args;
	task->user_data = user_data;

	pthread_mutex_lock(&wg->lock);
	if (wg->count == wg->cap) {
		grow(wg);
	}
	task->slot = wg->count;
	wg->tasks[wg->count++] = task;
	wg->remaining++;
	pthread_mutex_unlock(&wg->lock);

	worker_ctx_t *ctx = (worker_ctx_t *)malloc(sizeof(*ctx));
	if (ctx == NULL) { abort(); }
	ctx->wg = wg;
	ctx->task = task;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	/* musl default thread stack ~128 KB — too small for a PHP interpreter.
	 * Set 8 MB to match glibc. ini override comes later via the PHP binding. */
	pthread_attr_setstacksize(&attr, (size_t)8 * 1024 * 1024);

	if (pthread_create(&task->thread, &attr, worker_thread, ctx) != 0) {
		pthread_attr_destroy(&attr);
		free(ctx);
		pthread_mutex_lock(&wg->lock);
		wg->remaining--;
		pthread_mutex_unlock(&wg->lock);
		task->ok = false;
		task->thread_joinable = false;
		return (size_t)-1;
	}
	pthread_attr_destroy(&attr);
	task->thread_joinable = true;
	return task->slot;
}

void wg_signal_done(wait_group_t *wg)
{
	pthread_mutex_lock(&wg->lock);
	if (--wg->remaining == 0) {
		pthread_cond_broadcast(&wg->cond);
	}
	pthread_mutex_unlock(&wg->lock);
}

void wg_wait(wait_group_t *wg)
{
	pthread_mutex_lock(&wg->lock);
	while (wg->remaining > 0) {
		pthread_cond_wait(&wg->cond, &wg->lock);
	}
	pthread_mutex_unlock(&wg->lock);

	for (size_t i = 0; i < wg->count; i++) {
		if (wg->tasks[i] && wg->tasks[i]->thread_joinable) {
			pthread_join(wg->tasks[i]->thread, NULL);
			wg->tasks[i]->thread_joinable = false;
		}
	}
}

const task_t *wg_get(const wait_group_t *wg, size_t slot)
{
	if (slot >= wg->count) {
		return NULL;
	}
	return wg->tasks[slot];
}

size_t wg_size(const wait_group_t *wg)
{
	return wg->count;
}

void wg_destroy(wait_group_t *wg)
{
	if (wg == NULL) {
		return;
	}
	for (size_t i = 0; i < wg->count; i++) {
		task_t *t = wg->tasks[i];
		if (t == NULL) {
			continue;
		}
		if (t->thread_joinable) {
			pthread_join(t->thread, NULL);
		}
		value_free(t->args);
		value_free(t->result);
		value_free(t->error);
		free(t);
	}
	free(wg->tasks);
	pthread_mutex_destroy(&wg->lock);
	pthread_cond_destroy(&wg->cond);
	free(wg);
}
