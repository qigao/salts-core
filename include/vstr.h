#ifndef SALTS_CORE_VSTR_H
#define SALTS_CORE_VSTR_H

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A borrowed, counted text view. No heap/pool ownership, CMeta reflection,
 * implicit UTF-8 validation, global state or compatibility type alias. */
typedef struct vstr {
    const char *data;
    size_t len;
} vstr;

/* Invalid nonempty NULL ranges stay invalid; never silently truncate. */
static inline vstr vstr_from_buf(const char *data, size_t size) {
    vstr result = {data, size};
    return result;
}

static inline vstr vstr_from_cstr(const char *data) {
    vstr result = {data, data ? strlen(data) : 0u};
    return result;
}

static inline int vstr_is_valid(vstr value) {
    return value.data != NULL || value.len == 0u;
}

#ifdef __cplusplus
}
#endif

#endif /* SALTS_CORE_VSTR_H */
