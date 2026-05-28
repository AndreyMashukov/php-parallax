#include "bridge.h"
#include "php_parallax.h"

#include <Zend/zend_API.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_stream.h>
#include <TSRM.h>
#include <main/SAPI.h>

#include <stdlib.h>
#include <string.h>

/* user_data carried per task by the binding layer. */
typedef struct {
	px_callable_t callable;
	/* args live inside task->args as a VAL_ARR */
} px_task_payload_t;

/* TLS slot used by the user-exception-handler trampoline to surface the
 * captured exception back to worker_main_thread. PHP's engine treats an
 * uncaught exception inside call_user_function as fatal when there is no
 * parent PHP frame; installing a user-exception-handler intercepts that
 * path and lets us record the original exception. */
static __thread value_t *worker_captured_error = NULL;

static value_t *exception_to_value(zend_object *ex); /* forward */

PHP_FUNCTION(parallax_internal_capture_exception)
{
	zval *ex_zv;
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_OBJECT(ex_zv)
	ZEND_PARSE_PARAMETERS_END();

	if (worker_captured_error == NULL) {
		worker_captured_error = exception_to_value(Z_OBJ_P(ex_zv));
	}
}

static char *zstr_to_cstr(zend_string *zs)
{
	if (zs == NULL) {
		return NULL;
	}
	char *p = (char *)malloc(ZSTR_LEN(zs) + 1);
	if (p == NULL) { abort(); }
	memcpy(p, ZSTR_VAL(zs), ZSTR_LEN(zs));
	p[ZSTR_LEN(zs)] = '\0';
	return p;
}

static value_t *exception_to_value(zend_object *ex)
{
	zend_class_entry *ce = ex->ce;
	zval rv;

	zval *msg_zv  = zend_read_property(ce, ex, "message", sizeof("message") - 1, 1, &rv);
	zval *code_zv = zend_read_property(ce, ex, "code",    sizeof("code")    - 1, 1, &rv);

	char *cls_c = NULL;
	if (ce->name != NULL) {
		cls_c = zstr_to_cstr(ce->name);
	}
	char *msg_c = NULL;
	if (msg_zv != NULL && Z_TYPE_P(msg_zv) == IS_STRING) {
		msg_c = zstr_to_cstr(Z_STR_P(msg_zv));
	}
	int64_t code = 0;
	if (code_zv != NULL && Z_TYPE_P(code_zv) == IS_LONG) {
		code = (int64_t)Z_LVAL_P(code_zv);
	}

	/* Trace rendering is deferred — calling getTraceAsString() while the
	 * worker has just unwound from an exception can re-perturb engine state.
	 * The PHP-level WorkerError exposes class/message/code which is enough
	 * for v0.1.0 diagnostics. */
	char *trace_c = NULL;

	value_t *err = value_err(cls_c, msg_c, code, trace_c);
	free(cls_c); free(msg_c); free(trace_c);
	return err;
}

static void *worker_main_thread(void *task_void);

void px_worker_main(task_t *task)
{
	/* Real worker body executes on the spawned thread; this trampoline simply
	 * invokes it. The signature exists for symmetry with the C-only test runners. */
	worker_main_thread(task);
}

