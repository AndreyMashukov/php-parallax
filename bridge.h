#ifndef PARALLAX_BRIDGE_H
#define PARALLAX_BRIDGE_H

#include "php_parallax.h"
#include "value/value.h"

/* Resolution of a userland callable into a recipe that survives the move
 * across to a worker thread. v1 supports named functions and static methods;
 * inline closures with `use(...)` captures are deferred to v0.2.0. */
typedef enum {
	PX_CALL_KIND_FUNCTION    = 1,   /* "fname"                  */
	PX_CALL_KIND_STATIC_METH = 2,   /* "ClassName::method"      */
} px_call_kind_t;

typedef struct {
	px_call_kind_t kind;
	char *class_name;     /* NULL for FUNCTION; otherwise malloc'd */
	char *fn_name;        /* malloc'd; never NULL */
	char *bootstrap;      /* malloc'd or NULL; absolute path to a PHP file
	                         the worker should require before resolving fn */
} px_callable_t;

void px_callable_free(px_callable_t *c);

/* Resolve a callable zval (string / array / first-class-callable Closure).
 * On success: writes *out, returns 0.
 * On failure: throws a CaptureError or SpawnError on the caller's thread,
 *             returns -1. */
int px_resolve_callable(zval *callable, px_callable_t *out);

/* Convert a zval into a snapshot value_t. Returns NULL and throws a
 * CaptureError on the caller's thread when the source contains unsupported
 * material (resource, nested Closure, by-ref, native-handle object, cycle). */
value_t *px_zval_to_value(zval *zv);

/* Reverse direction — turn a snapshot value_t into a freshly minted zval in
 * the current thread's arena. Always succeeds. */
void px_value_to_zval(const value_t *v, zval *out);

/* Worker entry — defined in bridge_worker.c. Runs inside the spawned thread. */
void px_worker_main(task_t *task);

#endif /* PARALLAX_BRIDGE_H */
