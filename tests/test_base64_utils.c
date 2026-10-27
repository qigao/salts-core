/* Original tn_base64_* contract, plus strict caller-owned counted byte ABI. */
#include <base64_utils.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
  fprintf(stderr, "Base64 test failed at %s:%d: %s\n", \
          __FILE__, __LINE__, #expr); return 1; } } while (0)

static int test_vectors(void) {
  static const char *const raw[] = {
    "", "f", "fo", "foo", "foob", "fooba", "foobar", "TurboUtils"
  };
  static const char *const canonical[] = {
    "", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy",
    "VHVyYm9VdGlscw=="
  };
  for (size_t i = 0u; i < sizeof(raw) / sizeof(raw[0]); ++i) {
    char out[64] = {0};
    char *heap = NULL;
    size_t size = 0u, actual = 0u;
    uint8_t decoded[64] = {0};
    CHECK(tn_base64_encode_buf_ex((const uint8_t *)raw[i], strlen(raw[i]),
                                  out, sizeof(out)) == TN_BASE64_OK);
    CHECK(strcmp(out, canonical[i]) == 0);
    CHECK(tn_base64_encode((const uint8_t *)raw[i], strlen(raw[i]), &heap) == 0);
    CHECK(heap != NULL && strcmp(heap, canonical[i]) == 0);
    free(heap);
    CHECK(tn_base64_decode_buf_ex(canonical[i], strlen(canonical[i]),
                                  NULL, 0u, &size) == TN_BASE64_OK);
    CHECK(size == strlen(raw[i]));
    CHECK(tn_base64_decode_buf_ex(canonical[i], strlen(canonical[i]),
                                  decoded, sizeof(decoded), &actual) == TN_BASE64_OK);
    CHECK(actual == size && memcmp(decoded, raw[i], size) == 0);
    if (i != 0u) {
      uint8_t *allocated = NULL;
      size_t written = 0u;
      CHECK(tn_base64_decode(canonical[i], &allocated, &written) == 0);
      CHECK(allocated != NULL && written == size &&
            memcmp(allocated, raw[i], size) == 0);
      free(allocated);
    }
  }
  return 0;
}

static int test_binary_folding(void) {
  const uint8_t bytes[] = {0u, 255u, 1u, 2u, 0u, 125u};
  char encoded[32] = {0};
  uint8_t decoded[16] = {0};
  size_t len = 0u;
  CHECK(tn_base64_encode_buf(bytes, sizeof(bytes), encoded, sizeof(encoded)) == 0);
  CHECK(tn_base64_decode_buf_ex(encoded, strlen(encoded), decoded,
                                sizeof(decoded), &len) == TN_BASE64_OK);
  CHECK(len == sizeof(bytes) && memcmp(decoded, bytes, len) == 0);
  CHECK(tn_base64_decode_buf_ex(" Zm9v\r\nYmFy \t", 14u, decoded,
                                sizeof(decoded), &len) == TN_BASE64_OK);
  CHECK(len == 6u && memcmp(decoded, "foobar", len) == 0);
  CHECK(tn_base64_decode_buf_ex("AA==", 4u, decoded, sizeof(decoded),
                                &len) == TN_BASE64_OK);
  CHECK(len == 1u && decoded[0] == 0u);
  return 0;
}

static int test_rejects_invalid_input_atomically(void) {
  static const char *const invalid[] = {
    "Zg=", "Zg===", "=m9v", "Zg=a", "Zg==Zg==",
    "Zm9v=", "Zh==", "Zm9=", "not valid base64!", "Zg\xFF==",
    "A", "AA", "AAA", "AA=A", "A===", "AAAA\nB"
  };
  for (size_t i = 0u; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
    uint8_t buf[32];
    size_t len = 817u;
    memset(buf, 0xA5, sizeof(buf));
    CHECK(tn_base64_decode_buf_ex(invalid[i], strlen(invalid[i]), buf,
                                  sizeof(buf), &len) == TN_BASE64_ERR_INVALID_INPUT);
    CHECK(len == 817u);
    for (size_t j = 0u; j < sizeof(buf); ++j) CHECK(buf[j] == 0xA5);
  }
  {
    const char embedded[] = {'Z', 'g', '\0', '=', '='};
    size_t len = 37u;
    CHECK(tn_base64_decode_buf_ex(embedded, sizeof(embedded), NULL, 0u, &len)
          == TN_BASE64_ERR_INVALID_INPUT);
    CHECK(len == 37u);
  }
  return 0;
}

static int test_errors_no_buffer_mutation(void) {
  unsigned char byte = (unsigned char)'f';
  char output[5] = {'!', '!', '!', '!', '!'};
  size_t written = 777u;
  uint8_t data[5] = {0xDD, 0xDD, 0xDD, 0xDD, 0xDD};
  CHECK(tn_base64_encode_buf_ex(&byte, 1u, output, 4u)
        == TN_BASE64_ERR_BUFFER_TOO_SMALL);
  CHECK(output[0] == '!' && output[4] == '!');
  CHECK(tn_base64_encode_buf_ex(&byte, SIZE_MAX, output, sizeof(output))
        == TN_BASE64_ERR_OVERFLOW);
  CHECK(output[0] == '!');
  CHECK(tn_base64_decode_buf_ex("Zg==", 4u, data, 0u, &written)
        == TN_BASE64_ERR_BUFFER_TOO_SMALL);
  CHECK(written == 777u && data[0] == 0xDD);
  CHECK(tn_base64_decode_buf_ex("Zg==", 4u, NULL, 3u, &written)
        == TN_BASE64_ERR_INVALID_ARG);
  CHECK(tn_base64_decode_buf_ex(NULL, 1u, data, sizeof(data), &written)
        == TN_BASE64_ERR_INVALID_ARG);
  CHECK(tn_base64_decode_buf_ex("Zg==", 4u, data, sizeof(data), NULL)
        == TN_BASE64_ERR_INVALID_ARG);
  CHECK(tn_base64_encode_ex(NULL, 10u).error == TN_BASE64_ERR_INVALID_ARG);
  CHECK(!tn_base64_decode_ex("!!").ok);
  {
    char *encoded = (char *)(uintptr_t)1u;
    uint8_t *decoded = (uint8_t *)(uintptr_t)1u;
    size_t size = 0u;
    CHECK(tn_base64_encode(NULL, 4u, &encoded) == -1 && encoded == NULL);
    CHECK(tn_base64_decode("invalid*data", &decoded, &size) == -1);
    CHECK(decoded == NULL && size == 0u);
  }
  CHECK(tn_base64_encode_buf_ex(NULL, 0u, output, sizeof(output)) == TN_BASE64_OK);
  CHECK(output[0] == '\0');
  return 0;
}

int main(void) {
  if (test_vectors() != 0 || test_binary_folding() != 0 ||
      test_rejects_invalid_input_atomically() != 0 ||
      test_errors_no_buffer_mutation() != 0) return 1;
  puts("Canonical tn_base64_* sole C11 owner passed");
  return 0;
}
