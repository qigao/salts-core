/* Independent C11 installed consumer. Only Salts::SIMD is required. */
#include <salts/simd.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(cmeta_v128) == 16u, "128-bit public carrier");
_Static_assert(_Alignof(cmeta_v128) == 16u, "16-byte public alignment");
_Static_assert(sizeof(salts_simd_v128) == sizeof(cmeta_v128), "one carrier");
_Static_assert(offsetof(salts_simd_v128, bytes) == 0u, "bytes start at zero");

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "SIMD C11 consumer: %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
    return 1; \
} } while (0)

/* Typed volatile references force all 32 DLL/import-library symbols to link,
 * including APIs that this small runtime consumer does not otherwise call. */
#define SIMD_EXECUTION_APIS(X) \
    X(cmeta_simd_v128_load, void, (cmeta_v128 *, const void *)) \
    X(cmeta_simd_v128_store, void, (void *, const cmeta_v128 *)) \
    X(cmeta_simd_splat, bool, (const salts_simd_desc *, cmeta_v128 *, cmeta_simd_scalar)) \
    X(cmeta_simd_load_splat, bool, (const salts_simd_desc *, cmeta_v128 *, const void *)) \
    X(cmeta_simd_load_extend, bool, (const salts_simd_desc *, cmeta_v128 *, const void *)) \
    X(cmeta_simd_load_zero, bool, (uint16_t, cmeta_v128 *, const void *)) \
    X(cmeta_simd_extract_lane, bool, (const salts_simd_desc *, const cmeta_v128 *, uint16_t, cmeta_simd_scalar *)) \
    X(cmeta_simd_replace_lane, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *, uint16_t, cmeta_simd_scalar)) \
    X(cmeta_simd_shuffle_bytes, bool, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *, const uint8_t *)) \
    X(cmeta_simd_swizzle_bytes, bool, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_unary, bool, (const salts_simd_desc *, cmeta_simd_unary_op, cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_binary, bool, (const salts_simd_desc *, cmeta_simd_binary_op, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_compare, bool, (const salts_simd_desc *, cmeta_simd_compare_op, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_shift, bool, (const salts_simd_desc *, cmeta_simd_shift_op, cmeta_v128 *, const cmeta_v128 *, uint32_t)) \
    X(cmeta_simd_saturating_binary, bool, (const salts_simd_desc *, cmeta_simd_saturating_op, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_reduce, bool, (const salts_simd_desc *, cmeta_simd_reduce_op, const cmeta_v128 *, uint32_t *)) \
    X(cmeta_simd_narrow, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_extend_half, bool, (const salts_simd_desc *, cmeta_simd_half, cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_extmul_half, bool, (const salts_simd_desc *, cmeta_simd_half, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_extadd_pairwise, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_q15mulr_sat, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_dot_pairwise, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_convert, bool, (const salts_simd_desc *, const salts_simd_desc *, cmeta_simd_convert_op, cmeta_simd_lane_policy, cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_select, bool, (const salts_simd_desc *, cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_i32x4_splat, void, (cmeta_v128 *, int32_t)) \
    X(cmeta_simd_i32x4_add, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_i32x4_sub, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_i32x4_mul, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_i32x4_eq, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_f32x4_add, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_f32x4_mul, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *)) \
    X(cmeta_simd_v128_bitselect, void, (cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *, const cmeta_v128 *))

#define DECLARE_API(name, result, arguments) \
    _Static_assert(_Generic(&name, result (*) arguments: 1, default: 0), \
                   #name " public C signature"); \
    static result (*const volatile linked_##name) arguments = &name;
SIMD_EXECUTION_APIS(DECLARE_API)
#undef DECLARE_API

static int check_exported_descriptors(void) {
    struct expected_shape {
        const salts_simd_desc *desc;
        salts_simd_lane_kind kind;
        uint16_t bits;
        bool mask;
    };
    const struct expected_shape shapes[] = {
        {&salts_simd_i8x16, SALTS_SIMD_LANE_I8, 8u, false},
        {&salts_simd_u8x16, SALTS_SIMD_LANE_U8, 8u, false},
        {&salts_simd_i16x8, SALTS_SIMD_LANE_I16, 16u, false},
        {&salts_simd_u16x8, SALTS_SIMD_LANE_U16, 16u, false},
        {&salts_simd_i32x4, SALTS_SIMD_LANE_I32, 32u, false},
        {&salts_simd_u32x4, SALTS_SIMD_LANE_U32, 32u, false},
        {&salts_simd_i64x2, SALTS_SIMD_LANE_I64, 64u, false},
        {&salts_simd_u64x2, SALTS_SIMD_LANE_U64, 64u, false},
        {&salts_simd_f32x4, SALTS_SIMD_LANE_F32, 32u, false},
        {&salts_simd_f64x2, SALTS_SIMD_LANE_F64, 64u, false},
        {&salts_simd_b8x16, SALTS_SIMD_LANE_BOOL, 8u, true},
        {&salts_simd_b16x8, SALTS_SIMD_LANE_BOOL, 16u, true},
        {&salts_simd_b32x4, SALTS_SIMD_LANE_BOOL, 32u, true},
        {&salts_simd_b64x2, SALTS_SIMD_LANE_BOOL, 64u, true}
    };
    size_t i;
    _Static_assert(sizeof(shapes) / sizeof(shapes[0]) == 14u, "all DLL data exports");
    for (i = 0u; i < sizeof(shapes) / sizeof(shapes[0]); ++i) {
        CHECK(salts_simd_desc_valid(shapes[i].desc));
        CHECK(shapes[i].desc->lane_kind == shapes[i].kind);
        CHECK(shapes[i].desc->lane_bits == shapes[i].bits);
        CHECK(shapes[i].desc->lane_count == 128u / shapes[i].bits);
        CHECK(shapes[i].desc->is_mask == shapes[i].mask);
    }
    CHECK(!salts_simd_desc_valid(NULL));
    return 0;
}

static int check_numeric_and_masks(void) {
    const uint32_t a[4] = {UINT32_MAX, 0u, UINT32_C(0x80000000), 7u};
    const uint32_t b[4] = {1u, UINT32_MAX, UINT32_C(0x80000000), 9u};
    const uint32_t sum[4] = {0u, UINT32_MAX, 0u, 16u};
    const uint32_t cmp_a[4] = {UINT32_MAX, 1u, 2u, UINT32_C(0x80000000)};
    const uint32_t cmp_b[4] = {1u, 2u, 2u, UINT32_MAX};
    const uint32_t expected_mask[4] = {0u, UINT32_MAX, 0u, UINT32_MAX};
    const float fa[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    const float fb[4] = {0.5f, 2.0f, 1.5f, 2.0f};
    const float product[4] = {0.5f, 4.0f, 4.5f, 8.0f};
    uint32_t words[4] = {0};
    float floats[4] = {0};
    cmeta_v128 left = {{0}}, right = {{0}}, out = {{0}};

    cmeta_simd_v128_load(&left, a);
    cmeta_simd_v128_load(&right, b);
    CHECK(cmeta_simd_binary(&salts_simd_u32x4, SALTS_SIMD_BINARY_ADD,
                            &left, &left, &right));
    cmeta_simd_v128_store(words, &left);
    CHECK(memcmp(words, sum, sizeof(words)) == 0);

    cmeta_simd_v128_load(&left, cmp_a);
    cmeta_simd_v128_load(&right, cmp_b);
    CHECK(cmeta_simd_compare(&salts_simd_u32x4, SALTS_SIMD_COMPARE_LT,
                             &out, &left, &right));
    cmeta_simd_v128_store(words, &out);
    CHECK(memcmp(words, expected_mask, sizeof(words)) == 0);

    cmeta_simd_v128_load(&left, fa);
    cmeta_simd_v128_load(&right, fb);
    cmeta_simd_f32x4_mul(&out, &left, &right);
    cmeta_simd_v128_store(floats, &out);
    CHECK(memcmp(floats, product, sizeof(floats)) == 0);
    return 0;
}

static int check_rejected_operations(void) {
    cmeta_v128 value = {{0}}, out;
    unsigned char before[16];
    memset(out.bytes, 0x5a, sizeof(out.bytes));
    memcpy(before, out.bytes, sizeof(before));
    CHECK(!cmeta_simd_binary(&salts_simd_i32x4, SALTS_SIMD_BINARY_DIV,
                             &out, &value, &value));
    CHECK(memcmp(before, out.bytes, sizeof(before)) == 0);
    CHECK(!cmeta_simd_compare(&salts_simd_u64x2, SALTS_SIMD_COMPARE_LT,
                              &out, &value, &value));
    CHECK(memcmp(before, out.bytes, sizeof(before)) == 0);
    CHECK(!cmeta_simd_binary(NULL, SALTS_SIMD_BINARY_ADD,
                             &out, &value, &value));
    CHECK(memcmp(before, out.bytes, sizeof(before)) == 0);
    CHECK(!cmeta_simd_unary(&salts_simd_i32x4, SALTS_SIMD_UNARY_ABS,
                            &out, NULL));
    CHECK(memcmp(before, out.bytes, sizeof(before)) == 0);
    CHECK(!cmeta_simd_load_zero(16u, &out, value.bytes));
    CHECK(memcmp(before, out.bytes, sizeof(before)) == 0);
    return 0;
}

static int check_byte_boundaries_and_alias(void) {
    struct guarded_value {
        unsigned char before[16];
        cmeta_v128 value;
        unsigned char after[16];
    } guarded;
    unsigned char source[18], destination[18];
    size_t i;
    memset(&guarded, 0xa5, sizeof(guarded));
    memset(destination, 0x6b, sizeof(destination));
    for (i = 0u; i < sizeof(source); ++i) source[i] = (unsigned char)(i * 17u);
    source[4] = 0u;

    /* Unaligned byte buffers need no private vector type or extra owner. */
    cmeta_simd_v128_load(&guarded.value, source + 1);
    CHECK(memcmp(guarded.value.bytes, source + 1, 16u) == 0);
    cmeta_simd_v128_load(&guarded.value, guarded.value.bytes);
    cmeta_simd_v128_store(guarded.value.bytes, &guarded.value);
    cmeta_simd_v128_store(destination + 1, &guarded.value);
    CHECK(memcmp(destination + 1, source + 1, 16u) == 0);
    CHECK(destination[0] == 0x6bu && destination[17] == 0x6bu);
    CHECK(cmeta_simd_unary(&salts_simd_u8x16, SALTS_SIMD_UNARY_NOT,
                           &guarded.value, &guarded.value));
    for (i = 0u; i < 16u; ++i) {
        CHECK(guarded.before[i] == 0xa5u && guarded.after[i] == 0xa5u);
        CHECK(guarded.value.bytes[i] == (unsigned char)(source[i + 1u] ^ 0xffu));
    }
    return 0;
}

int main(void) {
#define CHECK_LINK(name, result, arguments) CHECK(linked_##name != NULL);
    SIMD_EXECUTION_APIS(CHECK_LINK)
#undef CHECK_LINK
    if (check_exported_descriptors() || check_numeric_and_masks() ||
        check_rejected_operations() || check_byte_boundaries_and_alias())
        return 1;
    puts("PASS installed SIMD C11: 32 functions, 14 descriptors, numeric and byte contracts");
    return 0;
}

#undef SIMD_EXECUTION_APIS
#undef CHECK
