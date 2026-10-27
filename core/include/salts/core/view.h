#ifndef SALTS_VIEW_H
#define SALTS_VIEW_H

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A borrowed, counted text view. No heap/pool ownership, CMeta reflection,
 * implicit UTF-8 validation, global state or compatibility type alias. */
typedef struct salts_view {
    const char *data;
    size_t len;
} salts_view;

/* Invalid nonempty NULL ranges stay invalid; never silently truncate. */
static inline salts_view salts_view_from_buf(const char *data, size_t size) {
    salts_view result = {data, size};
    return result;
}

static inline salts_view salts_view_from_cstr(const char *data) {
    salts_view result = {data, data ? strlen(data) : 0u};
    return result;
}

static inline int salts_view_is_valid(salts_view value) {
    return value.data != NULL || value.len == 0u;
}

#ifdef __cplusplus
}
#endif

#endif /* SALTS_VIEW_H */
