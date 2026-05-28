/* This translation unit exists so config.m4 can list a stable entry point
 * file name. The actual module entry + class registrations live in
 * php_binding.c; we just re-export the module-globals storage symbol. */

#include "php_parallax.h"
