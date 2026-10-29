#include <salts/simd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "SIMD boundary failure at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

int main(void) {
    const salts_simd_desc *const shapes[] = {
        &salts_simd_i8x16, &salts_simd_u8x16,
        &salts_simd_i16x8, &salts_simd_u16x8,
        &salts_simd_i32x4, &salts_simd_u32x4,
        &salts_simd_i64x2, &salts_simd_u64x2,
        &salts_simd_f32x4, &salts_simd_f64x2,
        &salts_simd_b8x16, &salts_simd_b16x8,
        &salts_simd_b32x4, &salts_simd_b64x2
    };
    const int32_t left_data[4] = {1, -2, 3, -4};
    const int32_t right_data[4] = {10, 20, 30, 40};
    const int32_t expected_sum[4] = {11, 18, 33, 36};
    int32_t result[4] = {0};
    salts_simd_desc copy = salts_simd_i32x4;
    salts_simd_desc invalid = copy;
    cmeta_v128 left = {{0}}, right = {{0}}, out = {{0}}, sentinel = {{0}};
    cmeta_simd_scalar scalar = {0};
    unsigned char unaligned[18], stored[18];
    uint8_t selectors[16] = {0};
    size_t i;

    CHECK(sizeof(cmeta_v128) == 16u);
    CHECK(_Alignof(cmeta_v128) >= 16u);
    for (i = 0u; i < sizeof(shapes) / sizeof(shapes[0]); ++i) {
        CHECK(salts_simd_desc_valid(shapes[i]));
        CHECK((size_t)shapes[i]->lane_bits * shapes[i]->lane_count == 128u);
        CHECK(shapes[i]->is_mask == (shapes[i]->lane_kind == SALTS_SIMD_LANE_BOOL));
    }

    /* Plain value descriptors work independently of global pointer identity. */
    cmeta_simd_v128_load(&left, left_data);
    cmeta_simd_v128_load(&right, right_data);
    CHECK(cmeta_simd_binary(&copy, SALTS_SIMD_BINARY_ADD, &left, &left, &right));
    cmeta_simd_v128_store(result, &left);
    CHECK(memcmp(result, expected_sum, sizeof(result)) == 0);

    /* The implementation owns no caller storage; unaligned byte ranges need
     * only the documented readable/writable 16 bytes. Guards stay untouched. */
    memset(unaligned, 0xa5, sizeof(unaligned));
    memset(stored, 0x5a, sizeof(stored));
    memcpy(unaligned + 1u, left_data, sizeof(left_data));
    cmeta_simd_v128_load(&out, unaligned + 1u);
    cmeta_simd_v128_store(stored + 1u, &out);
    CHECK(memcmp(stored + 1u, left_data, sizeof(left_data)) == 0);
    CHECK(stored[0] == 0x5au && stored[17] == 0x5au);

    memset(sentinel.bytes, 0x6b, sizeof(sentinel.bytes));
    out = sentinel;
    CHECK(!salts_simd_desc_valid(NULL));
    CHECK(!cmeta_simd_binary(NULL, SALTS_SIMD_BINARY_ADD, &out, &left, &right));
    CHECK(memcmp(&out, &sentinel, sizeof(out)) == 0);
    invalid.lane_count = 3u;
    CHECK(!salts_simd_desc_valid(&invalid));
    CHECK(!cmeta_simd_binary(&invalid, SALTS_SIMD_BINARY_ADD, &out, &left, &right));
    CHECK(memcmp(&out, &sentinel, sizeof(out)) == 0);
    CHECK(!cmeta_simd_binary(&copy, SALTS_SIMD_BINARY_DIV, &out, &left, &right));
    CHECK(memcmp(&out, &sentinel, sizeof(out)) == 0);
    CHECK(!cmeta_simd_binary(&copy, SALTS_SIMD_BINARY_ADD, NULL, &left, &right));
    CHECK(!cmeta_simd_binary(&copy, SALTS_SIMD_BINARY_ADD, &out, NULL, &right));
    CHECK(memcmp(&out, &sentinel, sizeof(out)) == 0);

    scalar.i32 = 12345;
    CHECK(!cmeta_simd_extract_lane(&copy, &left, 4u, &scalar));
    CHECK(scalar.i32 == 12345);
    selectors[15] = 32u;
    CHECK(!cmeta_simd_shuffle_bytes(&out, &left, &right, selectors));
    CHECK(memcmp(&out, &sentinel, sizeof(out)) == 0);
    cmeta_simd_v128_load(&out, NULL);
    CHECK(memcmp(&out, &sentinel, sizeof(out)) == 0);
    cmeta_simd_v128_load(NULL, left_data);
    cmeta_simd_v128_store(NULL, &left);
    cmeta_simd_v128_store(stored + 1u, NULL);
    CHECK(memcmp(stored + 1u, left_data, sizeof(left_data)) == 0);
    return 0;
}
