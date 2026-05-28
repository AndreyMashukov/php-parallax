#ifndef PARALLAX_BRIDGE_COMPAT_H
#define PARALLAX_BRIDGE_COMPAT_H

#include <Zend/zend_compile.h>
#include <Zend/zend_modules.h>

/* All reads of zend_function::fn_flags / zend_class_entry::ce_flags route through
 * the helpers in this file. PHP 8.6 introduces fn_flags2/ce_flags2 because the
 * primary bitfields are exhausted; any future bit relocation lives here. */

static zend_always_inline bool px_fn_is_closure(const zend_function *f)
{
	return (f->common.fn_flags & ZEND_ACC_CLOSURE) != 0;
}

static zend_always_inline bool px_fn_is_static(const zend_function *f)
{
	return (f->common.fn_flags & ZEND_ACC_STATIC) != 0;
}

static zend_always_inline bool px_fn_is_abstract(const zend_function *f)
{
	return (f->common.fn_flags & ZEND_ACC_ABSTRACT) != 0;
}

#endif /* PARALLAX_BRIDGE_COMPAT_H */