static void *worker_main_thread(void *task_void)
{
	task_t *task = (task_t *)task_void;
	px_task_payload_t *payload = (px_task_payload_t *)task->user_data;

	/* TSRM ----------------------------------------------------------------- */
	ts_resource(0);
	TSRMLS_CACHE_UPDATE();

	if (php_request_startup() == FAILURE) {
		task->ok = false;
		task->error = value_err("RuntimeException", "php_request_startup failed", 0, "");
		return NULL;
	}

	/* Bootstrap: load a user-supplied PHP file before resolving the callable.
	 * Without this, user-defined functions and classes are invisible to the
	 * fresh worker request — only internal functions resolve. */
	if (payload->callable.bootstrap != NULL) {
		zend_file_handle file_handle;
		zend_stream_init_filename(&file_handle, payload->callable.bootstrap);
		zend_first_try {
			zend_execute_scripts(ZEND_REQUIRE, NULL, 1, &file_handle);
		} zend_catch {
			/* bootstrap bailed — surface as a worker error and short-circuit */
		} zend_end_try();
		zend_destroy_file_handle(&file_handle);

		if (EG(exception) != NULL) {
			zend_object *ex = EG(exception);
			EG(exception) = NULL;
			task->ok = false;
			task->error = exception_to_value(ex);
			OBJ_RELEASE(ex);
			php_request_shutdown(NULL);
			ts_free_thread();
			px_callable_free(&payload->callable);
			free(payload);
			task->user_data = NULL;
			return NULL;
		}
	}

	/* Install our capture trampoline as the user-exception-handler. Without
	 * one, the engine treats any uncaught exception inside call_user_function
	 * as fatal because the worker's call stack has no parent PHP frame
	 * (EG(current_execute_data) is NULL at the boundary). The trampoline
	 * routes the exception object into worker_captured_error TLS so we can
	 * surface it to task->error after call_user_function returns. */
	worker_captured_error = NULL;
	zval handler_name;
	ZVAL_STRING(&handler_name, "parallax_internal_capture_exception");
	zval_ptr_dtor(&EG(user_exception_handler));
	ZVAL_COPY_VALUE(&EG(user_exception_handler), &handler_name);

	zend_first_try {
		/* Reify args: task->args is a VAL_ARR carrying positional zval payloads. */
		zval args_zv;
		px_value_to_zval(task->args, &args_zv);
		if (Z_TYPE(args_zv) != IS_ARRAY) {
			task->ok = false;
			task->error = value_err("LogicError", "task args were not an array", 0, "");
			goto shutdown;
		}

		/* Build zend_fcall_info{,_cache} for either a free function or a static method. */
		zval callable_zv;
		if (payload->callable.kind == PX_CALL_KIND_FUNCTION) {
			ZVAL_STRING(&callable_zv, payload->callable.fn_name);
		} else {
			array_init(&callable_zv);
			add_next_index_string(&callable_zv, payload->callable.class_name);
			add_next_index_string(&callable_zv, payload->callable.fn_name);
		}

		uint32_t argc = (uint32_t)zend_hash_num_elements(Z_ARRVAL(args_zv));
		zval *argv = (zend_hash_num_elements(Z_ARRVAL(args_zv)) > 0)
			? safe_emalloc(argc, sizeof(zval), 0)
			: NULL;
		uint32_t i = 0;
		zval *cur;
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL(args_zv), cur) {
			ZVAL_COPY(&argv[i++], cur);
		} ZEND_HASH_FOREACH_END();

		zval retval;
		ZVAL_UNDEF(&retval);

		int rc = call_user_function(EG(function_table), NULL, &callable_zv, &retval, argc, argv);

		if (argv != NULL) {
			for (uint32_t k = 0; k < argc; k++) {
				zval_ptr_dtor(&argv[k]);
			}
			efree(argv);
		}
		zval_ptr_dtor(&callable_zv);
		zval_ptr_dtor(&args_zv);

		if (worker_captured_error != NULL) {
			/* The user-exception-handler trampoline intercepted an
			 * uncaught throw and parked the value_t in TLS. */
			task->ok = false;
			task->error = worker_captured_error;
			worker_captured_error = NULL;
			if (task->result != NULL) {
				value_free(task->result);
				task->result = NULL;
			}
		} else if (rc == SUCCESS && !EG(exception)) {
			task->ok = true;
			task->result = px_zval_to_value(&retval);
			zval_ptr_dtor(&retval);
		} else if (EG(exception) != NULL) {
			zend_object *ex = EG(exception);
			EG(exception) = NULL;
			task->ok = false;
			task->error = exception_to_value(ex);
			OBJ_RELEASE(ex);
		} else {
			task->ok = false;
			task->error = value_err("RuntimeException", "call_user_function failed", 0, "");
		}

shutdown:
		;
	} zend_catch {
		task->ok = false;
		if (task->error == NULL) {
			task->error = value_err("Error", "bailout during worker execution", 0, "");
		}
	} zend_end_try();

	php_request_shutdown(NULL);
	ts_free_thread();

	px_callable_free(&payload->callable);
	free(payload);
	task->user_data = NULL;

	return NULL;
}
