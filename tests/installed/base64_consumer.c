#include <base64_utils.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  const uint8_t raw[] = {0u, 17u, 128u, 255u};
  char encoded[16] = {0};
  uint8_t decoded[sizeof(raw)] = {0};
  char *heap = NULL;
  size_t needed = 0u, length = 0u;
  if (tn_base64_encode_buf_ex(raw, sizeof(raw), encoded, sizeof(encoded))
      != TN_BASE64_OK) return 1;
  if (tn_base64_encode(raw, sizeof(raw), &heap) != 0 ||
      !heap || strcmp(encoded, heap) != 0) return 2;
  free(heap);
  if (tn_base64_decode_buf_ex(encoded, strlen(encoded), NULL, 0u, &needed)
      != TN_BASE64_OK || needed != sizeof(raw)) return 3;
  if (tn_base64_decode_buf_ex(encoded, strlen(encoded), decoded,
                               sizeof(decoded), &length) != TN_BASE64_OK ||
      length != sizeof(raw) || memcmp(decoded, raw, sizeof(raw)) != 0) return 4;
  return 0;
}
