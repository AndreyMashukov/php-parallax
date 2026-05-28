#ifndef PARALLAX_CLONE_H
#define PARALLAX_CLONE_H

#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Deep clone — recursively duplicates every malloc'd allocation owned by `src`.
 * The returned value_t is independently owned and may safely cross thread
 * boundaries without aliasing the source. Returns NULL only when src is NULL. */
value_t *value_clone(const value_t *src);

#ifdef __cplusplus
}
#endif

#endif /* PARALLAX_CLONE_H */
