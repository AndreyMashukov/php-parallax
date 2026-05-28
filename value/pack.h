#ifndef PARALLAX_PACK_H
#define PARALLAX_PACK_H

#include <stdint.h>
#include <stddef.h>

#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pack value into a freshly malloc'd buffer.
 * Returns 0 on success, -1 on failure.
 * On success the caller owns *out_buf and must free() it. */
int value_pack(const value_t *v, uint8_t **out_buf, size_t *out_len);

/* Unpack a value previously produced by value_pack().
 * Returns a malloc'd value_t (release with value_free()) or NULL on error. */
value_t *value_unpack(const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PARALLAX_PACK_H */
