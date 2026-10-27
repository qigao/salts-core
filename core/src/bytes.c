#include <salts/core/bytes.h>

int salts_bytes_view_init(salts_bytes_view *out,
                               const void *data, size_t length) {
  salts_bytes_view candidate;
  if (out == NULL || (data == NULL && length != 0u)) return SALTS_BYTES_EINVAL;
  candidate.data = (const char *)data;
  candidate.len = length;
  *out = candidate;
  return SALTS_BYTES_OK;
}

int salts_bytes_view_slice(salts_bytes_view view,
                                size_t start, size_t length,
                                salts_bytes_view *out) {
  salts_bytes_view candidate;
  if (out == NULL || (view.data == NULL && view.len != 0u) ||
      start > view.len || length > view.len - start) return SALTS_BYTES_EINVAL;
  candidate.data = view.data == NULL ? NULL : view.data + start;
  candidate.len = length;
  *out = candidate;
  return SALTS_BYTES_OK;
}
