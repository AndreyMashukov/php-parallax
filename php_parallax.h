#ifndef PHP_PARALLAX_H
#define PHP_PARALLAX_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <php.h>
#include <php_ini.h>
#include <ext/standard/info.h>
#include <main/SAPI.h>
#include <main/php_main.h>
#include <Zend/zend_modules.h>
#include <Zend/zend_API.h>
#include <Zend/zend_exceptions.h>

#if PHP_VERSION_ID < 80400
# error "parallax requires PHP 8.4 or newer"
#endif

#ifndef ZTS
# error "parallax requires a ZTS (thread-safe) PHP build"
#endif

#define PHP_PARALLAX_VERSION "0.2.0"

extern zend_module_entry parallax_module_entry;
#define phpext_parallax_ptr &parallax_module_entry

ZEND_BEGIN_MODULE_GLOBALS(parallax)
	zend_long max_workers;
	zend_long worker_stack_kb;
ZEND_END_MODULE_GLOBALS(parallax)

ZEND_EXTERN_MODULE_GLOBALS(parallax)

#define PARALLAX_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(parallax, v)

/* Class entries — defined in php_binding.c, shared with bridge.c / bridge_worker.c */
extern zend_class_entry *parallax_waitgroup_ce;
extern zend_class_entry *parallax_result_ce;
extern zend_class_entry *parallax_worker_error_ce;
extern zend_class_entry *parallax_capture_error_ce;
extern zend_class_entry *parallax_spawn_error_ce;

#include "value/value.h"
#include "core/waitgroup.h"

/* Internal object struct attached to WaitGroup userland instances. */
typedef struct {
	wait_group_t *wg;
	char         *bootstrap;    /* malloc'd; NULL when none */
	zend_object   std;          /* MUST stay last — Zend expects this layout */
} parallax_waitgroup_object_t;

static inline parallax_waitgroup_object_t *parallax_waitgroup_from_obj(zend_object *obj)
{
	return (parallax_waitgroup_object_t *)((char *)obj - XtOffsetOf(parallax_waitgroup_object_t, std));
}

#endif /* PHP_PARALLAX_H */
