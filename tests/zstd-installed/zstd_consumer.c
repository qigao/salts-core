#include <cmeta_error.h>
#include <cmeta_zstd.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#include <type_traits>
#define SIGNATURE(name, ...) static_assert(std::is_same_v<decltype(&name), __VA_ARGS__>, #name)
#else
#define SIGNATURE(name, ...) _Static_assert(_Generic(&name, __VA_ARGS__: 1, default: 0), #name)
#endif

#define REQUIRE(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "Installed Zstd contract failed at line %d: %s\n", \
            __LINE__, #condition); \
    goto cleanup; \
  } \
} while (0)

SIGNATURE(cmeta_zstd_compress_bound, int (*)(size_t, size_t *));
SIGNATURE(cmeta_zstd_compress, int (*)(void *, size_t, size_t *, const void *, size_t, int));
SIGNATURE(cmeta_zstd_decompress, int (*)(void *, size_t, size_t *, const void *, size_t));
SIGNATURE(cmeta_zstd_frame_content_size, int (*)(const void *, size_t, size_t *));
SIGNATURE(cmeta_zstd_default_level, int (*)(void));
SIGNATURE(cmeta_zstd_min_level, int (*)(void));
SIGNATURE(cmeta_zstd_max_level, int (*)(void));

int main(void) {
  unsigned char source[256];
  unsigned char restored[sizeof(source)];
  unsigned char *compressed = NULL;
  size_t bound = 0u;
  size_t compressed_size = 0u;
  size_t output_size = 101u;
  size_t index;
  int minimum = cmeta_zstd_min_level();
  int maximum = cmeta_zstd_max_level();
  int default_level = cmeta_zstd_default_level();
  int status = EXIT_FAILURE;

  REQUIRE(minimum < maximum);
  REQUIRE(minimum <= default_level && default_level <= maximum);
  for (index = 0u; index < sizeof(source); ++index) source[index] = (unsigned char)index;
  REQUIRE(cmeta_zstd_compress_bound(sizeof(source), &bound) == SALTS_OK);
  REQUIRE(bound >= sizeof(source));
  compressed = (unsigned char *)malloc(bound);
  REQUIRE(compressed != NULL);

  /* All seven public symbols are called through the installed Core owner.
   * Source and destination allocations remain entirely owned by this client. */
  REQUIRE(cmeta_zstd_compress(compressed, bound, &compressed_size, source,
                              sizeof(source), 0) == SALTS_OK);
  REQUIRE(compressed_size > 0u && compressed_size <= bound);
  REQUIRE(cmeta_zstd_frame_content_size(compressed, compressed_size, &output_size) == SALTS_OK);
  REQUIRE(output_size == sizeof(source));
  memset(source, 0xa5, sizeof(source));
  REQUIRE(cmeta_zstd_decompress(restored, sizeof(restored), &output_size,
                                compressed, compressed_size) == SALTS_OK);
  REQUIRE(output_size == sizeof(source));
  for (index = 0u; index < sizeof(restored); ++index)
    REQUIRE(restored[index] == (unsigned char)index);

  output_size = 102u;
  REQUIRE(cmeta_zstd_decompress(restored, sizeof(restored) - 1u, &output_size,
                                compressed, compressed_size) == SALTS_ENOSPC);
  REQUIRE(output_size == 102u);
  REQUIRE(cmeta_zstd_compress(compressed, 1u, &output_size, source,
                              sizeof(source), 0) == SALTS_ENOSPC);
  REQUIRE(output_size == 102u);
  REQUIRE(cmeta_zstd_compress_bound(SIZE_MAX, &output_size) == SALTS_ERANGE);
  REQUIRE(output_size == 102u);
  REQUIRE(cmeta_zstd_compress_bound(0u, NULL) == SALTS_EINVAL);
  REQUIRE(cmeta_zstd_compress(compressed, bound, &output_size, NULL, 1u, 0) == SALTS_EINVAL);
  REQUIRE(output_size == 102u);
  REQUIRE(cmeta_zstd_decompress(NULL, 1u, &output_size, compressed,
                                compressed_size) == SALTS_EINVAL);
  REQUIRE(output_size == 102u);
  REQUIRE(cmeta_zstd_frame_content_size(NULL, 0u, &output_size) == SALTS_EINVAL);
  REQUIRE(output_size == 102u);
  if (maximum < INT_MAX) {
    REQUIRE(cmeta_zstd_compress(compressed, bound, &output_size, source,
                                sizeof(source), maximum + 1) == SALTS_EINVAL);
    REQUIRE(output_size == 102u);
  }

  REQUIRE(cmeta_zstd_compress(compressed, bound, &compressed_size,
                              NULL, 0u, default_level) == SALTS_OK);
  REQUIRE(cmeta_zstd_decompress(NULL, 0u, &output_size,
                                compressed, compressed_size) == SALTS_OK);
  REQUIRE(output_size == 0u);
  REQUIRE(cmeta_zstd_frame_content_size(compressed, compressed_size, &output_size) == SALTS_OK);
  REQUIRE(output_size == 0u);
  status = EXIT_SUCCESS;

cleanup:
  free(compressed);
  return status;
}
