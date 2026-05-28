PHP_ARG_ENABLE([parallax],
  [whether to enable parallax],
  [AS_HELP_STRING([--enable-parallax], [Enable parallax extension])],
  [no])

if test "$PHP_PARALLAX" != "no"; then
  AC_DEFINE(HAVE_PARALLAX, 1, [parallax enabled])
  PHP_ADD_LIBRARY(pthread,, PARALLAX_SHARED_LIBADD)
  PHP_SUBST(PARALLAX_SHARED_LIBADD)

  PARALLAX_SOURCES="\
    php_parallax.c \
    php_binding.c \
    bridge.c \
    bridge_worker.c \
    bridge_closure.c \
    core/waitgroup.c \
    value/value.c \
    value/pack.c \
    value/clone.c"

  PHP_NEW_EXTENSION(parallax, $PARALLAX_SOURCES, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)

  PHP_ADD_BUILD_DIR([$ext_builddir/core])
  PHP_ADD_BUILD_DIR([$ext_builddir/value])
  PHP_ADD_INCLUDE([$ext_builddir/value])
  PHP_ADD_INCLUDE([$ext_builddir/core])
fi
